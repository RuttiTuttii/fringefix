#define COBJMACROS
#include <windows.h>
#include <string.h>

#include "gfx.h"

void gfx_upload(ff_gfx *g, const ff_config *c)
{
    D3D11_MAPPED_SUBRESOURCE m;
    float v[8];

    v[0] = c->shift[0][0];
    v[1] = c->shift[0][1];
    v[2] = c->shift[1][0];
    v[3] = c->shift[1][1];
    v[4] = c->shift[2][0];
    v[5] = c->shift[2][1];
    v[6] = c->sharpen;
    v[7] = g->hdr ? 1.0f : 0.0f;
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
    ID3D11DeviceContext_PSSetShaderResources(g->ctx, 0, 1, &g->srv);
    ID3D11DeviceContext_PSSetSamplers(g->ctx, 0, 1, &g->smp);
    ID3D11DeviceContext_PSSetConstantBuffers(g->ctx, 0, 1, &g->cb);
    ID3D11DeviceContext_Draw(g->ctx, 3, 0);
    IDXGISwapChain1_Present(g->sc, 1, 0);

    /* окно показываем только после первой готовой картинки, иначе экран на миг станет чёрным */
    if (!g->shown) {
        ShowWindow(g->hwnd, SW_SHOWNOACTIVATE);
        g->shown = 1;
    }
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
        /* LastPresentTime == 0: обновился только курсор, гонять шейдер незачем */
        if (info.LastPresentTime.QuadPart != 0) {
            ID3D11Texture2D *tex = NULL;

            if (SUCCEEDED(IDXGIResource_QueryInterface(res, &IID_ID3D11Texture2D, (void **)&tex))) {
                ID3D11DeviceContext_CopyResource(g->ctx, (ID3D11Resource *)g->frame,
                                                 (ID3D11Resource *)tex);
                ID3D11Texture2D_Release(tex);
                g->have_frame = 1;
                force = 1;
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
