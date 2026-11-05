#define COBJMACROS
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "gfx.h"

/* перерисовка по метаданным нужна, если сдвинулось окно или изменилась малая область:
 * доля экрана 1/50 отделяет курсор, каретку и текст от полноэкранного эха оверлея */
#define SMALL_AREA_DIV 50

void gfx_upload(ff_gfx *g, const ff_config *c)
{
    D3D11_MAPPED_SUBRESOURCE m;
    float v[12];

    v[0] = c->shift[0][0];
    v[1] = c->shift[0][1];
    v[2] = c->shift[1][0];
    v[3] = c->shift[1][1];
    v[4] = c->shift[2][0];
    v[5] = c->shift[2][1];
    v[6] = c->sharpen;
    v[7] = g->hdr ? 1.0f : 0.0f;
    v[8] = g->light ? 1.0f : 0.0f;
    v[9] = v[10] = v[11] = 0.0f;
    if (SUCCEEDED(ID3D11DeviceContext_Map(g->ctx, (ID3D11Resource *)g->cb, 0,
                                          D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        memcpy(m.pData, v, sizeof v);
        ID3D11DeviceContext_Unmap(g->ctx, (ID3D11Resource *)g->cb, 0);
    }
}

static void draw(ff_gfx *g)
{
    D3D11_VIEWPORT vp;

    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = (float)g->w;
    vp.Height = (float)g->h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    ID3D11DeviceContext_OMSetRenderTargets(g->ctx, 1, &g->rtv, NULL);
    ID3D11DeviceContext_RSSetViewports(g->ctx, 1, &vp);
    ID3D11DeviceContext_IASetPrimitiveTopology(g->ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(g->ctx, g->vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(g->ctx, g->ps, NULL, 0);
    ID3D11DeviceContext_PSSetShaderResources(g->ctx, 0, 1, &g->srv[g->cur]);
    ID3D11DeviceContext_PSSetSamplers(g->ctx, 0, 1, &g->smp);
    ID3D11DeviceContext_PSSetConstantBuffers(g->ctx, 0, 1, &g->cb);
    ID3D11DeviceContext_Draw(g->ctx, 3, 0);
    IDXGISwapChain1_Present(g->sc, 1, 0);
    g->drawn++;

    /* окно показываем только после первой готовой картинки, иначе экран на миг станет чёрным */
    if (!g->shown) {
        ShowWindow(g->hwnd, SW_SHOWNOACTIVATE);
        g->shown = 1;
    }
}

/* читает dirty и move прямоугольники текущего кадра; 0 при нехватке памяти */
static int read_rects(ff_gfx *g, const DXGI_OUTDUPL_FRAME_INFO *info, UINT *nmove, UINT *ndirty,
                      long long *area)
{
    UINT total = info->TotalMetadataBufferSize;
    UINT move_used = 0;
    UINT dirty_used = 0;
    const RECT *rects;
    UINT i;

    *nmove = 0;
    *ndirty = 0;
    *area = 0;
    if (total == 0) {
        return 1;
    }
    if (total > g->meta_cap) {
        unsigned char *grown = realloc(g->meta, total);

        if (!grown) {
            return 0;
        }
        g->meta = grown;
        g->meta_cap = total;
    }
    IDXGIOutputDuplication_GetFrameMoveRects(g->dup, total, (DXGI_OUTDUPL_MOVE_RECT *)g->meta,
                                             &move_used);
    IDXGIOutputDuplication_GetFrameDirtyRects(g->dup, total - move_used,
                                              (RECT *)(g->meta + move_used), &dirty_used);
    *nmove = move_used / sizeof(DXGI_OUTDUPL_MOVE_RECT);
    *ndirty = dirty_used / sizeof(RECT);
    rects = (const RECT *)(g->meta + move_used);
    for (i = 0; i < *ndirty; i++) {
        *area += (long long)(rects[i].right - rects[i].left) * (rects[i].bottom - rects[i].top);
    }
    return 1;
}

/* настоящее ли изменение экрана; кадр уже скопирован в g->frame[g->cur], предыдущий лежит во второй копии.
 * сам оверлей тоже будит захват: DWM пересобирает рабочий стол после каждого Present, и без этой
 * проверки каждая отрисовка рождала бы следующую на полной частоте обновления.
 * браузеры отдают DWM окно целиком, поэтому набор текста приходит с большой областью и должен
 * сравниваться по всем пикселям: выборочное сравнение пропускало такие кадры и экран замирал */
static int frame_changed(ff_gfx *g, const DXGI_OUTDUPL_FRAME_INFO *info)
{
    UINT nmove;
    UINT ndirty;
    long long area;
    int changed;

    if (!read_rects(g, info, &nmove, &ndirty, &area)) {
        return 1;
    }
    if (nmove == 0 && ndirty == 0 && info->LastPresentTime.QuadPart == 0) {
        return 0;
    }
    /* прокрутка и малые области настоящие без сравнения */
    if (nmove > 0 || (ndirty > 0 && area < (long long)g->w * g->h / SMALL_AREA_DIV)) {
        return 1;
    }
    if (gfx_sig_diff(g, &changed) != 0) {
        return 1;
    }
    if (!changed) {
        g->echo_skipped++;
    }
    return changed;
}

int gfx_pump(ff_gfx *g, UINT timeout_ms, int force)
{
    DXGI_OUTDUPL_FRAME_INFO info;
    IDXGIResource *res = NULL;
    HRESULT hr = IDXGIOutputDuplication_AcquireNextFrame(g->dup, timeout_ms, &info, &res);

    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        /* экран не менялся, перерисовка только по force */
    } else if (FAILED(hr)) {
        return 0;
    } else {
        /* кадр только с курсором не копируем и не рисуем */
        if (info.LastPresentTime.QuadPart != 0 || info.TotalMetadataBufferSize > 0) {
            ID3D11Texture2D *tex = NULL;

            if (SUCCEEDED(IDXGIResource_QueryInterface(res, &IID_ID3D11Texture2D, (void **)&tex))) {
                int first = !g->have_frame;

                /* новый кадр ложится во вторую копию, прежний остаётся для сравнения */
                g->cur = !g->cur;
                ID3D11DeviceContext_CopyResource(g->ctx, (ID3D11Resource *)g->frame[g->cur],
                                                 (ID3D11Resource *)tex);
                ID3D11Texture2D_Release(tex);
                g->have_frame = 1;
                if (frame_changed(g, &info) || first) {
                    force = 1;
                }
            }
        }
        IDXGIResource_Release(res);
        IDXGIOutputDuplication_ReleaseFrame(g->dup);
    }
    if (force && g->have_frame) {
        draw(g);
    }
    return 1;
}

void gfx_reassert(ff_gfx *g)
{
    if (!g->shown) {
        return;
    }
    if (!IsWindowVisible(g->hwnd)) {
        ShowWindow(g->hwnd, SW_SHOWNOACTIVATE);
    }
    SetWindowPos(g->hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW);
}
