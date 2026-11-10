#define COBJMACROS
#include <windows.h>
#include <string.h>

#include "bench.h"
#include "com.h"
#include "gfx.h"

/* слот, потерявший результат (потеря устройства, сброс), через столько мс освобождается без записи */
#define SLOT_TTL_MS 2000.0

static void slot_release(ff_prof_slot *s)
{
    int i;

    FF_REL(s->disjoint);
    for (i = 0; i < 4; i++) {
        FF_REL(s->ts[i]);
    }
    s->state = 0;
}

void gfx_prof_close(ff_gfx *g)
{
    int i;

    for (i = 0; i < PROF_SLOTS; i++) {
        slot_release(&g->prof.slot[i]);
    }
    memset(&g->prof, 0, sizeof g->prof);
    g->prof.cur = -1;
}

/* запросы создаются заранее: замер не должен аллоцировать посреди кадра. при сбое замеры просто выключены */
void gfx_prof_open(ff_gfx *g)
{
    D3D11_QUERY_DESC qd;
    int i;
    int k;

    memset(&g->prof, 0, sizeof g->prof);
    g->prof.cur = -1;
    for (i = 0; i < PROF_SLOTS; i++) {
        ff_prof_slot *s = &g->prof.slot[i];

        memset(&qd, 0, sizeof qd);
        qd.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        if (FAILED(ID3D11Device_CreateQuery(g->dev, &qd, &s->disjoint))) {
            gfx_prof_close(g);
            return;
        }
        qd.Query = D3D11_QUERY_TIMESTAMP;
        for (k = 0; k < 4; k++) {
            if (FAILED(ID3D11Device_CreateQuery(g->dev, &qd, &s->ts[k]))) {
                gfx_prof_close(g);
                return;
            }
        }
    }
    g->prof.open = 1;
}

void gfx_prof_begin(ff_gfx *g, LONGLONG last_present_qpc, UINT accumulated)
{
    ff_prof *p = &g->prof;
    ff_prof_slot *s;
    LARGE_INTEGER now;

    p->cur = -1;
    if (!p->open || !g->diag || !bench_wanted()) {
        return;
    }
    s = &p->slot[p->next];
    if (s->state != 0) {
        return;
    }
    QueryPerformanceCounter(&now);
    s->acq = now.QuadPart;
    s->t_ms = bench_now_ms();
    s->age_ms = -1.0f;
    if (last_present_qpc > 0 && now.QuadPart >= last_present_qpc) {
        s->age_ms = (float)bench_ticks_to_ms(now.QuadPart - last_present_qpc);
    }
    s->pre_present = 0;
    s->present_ms = -1.0f;
    s->cpu_ms = -1.0f;
    s->marks = 1;
    s->state = 1;
    s->accum = (unsigned short)(accumulated > 65535u ? 65535u : accumulated);
    s->engine = (unsigned char)gfx_engine_effective(g);
    s->light = (unsigned char)(g->light != 0);
    ID3D11DeviceContext_Begin(g->ctx, (ID3D11Asynchronous *)s->disjoint);
    ID3D11DeviceContext_End(g->ctx, (ID3D11Asynchronous *)s->ts[0]);
    p->cur = p->next;
    p->next = (p->next + 1) % PROF_SLOTS;
}

void gfx_prof_mark(ff_gfx *g, int index)
{
    ff_prof_slot *s;

    if (g->prof.cur < 0 || index < 1 || index > 3) {
        return;
    }
    s = &g->prof.slot[g->prof.cur];
    ID3D11DeviceContext_End(g->ctx, (ID3D11Asynchronous *)s->ts[index]);
    s->marks |= 1u << index;
}

void gfx_prof_present(ff_gfx *g, LONGLONG before, LONGLONG after)
{
    ff_prof_slot *s;

    if (g->prof.cur < 0) {
        return;
    }
    s = &g->prof.slot[g->prof.cur];
    s->pre_present = before;
    s->present_ms = (float)bench_ticks_to_ms(after - before);
}

void gfx_prof_end(ff_gfx *g, int drew, int echo)
{
    ff_prof_slot *s;
    LARGE_INTEGER now;

    if (g->prof.cur < 0) {
        return;
    }
    s = &g->prof.slot[g->prof.cur];
    QueryPerformanceCounter(&now);
    /* процессорное время идёт до вызова Present, если он был: само ожидание синхронизации отдельная колонка */
    s->cpu_ms = (float)bench_ticks_to_ms((s->pre_present ? s->pre_present : now.QuadPart) - s->acq);
    s->drew = (unsigned char)(drew != 0);
    s->echo = (unsigned char)(echo != 0);
    ID3D11DeviceContext_End(g->ctx, (ID3D11Asynchronous *)s->disjoint);
    s->state = 2;
    g->prof.cur = -1;
}

/* готовые циклы переводятся в миллисекунды и уходят сборщику; недоступный пока результат ждёт следующего вызова */
void gfx_prof_collect(ff_gfx *g)
{
    ff_prof *p = &g->prof;
    int n;

    if (!p->open) {
        return;
    }
    for (n = 0; n < PROF_SLOTS; n++) {
        ff_prof_slot *s = &p->slot[(p->next + n) % PROF_SLOTS];
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj;
        UINT64 t[4] = { 0, 0, 0, 0 };
        HRESULT hr;
        ff_sample out;
        int last = 0;
        int k;

        if (s->state != 2) {
            continue;
        }
        hr = ID3D11DeviceContext_GetData(g->ctx, (ID3D11Asynchronous *)s->disjoint, &dj, sizeof dj,
                                         D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if (hr == S_FALSE) {
            if (bench_now_ms() - s->t_ms > SLOT_TTL_MS) {
                s->state = 0;
            }
            continue;
        }
        if (FAILED(hr) || dj.Disjoint || dj.Frequency == 0) {
            s->state = 0;
            continue;
        }
        for (k = 0; k < 4; k++) {
            if ((s->marks & (1u << k)) && ID3D11DeviceContext_GetData(g->ctx, (ID3D11Asynchronous *)s->ts[k],
                                                                       &t[k], sizeof t[k], 0) != S_OK) {
                s->marks &= ~(1u << k);
            }
        }
        memset(&out, 0, sizeof out);
        out.t_ms = s->t_ms;
        out.copy_ms = out.diff_ms = out.draw_ms = -1.0f;
        /* этап считается между двумя соседними поставленными метками */
        for (k = 1; k < 4; k++) {
            float ms;

            if (!(s->marks & (1u << k))) {
                continue;
            }
            ms = (float)((double)(t[k] - t[last]) * 1000.0 / (double)dj.Frequency);
            if (k == 1) {
                out.copy_ms = ms;
            } else if (k == 2) {
                out.diff_ms = ms;
            } else {
                out.draw_ms = ms;
            }
            last = k;
        }
        out.cpu_ms = s->cpu_ms;
        out.present_ms = s->present_ms;
        out.age_ms = s->age_ms;
        out.accum = s->accum;
        out.engine = s->engine;
        out.light = s->light;
        out.drew = s->drew;
        out.echo = s->echo;
        bench_push(&out);
        s->state = 0;
    }
}
