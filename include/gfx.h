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
    /* счётчики для статистики; поток захвата забирает их и обнуляет */
    unsigned drawn;
    unsigned echo_skipped;
    UINT w;
    UINT h;
    DXGI_FORMAT fmt;
    HWND hwnd;
    ID3D11Device *dev;
    ID3D11DeviceContext *ctx;
    IDXGIOutputDuplication *dup;
    IDXGISwapChain1 *sc;
    ID3D11RenderTargetView *rtv;
    /* две копии кадра: cur это последний полученный, вторая предыдущий, они нужны для точного сравнения */
    ID3D11Texture2D *frame[2];
    ID3D11ShaderResourceView *srv[2];
    int cur;
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    ID3D11Buffer *cb;
    ID3D11SamplerState *smp;
    /* сравнение кадров на GPU: по флагу на блок 16x16, чтобы отличить настоящее изменение от эха собственного оверлея */
    ID3D11PixelShader *sig_ps;
    ID3D11Texture2D *sig_tex;
    ID3D11RenderTargetView *sig_rtv;
    ID3D11Texture2D *sig_stage;
    UINT sig_w;
    UINT sig_h;
    /* буфер метаданных dirty и move прямоугольников */
    unsigned char *meta;
    size_t meta_cap;
} ff_gfx;

/* поднимает всё сразу или ничего: монитор и приоритет GPU берутся из cfg; при ошибке кладёт текст в err
 * (GFX_ERR_LEN байт) и возвращает -1 */
int gfx_init(ff_gfx *g, const ff_config *cfg, char *err);

/* пересоздаёт копии кадра и цепочку под другой формат поверхности; 0 при успехе, -1 при сбое.
 * зовётся из gfx_pump, когда формат полученной текстуры не совпал с ожидаемым */
int gfx_reformat(ff_gfx *g, DXGI_FORMAT fmt);

/* безопасен для нулевой структуры и для повторного вызова */
void gfx_shutdown(ff_gfx *g);

/* заливает сдвиги и резкость в константный буфер шейдера */
void gfx_upload(ff_gfx *g, const ff_config *c);

/* ждёт кадр до timeout_ms и рисует, если экран изменился или force; 0 при потере захвата */
int gfx_pump(ff_gfx *g, UINT timeout_ms, int force);

/* возвращает окно в самый верх: полноэкранные приложения и блокировка экрана сбивают z-порядок */
void gfx_reassert(ff_gfx *g);

/* ресурсы сравнения кадров и само сравнение: шейдер пишет флаг на каждый блок 16x16, процессор читает флаги.
 * в *changed попадает 1, если текущий и предыдущий кадр различаются хоть в одном пикселе; -1 при сбое чтения */
int gfx_sig_open(ff_gfx *g, char *err);
int gfx_sig_diff(ff_gfx *g, int *changed);

#endif
