#ifndef FRINGEFIX_GFX_H
#define FRINGEFIX_GFX_H

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>

#include "config.h"

#define GFX_ERR_LEN 160
#define PROF_SLOTS 8

/* один цикл обработки кадра в замере: таймеры видеокарты ставятся на копию, сравнение и отрисовку,
 * результат читается позже без ожидания, поэтому слотов несколько */
typedef struct {
    ID3D11Query *disjoint;
    ID3D11Query *ts[4];
    /* 0 свободен, 1 цикл идёт, 2 ждёт результата от видеокарты */
    int state;
    unsigned marks;
    double t_ms;
    LONGLONG acq;
    LONGLONG pre_present;
    float present_ms;
    float age_ms;
    float cpu_ms;
    unsigned short accum;
    unsigned char engine;
    unsigned char light;
    unsigned char drew;
    unsigned char echo;
} ff_prof_slot;

typedef struct {
    ff_prof_slot slot[PROF_SLOTS];
    /* следующий слот по кругу и слот открытого цикла, -1 если цикл не меряется */
    int next;
    int cur;
    int open;
} ff_prof;

/* один монитор: захват рабочего стола, копия кадра, шейдер и окно-оверлей поверх всего.
 * все вызовы делаются из одного потока, он же владеет окном */
typedef struct {
    int alive;
    int hdr;
    int shown;
    int have_frame;
    /* облегчённого режима в программе больше нет: поле всегда ноль, оно осталось в шейдере и в таблицах замеров */
    int light;
    /* монитор показан в окне настроек: только его кадры идут в замеры и панель диагностики, иначе мониторы
     * перемешали бы в них свои циклы */
    int diag;
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
    /* движок обработки (бета): заказан в настройках; cs_ok значит, что compute-шейдер сравнения создан,
     * ps_k что готов пиксельный шейдер ядра 3x3; без них действует классический путь */
    int engine;
    int cs_ok;
    ID3D11PixelShader *ps_k;
    ID3D11ComputeShader *cs_diff;
    ID3D11UnorderedAccessView *sig_uav;
    char note[GFX_ERR_LEN];
    char adapter[96];
    ff_prof prof;
    /* способ вывода: заказанный в настройках и действующий (tearing вывод без ожидания синхронизации; 0, если система
     * не умеет или не просили), приоритет GPU заказанный и тот, что вернула система после установки */
    int present_req;
    int tearing;
    int prio_req;
    int prio_eff;
    char pnote[GFX_ERR_LEN];
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

/* движок, который действует на самом деле: заказанный, но не выше того, что поддерживает устройство */
int gfx_engine_effective(const ff_gfx *g);

/* безопасен для нулевой структуры и для повторного вызова */
void gfx_shutdown(ff_gfx *g);

/* ставит приоритет GPU-потока устройства и читает обратно, что принято: приоритет выше нуля система может не дать */
void gfx_set_priority(ff_gfx *g, int priority);

/* заливает сдвиги и резкость монитора v в константный буфер шейдера; движок и вывод берутся из c.
 * движок 2 уступает движку 1 там, где окна ядра трёх каналов не сходятся в одну клетку */
void gfx_upload(ff_gfx *g, const ff_config *c, const ff_view *v);

/* ждёт кадр до timeout_ms и рисует, если экран изменился или force; 0 при потере захвата */
int gfx_pump(ff_gfx *g, UINT timeout_ms, int force);

/* возвращает окно в самый верх: полноэкранные приложения и блокировка экрана сбивают z-порядок */
void gfx_reassert(ff_gfx *g);

/* ресурсы сравнения кадров и само сравнение: шейдер пишет флаг на каждый блок 16x16, процессор читает флаги.
 * в *changed попадает 1, если текущий и предыдущий кадр различаются хоть в одном пикселе; -1 при сбое чтения */
int gfx_sig_open(ff_gfx *g, char *err);
int gfx_sig_diff(ff_gfx *g, int *changed);

/* замеры цикла кадра для окна диагностики (bench.h); включены только пока окно открыто. begin ставит первую метку
 * до копии кадра, mark 1..3 после копии, сравнения и отрисовки, end закрывает цикл; collect забирает готовые */
void gfx_prof_open(ff_gfx *g);
void gfx_prof_close(ff_gfx *g);
void gfx_prof_begin(ff_gfx *g, LONGLONG last_present_qpc, UINT accumulated);
void gfx_prof_mark(ff_gfx *g, int index);
void gfx_prof_present(ff_gfx *g, LONGLONG before, LONGLONG after);
void gfx_prof_end(ff_gfx *g, int drew, int echo);
void gfx_prof_collect(ff_gfx *g);

#endif
