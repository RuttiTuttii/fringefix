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
    /* облегчённый режим: один сдвиг без лапласиана и резкости, чтобы шейдер почти ничего не стоил */
    int light;
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
    /* подпись кадра: разреженный хеш на GPU, чтобы отличить настоящее изменение от эха собственного оверлея */
    ID3D11PixelShader *sig_ps;
    ID3D11Texture2D *sig_tex;
    ID3D11RenderTargetView *sig_rtv;
    ID3D11Texture2D *sig_stage;
    unsigned sig;
    int sig_valid;
    /* буфер метаданных dirty и move прямоугольников */
    unsigned char *meta;
    size_t meta_cap;
} ff_gfx;

/* поднимает всё сразу или ничего: монитор и приоритет GPU берутся из cfg; при ошибке кладёт текст в err
 * (GFX_ERR_LEN байт) и возвращает -1 */
int gfx_init(ff_gfx *g, const ff_config *cfg, char *err);

/* безопасен для нулевой структуры и для повторного вызова */
void gfx_shutdown(ff_gfx *g);

/* заливает сдвиги и резкость в константный буфер шейдера */
void gfx_upload(ff_gfx *g, const ff_config *c);

/* ждёт кадр до timeout_ms и рисует, если экран изменился или force; 0 при потере захвата */
int gfx_pump(ff_gfx *g, UINT timeout_ms, int force);

/* возвращает окно в самый верх: полноэкранные приложения и блокировка экрана сбивают z-порядок */
void gfx_reassert(ff_gfx *g);

/* ресурсы подписи кадра и её чтение: рендер хеша в текстуру 1x1 и считывание четырёх байт */
int gfx_sig_open(ff_gfx *g, char *err);
int gfx_sig_read(ff_gfx *g, unsigned *out);

#endif
