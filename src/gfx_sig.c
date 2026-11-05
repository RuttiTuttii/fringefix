#define COBJMACROS
#include <windows.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "com.h"
#include "shader.h"

int gfx_sig_open(ff_gfx *g, char *err)
{
    HRESULT hr;
    int rc = -1;
    ID3DBlob *blob = NULL;
    D3D11_TEXTURE2D_DESC td;

    hr = ff_shader_compile("psig", "ps_5_0", &blob, err, GFX_ERR_LEN);
    if (FAILED(hr)) {
        goto done;
    }
    TRY(ID3D11Device_CreatePixelShader(g->dev, ID3D10Blob_GetBufferPointer(blob),
                                       ID3D10Blob_GetBufferSize(blob), NULL, &g->sig_ps),
        "CreatePixelShader sig");

    /* результат сравнения: по пикселю на блок 16x16 для рендера и такая же текстура для чтения процессором */
    g->sig_w = (g->w + 15) / 16;
    g->sig_h = (g->h + 15) / 16;
    memset(&td, 0, sizeof td);
    td.Width = g->sig_w;
    td.Height = g->sig_h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R32_UINT;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;
    TRY(ID3D11Device_CreateTexture2D(g->dev, &td, NULL, &g->sig_tex), "CreateTexture2D sig");
    TRY(ID3D11Device_CreateRenderTargetView(g->dev, (ID3D11Resource *)g->sig_tex, NULL,
                                            &g->sig_rtv), "CreateRenderTargetView sig");
    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    TRY(ID3D11Device_CreateTexture2D(g->dev, &td, NULL, &g->sig_stage), "CreateTexture2D stage");
    rc = 0;

done:
    FF_REL(blob);
    return rc;
}

int gfx_sig_diff(ff_gfx *g, int *changed)
{
    D3D11_VIEWPORT vp = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    D3D11_MAPPED_SUBRESOURCE m;
    ID3D11ShaderResourceView *srvs[2];
    ID3D11ShaderResourceView *none[2] = { NULL, NULL };
    UINT x;
    UINT y;

    vp.Width = (float)g->sig_w;
    vp.Height = (float)g->sig_h;
    /* t0 текущий кадр, t1 предыдущий */
    srvs[0] = g->srv[g->cur];
    srvs[1] = g->srv[!g->cur];
    ID3D11DeviceContext_OMSetRenderTargets(g->ctx, 1, &g->sig_rtv, NULL);
    ID3D11DeviceContext_RSSetViewports(g->ctx, 1, &vp);
    ID3D11DeviceContext_IASetPrimitiveTopology(g->ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(g->ctx, g->vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(g->ctx, g->sig_ps, NULL, 0);
    ID3D11DeviceContext_PSSetShaderResources(g->ctx, 0, 2, srvs);
    ID3D11DeviceContext_Draw(g->ctx, 3, 0);
    /* копии кадра сразу отвязываются: следующий кадр ляжет в одну из них */
    ID3D11DeviceContext_PSSetShaderResources(g->ctx, 0, 2, none);
    ID3D11DeviceContext_CopyResource(g->ctx, (ID3D11Resource *)g->sig_stage,
                                     (ID3D11Resource *)g->sig_tex);
    if (FAILED(ID3D11DeviceContext_Map(g->ctx, (ID3D11Resource *)g->sig_stage, 0, D3D11_MAP_READ, 0,
                                       &m))) {
        return -1;
    }
    *changed = 0;
    for (y = 0; y < g->sig_h && !*changed; y++) {
        const unsigned *row = (const unsigned *)((const unsigned char *)m.pData + (size_t)y * m.RowPitch);

        for (x = 0; x < g->sig_w; x++) {
            if (row[x]) {
                *changed = 1;
                break;
            }
        }
    }
    ID3D11DeviceContext_Unmap(g->ctx, (ID3D11Resource *)g->sig_stage, 0);
    return 0;
}
