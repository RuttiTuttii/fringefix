#ifndef FRINGEFIX_GFX_H
#define FRINGEFIX_GFX_H

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>

#include "config.h"

#define GFX_ERR_LEN 160

/* один монитор: захват рабочего стола, копия кадра, шейдер и окно-оверлей поверх всего.
 * все вызовы делаются из одного потока, он же владеет окном */
typedef struct {
    int alive;
    int hdr;
    int shown;
    int have_frame;
    UINT w;
    UINT h;
    DXGI_FORMAT fmt;
    HWND hwnd;
    ID3D11Device *dev;
    ID3D11DeviceContext *ctx;
    IDXGIOutputDuplication *dup;
    IDXGISwapChain1 *sc;
    ID3D11RenderTargetView *rtv;
    ID3D11Texture2D *frame;
    ID3D11ShaderResourceView *srv;
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    ID3D11Buffer *cb;
    ID3D11SamplerState *smp;
} ff_gfx;

/* поднимает всё сразу или ничего; при ошибке кладёт текст в err (GFX_ERR_LEN байт) и возвращает -1 */
int gfx_init(ff_gfx *g, int monitor, char *err);

/* безопасен для нулевой структуры и для повторного вызова */
void gfx_shutdown(ff_gfx *g);

/* заливает сдвиги и резкость в константный буфер шейдера */
void gfx_upload(ff_gfx *g, const ff_config *c);

/* ждёт кадр до timeout_ms и рисует, если экран изменился или force; 0 при потере захвата */
int gfx_pump(ff_gfx *g, UINT timeout_ms, int force);

#endif
