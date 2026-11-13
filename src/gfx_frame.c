#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bench.h"
#include "crash.h"
#include "gfx.h"
#include "kernel.h"

/* перерисовка по метаданным нужна, если сдвинулось окно или изменилась малая область:
 * доля экрана 1/50 отделяет курсор, каретку и текст от полноэкранного эха оверлея */
#define SMALL_AREA_DIV 50

/* действующий движок: заказанный, но не выше того, что собралось на этом устройстве */
int gfx_engine_effective(const ff_gfx *g)
{
    int e = g->engine;

    if (e >= 2 && !g->ps_k) {
        e = 1;
    }
    if (e >= 1 && !g->cs_ok) {
        e = 0;
    }
    return e;
}

/* раскладка совпадает с cbuffer в shader.c: три float4 классического шейдера, затем окна ядра 3x3 */
typedef struct {
    float shift_rg[4];
    float shift_bx[4];
    float mode[4];
    int kbase[4];
    int kbase2[4];
    float kw[9][4];
} cb_data;

typedef char cb_size_must_be_224[sizeof(cb_data) == 224 ? 1 : -1];

/* вектор сдвига в осях панели переводится в оси повёрнутого рабочего стола: панель повёрнута по часовой стрелке
 * на rot * 90 градусов, и содержимое канала уезжает на столько же */
static void rotate_shift(int rot, const float in[2], float out[2])
{
    switch (rot) {
    case 1:
        out[0] = -in[1];
        out[1] = in[0];
        break;
    case 2:
        out[0] = -in[0];
        out[1] = -in[1];
        break;
    case 3:
        out[0] = in[1];
        out[1] = -in[0];
        break;
    default:
        out[0] = in[0];
        out[1] = in[1];
        break;
    }
}

void gfx_upload(ff_gfx *g, const ff_config *c, const ff_view *v)
{
    D3D11_MAPPED_SUBRESOURCE m;
    cb_data d;
    ff_kernel k;
    float flat[6];
    char note[GFX_ERR_LEN];
    int ch;
    int row;

    /* режим А: настройки альбомной ориентации привязаны к субпикселям панели и едут вместе с ней; в режиме Б значения
     * это набор ориентации, и они уже в осях рабочего стола */
    for (ch = 0; ch < 3; ch++) {
        if (c->rot_mode == 0) {
            rotate_shift(g->rot, v->shift[ch], &flat[ch * 2]);
        } else {
            flat[ch * 2] = v->shift[ch][0];
            flat[ch * 2 + 1] = v->shift[ch][1];
        }
    }
    memset(&d, 0, sizeof d);
    d.shift_rg[0] = flat[0];
    d.shift_rg[1] = flat[1];
    d.shift_rg[2] = flat[2];
    d.shift_rg[3] = flat[3];
    d.shift_bx[0] = flat[4];
    d.shift_bx[1] = flat[5];
    d.shift_bx[2] = v->sharpen;
    d.shift_bx[3] = g->hdr ? 1.0f : 0.0f;
    d.mode[0] = g->light ? 1.0f : 0.0f;
    d.mode[1] = (float)g->rot;
    ff_kernel_build(flat, v->sharpen, &k);
    d.kbase[0] = k.base[0][0];
    d.kbase[1] = k.base[0][1];
    d.kbase[2] = k.base[1][0];
    d.kbase[3] = k.base[1][1];
    d.kbase2[0] = k.base[2][0];
    d.kbase2[1] = k.base[2][1];
    d.kbase2[2] = k.shared;
    for (ch = 0; ch < 3; ch++) {
        for (row = 0; row < 3; row++) {
            d.kw[ch * 3 + row][0] = k.w[ch][row * 3];
            d.kw[ch * 3 + row][1] = k.w[ch][row * 3 + 1];
            d.kw[ch * 3 + row][2] = k.w[ch][row * 3 + 2];
        }
    }
    g->engine = c->engine;
    snprintf(note, sizeof note, "%s", g->note);
    /* окна ядра трёх каналов в разных клетках: чтений в шейдере втрое больше, дешевле классический фильтр с сравнением
     * кадров. граница это разброс сдвигов в один пиксель по любой оси */
    if (g->engine >= 2 && !k.shared) {
        g->engine = 1;
        snprintf(note, sizeof note, "channel shifts differ by more than 1 px: engine 1 is used");
    }
    /* приоритет меняется на живом устройстве, способ вывода только пересозданием цепочки (app_reinit) */
    if (c->gpu_priority != g->prio_req) {
        gfx_set_priority(g, c->gpu_priority);
    }
    if (g->diag) {
        bench_set_engine_info(c->engine, gfx_engine_effective(g), g->cs_ok, note);
        bench_set_variant_info(g->present_req, g->tearing, g->prio_req, g->prio_eff, g->pnote);
    }
    if (SUCCEEDED(ID3D11DeviceContext_Map(g->ctx, (ID3D11Resource *)g->cb, 0,
                                          D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        memcpy(m.pData, &d, sizeof d);
        ID3D11DeviceContext_Unmap(g->ctx, (ID3D11Resource *)g->cb, 0);
    }
}

static void draw(ff_gfx *g)
{
    D3D11_VIEWPORT vp;

    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = (float)g->ow;
    vp.Height = (float)g->oh;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    ID3D11DeviceContext_OMSetRenderTargets(g->ctx, 1, &g->rtv, NULL);
    ID3D11DeviceContext_RSSetViewports(g->ctx, 1, &vp);
    ID3D11DeviceContext_IASetPrimitiveTopology(g->ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(g->ctx, g->vs, NULL, 0);
    /* облегчённый режим всегда идёт классическим шейдером: три чтения на пиксель дешевле окна 3x3 */
    ID3D11DeviceContext_PSSetShader(g->ctx, gfx_engine_effective(g) >= 2 && !g->light ? g->ps_k : g->ps,
                                    NULL, 0);
    ID3D11DeviceContext_PSSetShaderResources(g->ctx, 0, 1, &g->srv[g->cur]);
    ID3D11DeviceContext_PSSetSamplers(g->ctx, 0, 1, &g->smp);
    ID3D11DeviceContext_PSSetConstantBuffers(g->ctx, 0, 1, &g->cb);
    ID3D11DeviceContext_Draw(g->ctx, 3, 0);
    gfx_prof_mark(g, 3);
    crash_mark(g->mon, "present");
    {
        LARGE_INTEGER before;
        LARGE_INTEGER after;

        QueryPerformanceCounter(&before);
        IDXGISwapChain1_Present(g->sc, g->tearing ? 0 : 1, g->tearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
        QueryPerformanceCounter(&after);
        gfx_prof_present(g, before.QuadPart, after.QuadPart);
    }
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
    HRESULT hr;
    unsigned echo_before = g->echo_skipped;
    int echo = 0;

    gfx_prof_collect(g);
    crash_mark(g->mon, "acquire");
    hr = IDXGIOutputDuplication_AcquireNextFrame(g->dup, timeout_ms, &info, &res);

    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        /* экран не менялся, перерисовка только по force */
    } else if (FAILED(hr)) {
        return 0;
    } else {
        /* кадр только с курсором не копируем и не рисуем */
        if (info.LastPresentTime.QuadPart != 0 || info.TotalMetadataBufferSize > 0) {
            ID3D11Texture2D *tex = NULL;

            if (SUCCEEDED(IDXGIResource_QueryInterface(res, &IID_ID3D11Texture2D, (void **)&tex))) {
                int first;
                D3D11_TEXTURE2D_DESC td;

                /* формат настоящей поверхности важнее ожидаемого: при несовпадении CopyResource ничего не делает
                 * и оверлей показывал бы чёрный или застывший кадр */
                ID3D11Texture2D_GetDesc(tex, &td);
                if (td.Format != g->fmt && gfx_reformat(g, td.Format) != 0) {
                    ID3D11Texture2D_Release(tex);
                    IDXGIResource_Release(res);
                    IDXGIOutputDuplication_ReleaseFrame(g->dup);
                    return 0;
                }
                first = !g->have_frame;
                crash_mark(g->mon, "copy");

                /* новый кадр ложится во вторую копию, прежний остаётся для сравнения */
                gfx_prof_begin(g, info.LastPresentTime.QuadPart, info.AccumulatedFrames);
                g->cur = !g->cur;
                ID3D11DeviceContext_CopyResource(g->ctx, (ID3D11Resource *)g->frame[g->cur],
                                                 (ID3D11Resource *)tex);
                gfx_prof_mark(g, 1);
                ID3D11Texture2D_Release(tex);
                g->have_frame = 1;
                crash_mark(g->mon, "compare");
                if (frame_changed(g, &info) || first) {
                    force = 1;
                }
                echo = g->echo_skipped != echo_before;
            }
        }
        IDXGIResource_Release(res);
        IDXGIOutputDuplication_ReleaseFrame(g->dup);
    }
    if (force && g->have_frame) {
        crash_mark(g->mon, "draw");
        draw(g);
        gfx_prof_end(g, 1, echo);
    } else {
        gfx_prof_end(g, 0, echo);
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
