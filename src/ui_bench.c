#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "bench.h"
#include "benchstat.h"
#include "fileio.h"
#include "i18n.h"
#include "power.h"
#include "resource.h"
#include "theme.h"
#include "ui.h"
#include "ui_layout.h"
#include "ui_paint.h"
#include "ui_state.h"

#define DIAG_W 820
#define DIAG_H 756
#define TICK_MS 250
#define TIMER_ID 1
/* прогон: выключенный фильтр для базового потребления, затем три движка; сначала разогрев, потом замер */
#define NSTEPS 4
#define WARM_MS 3000.0
#define MEAS_MS 12000.0
#define OFF_WARM_MS 4000.0
#define OFF_MEAS_MS 12000.0
/* окно "живых" графиков и сводки */
#define LIVE_MS 4000.0
#define POWER_SPAN_MS 60000.0

enum { ID_D_ENGINE = 800, ID_D_FORCE, ID_D_RUN, ID_D_SAVE, ID_D_CLOSE };

static const wchar_t CLASS_NAME[] = L"fringefix_diag";
static HWND g_win;
static HWND g_engine;
static HFONT g_mono;
static int g_power_ok;

/* снимки колец для рисования и сводок; живут в главном потоке, поэтому статические */
static ff_sample g_snap[FF_RING_CAP];
static ff_power_sample g_psnap[FF_PRING_CAP];

static const int STEP_ENGINE[NSTEPS] = { FF_ENGINE_OFF, 0, 1, 2 };

/* состояние прогона по всем движкам */
static struct {
    int running;
    int step;
    int measuring;
    double phase_t0;
    double meas_t0;
    int saved_engine;
    int saved_enabled;
    int saved_force;
    ff_run_row rows[NSTEPS];
    int nrows;
    int done;
} T;

static wchar_t g_status[256];

static void set_status(const wchar_t *text)
{
    wcsncpy(g_status, text, 255);
    g_status[255] = L'\0';
}

static HWND child(const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id)
{
    return CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h), g_win,
                           (HMENU)(INT_PTR)id, ui.app->inst, NULL);
}

/* ступени 1, 2, 5 на каждый порядок: подпись верхней границы графика без дробных хвостов */
static double nice_max(double v)
{
    double base = 0.1;

    for (;;) {
        if (v <= base) {
            return base;
        }
        if (v <= 2.0 * base) {
            return 2.0 * base;
        }
        if (v <= 5.0 * base) {
            return 5.0 * base;
        }
        base *= 10.0;
    }
}

typedef struct {
    RECT r;
    double t0;
    double t1;
    double ymax;
} plot;

static int px(const plot *g, double t)
{
    double f = (t - g->t0) / (g->t1 - g->t0);

    f = f < 0.0 ? 0.0 : (f > 1.0 ? 1.0 : f);
    return g->r.left + (int)(f * (double)(g->r.right - g->r.left - 1));
}

static int py(const plot *g, double v)
{
    double f = v / g->ymax;

    f = f < 0.0 ? 0.0 : (f > 1.0 ? 1.0 : f);
    return g->r.bottom - 1 - (int)(f * (double)(g->r.bottom - g->r.top - 1));
}

static void text(HDC dc, int x, int y, const wchar_t *s, COLORREF c)
{
    SetTextColor(dc, c);
    TextOutW(dc, x, y, s, (int)wcslen(s));
}

/* рамка графика с заголовком, подписью верхней границы и нулём */
static void frame(HDC dc, const plot *g, const wchar_t *title, const wchar_t *unit)
{
    const ff_palette *p = theme_get();
    HBRUSH br = CreateSolidBrush(p->field);
    HPEN pen = CreatePen(PS_SOLID, 1, p->border);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_br = SelectObject(dc, br);
    wchar_t top[48];
    int h = S(16);

    Rectangle(dc, g->r.left, g->r.top, g->r.right, g->r.bottom);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_br);
    DeleteObject(pen);
    DeleteObject(br);
    text(dc, g->r.left, g->r.top - h - S(2), title, p->fg);
    _snwprintf(top, 48, L"%g %ls", g->ymax, unit);
    text(dc, g->r.left + S(4), g->r.top + S(2), top, p->muted);
    text(dc, g->r.left + S(4), g->r.bottom - h, L"0", p->muted);
}

static void line_series(HDC dc, const plot *g, COLORREF color, const double *t, const float *v, int n)
{
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old = SelectObject(dc, pen);
    int i;
    int started = 0;

    for (i = 0; i < n; i++) {
        if (v[i] < 0.0f) {
            continue;
        }
        if (!started) {
            MoveToEx(dc, px(g, t[i]), py(g, (double)v[i]), NULL);
            started = 1;
        } else {
            LineTo(dc, px(g, t[i]), py(g, (double)v[i]));
        }
    }
    SelectObject(dc, old);
    DeleteObject(pen);
}

/* цветные подписи рядов под заголовком справа */
static void legend(HDC dc, const plot *g, const wchar_t *const *names, const COLORREF *colors, int n)
{
    int x = g->r.right - S(8);
    int i;

    for (i = n - 1; i >= 0; i--) {
        SIZE sz;

        GetTextExtentPoint32W(dc, names[i], (int)wcslen(names[i]), &sz);
        x -= sz.cx;
        text(dc, x, g->r.top - S(18), names[i], colors[i]);
        x -= S(12);
    }
}

/* накопленные столбики этапов видеокарты за последние LIVE_MS */
static void draw_gpu(HDC dc, const plot *g0, const ff_sample *s, unsigned n, double now)
{
    static const wchar_t *const NAMES[] = { L"copy", L"diff", L"draw" };
    static const COLORREF COLS[] = { RGB(66, 133, 244), RGB(251, 140, 0), RGB(52, 168, 83) };
    plot g = *g0;
    double peak = 0.2;
    unsigned i;

    g.t0 = now - LIVE_MS;
    g.t1 = now;
    for (i = 0; i < n; i++) {
        double total = (s[i].copy_ms > 0 ? s[i].copy_ms : 0) + (s[i].diff_ms > 0 ? s[i].diff_ms : 0)
                       + (s[i].draw_ms > 0 ? s[i].draw_ms : 0);

        if (s[i].t_ms >= g.t0 && total > peak) {
            peak = total;
        }
    }
    g.ymax = nice_max(peak);
    frame(dc, &g, ff_tr(S_DIAG_G_GPU), L"ms");
    legend(dc, &g, NAMES, COLS, 3);
    for (i = 0; i < n; i++) {
        float part[3];
        int k;
        int x;
        int ybase;

        if (s[i].t_ms < g.t0) {
            continue;
        }
        part[0] = s[i].copy_ms > 0 ? s[i].copy_ms : 0;
        part[1] = s[i].diff_ms > 0 ? s[i].diff_ms : 0;
        part[2] = s[i].draw_ms > 0 ? s[i].draw_ms : 0;
        x = px(&g, s[i].t_ms);
        ybase = g.r.bottom - 1;
        for (k = 0; k < 3; k++) {
            int ytop = ybase - (int)((double)part[k] / g.ymax * (double)(g.r.bottom - g.r.top - 1));
            RECT bar;
            HBRUSH br;

            if (ytop >= ybase) {
                continue;
            }
            SetRect(&bar, x, ytop, x + 2, ybase);
            br = CreateSolidBrush(COLS[k]);
            FillRect(dc, &bar, br);
            DeleteObject(br);
            ybase = ytop;
        }
    }
}

/* задержки линиями: процессор, Present, возраст кадра, интервал между отрисовками */
static void draw_latency(HDC dc, const plot *g0, const ff_sample *s, unsigned n, double now)
{
    static const wchar_t *const NAMES[] = { L"cpu", L"present", L"age", L"interval" };
    static const COLORREF COLS[] = { RGB(156, 39, 176), RGB(130, 130, 130), RGB(229, 57, 53), RGB(0, 150, 136) };
    plot g = *g0;
    double peak = 1.0;
    static double t[FF_RING_CAP];
    static float cpu[FF_RING_CAP];
    static float pres[FF_RING_CAP];
    static float age[FF_RING_CAP];
    static float gap[FF_RING_CAP];
    unsigned i;
    int m = 0;
    double last_draw = -1.0;

    g.t0 = now - LIVE_MS;
    g.t1 = now;
    for (i = 0; i < n; i++) {
        if (s[i].t_ms < g.t0) {
            continue;
        }
        t[m] = s[i].t_ms;
        cpu[m] = s[i].cpu_ms;
        pres[m] = s[i].present_ms;
        age[m] = s[i].age_ms;
        gap[m] = -1.0f;
        if (s[i].drew) {
            if (last_draw >= 0.0) {
                gap[m] = (float)(s[i].t_ms - last_draw);
            }
            last_draw = s[i].t_ms;
        }
        peak = cpu[m] > peak ? cpu[m] : peak;
        peak = pres[m] > peak ? pres[m] : peak;
        peak = age[m] > peak ? age[m] : peak;
        peak = gap[m] > peak ? gap[m] : peak;
        m++;
    }
    g.ymax = nice_max(peak);
    frame(dc, &g, ff_tr(S_DIAG_G_LAT), L"ms");
    legend(dc, &g, NAMES, COLS, 4);
    line_series(dc, &g, COLS[0], t, cpu, m);
    line_series(dc, &g, COLS[1], t, pres, m);
    line_series(dc, &g, COLS[2], t, age, m);
    line_series(dc, &g, COLS[3], t, gap, m);
}

/* мощность в ваттах и загрузка в процентах на одном графике: проценты растянуты на ту же высоту */
static void draw_power(HDC dc, const plot *g0, const ff_power_sample *p, unsigned n, double now)
{
    static const wchar_t *const NAMES[] = { L"W", L"gpu %", L"cpu %" };
    static const COLORREF COLS[] = { RGB(255, 87, 34), RGB(66, 133, 244), RGB(130, 130, 130) };
    plot g = *g0;
    double peak = 10.0;
    static double t[FF_PRING_CAP];
    static float w[FF_PRING_CAP];
    static float gpu[FF_PRING_CAP];
    static float cpu[FF_PRING_CAP];
    unsigned i;
    int m = 0;
    double ymax;

    g.t0 = now - POWER_SPAN_MS;
    g.t1 = now;
    for (i = 0; i < n; i++) {
        if (p[i].t_ms < g.t0) {
            continue;
        }
        peak = p[i].watts > peak ? p[i].watts : peak;
        t[m] = p[i].t_ms;
        w[m] = p[i].watts;
        gpu[m] = p[i].util_gpu;
        cpu[m] = p[i].cpu_pct;
        m++;
    }
    ymax = nice_max(peak);
    g.ymax = ymax;
    frame(dc, &g, ff_tr(S_DIAG_G_PWR), L"W");
    legend(dc, &g, NAMES, COLS, 3);
    line_series(dc, &g, COLS[0], t, w, m);
    /* проценты 0..100 приводятся к шкале ватт: 100 % это верх графика */
    for (i = 0; i < (unsigned)m; i++) {
        gpu[i] = gpu[i] < 0.0f ? -1.0f : (float)((double)gpu[i] * ymax / 100.0);
        cpu[i] = cpu[i] < 0.0f ? -1.0f : (float)((double)cpu[i] * ymax / 100.0);
    }
    line_series(dc, &g, COLS[1], t, gpu, m);
    line_series(dc, &g, COLS[2], t, cpu, m);
}

static void format_row(wchar_t *out, size_t n, const ff_run_row *r, const ff_run_row *off)
{
    const wchar_t *name = r->engine == FF_ENGINE_OFF ? L"off" : (r->engine == 0 ? L"classic"
                          : (r->engine == 1 ? L"compute" : L"kernel"));
    double sec = r->seconds > 0.0 ? r->seconds : 1.0;
    wchar_t delta[16] = L"      -";

    if (off && r->engine != FF_ENGINE_OFF && r->watts >= 0.0f && off->watts >= 0.0f) {
        _snwprintf(delta, 16, L"%+7.1f", (double)(r->watts - off->watts));
    }
    if (r->engine == FF_ENGINE_OFF) {
        _snwprintf(out, n, L"%-8ls %7ls %8ls %7ls %8ls %9ls %8ls %6.1f %7ls %5.0f %6.0f", name, L"-", L"-", L"-",
                   L"-", L"-", L"-", (double)r->watts, delta, (double)r->util_gpu, (double)r->clock_mhz);
    } else {
        _snwprintf(out, n, L"%-8ls %7.1f %8.3f %7.3f %8.3f %9.3f %8.2f %6.1f %7ls %5.0f %6.0f", name,
                   (double)r->cycles / sec, (double)r->gpu.mean, (double)r->gpu.p95, (double)r->cpu.mean,
                   (double)r->present.mean, (double)r->interval.mean, (double)r->watts, delta,
                   (double)r->util_gpu, (double)r->clock_mhz);
    }
    out[n - 1] = L'\0';
}

static void paint(HWND h)
{
    PAINTSTRUCT ps;
    HDC wdc = BeginPaint(h, &ps);
    HDC dc = CreateCompatibleDC(wdc);
    RECT rc;
    HBITMAP bmp;
    HGDIOBJ old_bmp;
    const ff_palette *pal = theme_get();
    double now = bench_now_ms();
    unsigned n;
    unsigned np;
    plot g;
    int eng_req = 0;
    int eng_eff = 0;
    int cs_ok = 0;
    char note[160];
    wchar_t line[200];
    int lh = S(16);
    int y;
    int i;

    GetClientRect(h, &rc);
    bmp = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
    old_bmp = SelectObject(dc, bmp);
    FillRect(dc, &rc, paint_bg());
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, paint_font());

    n = bench_copy(g_snap, FF_RING_CAP);
    np = bench_copy_power(g_psnap, FF_PRING_CAP);
    bench_get_engine_info(&eng_req, &eng_eff, &cs_ok, note, sizeof note);

    text(dc, S(16), S(17), ff_tr(S_ENGINE), pal->fg);

    /* строка состояния: прогон, откат движка, нет NVML */
    y = S(76);
    if (g_status[0]) {
        text(dc, S(16), y, g_status, pal->accent);
    }
    y += lh + S(2);
    _snwprintf(line, 200, ff_tr(S_DIAG_NOW), eng_req, eng_eff);
    text(dc, S(16), y, line, pal->fg);
    if (eng_req > eng_eff) {
        wchar_t fb[260];

        _snwprintf(fb, 260, ff_tr(S_DIAG_FALLBACK), eng_req, note[0] ? note : "-");
        text(dc, S(260), y, fb, RGB(229, 57, 53));
    } else if (!g_power_ok) {
        text(dc, S(260), y, ff_tr(S_DIAG_NO_NVML), pal->muted);
    }

    /* живая сводка за последние секунды по действующему движку */
    y += lh + S(4);
    {
        ff_run_row r;

        ff_run_summarize(g_snap, n, g_psnap, np, now - LIVE_MS, now, eng_eff, &r);
        if (r.cycles == 0) {
            text(dc, S(16), y, ff_tr(S_DIAG_NODATA), pal->muted);
        } else {
            SelectObject(dc, g_mono);
            _snwprintf(line, 200, L"copy %6.3f/%6.3f  diff %6.3f/%6.3f  draw %6.3f/%6.3f  gpu %6.3f/%6.3f (avg/p95 ms)",
                       (double)r.copy.mean, (double)r.copy.p95, (double)r.diff.mean, (double)r.diff.p95,
                       (double)r.draw.mean, (double)r.draw.p95, (double)r.gpu.mean, (double)r.gpu.p95);
            text(dc, S(16), y, line, pal->fg);
            _snwprintf(line, 200, L"cpu  %6.3f/%6.3f  pres %6.3f/%6.3f  age  %6.3f/%6.3f  gap %6.2f/%6.2f  cycles/s %.0f",
                       (double)r.cpu.mean, (double)r.cpu.p95, (double)r.present.mean, (double)r.present.p95,
                       (double)r.age.mean, (double)r.age.p95, (double)r.interval.mean, (double)r.interval.p99,
                       (double)r.cycles / (LIVE_MS / 1000.0));
            text(dc, S(16), y + lh, line, pal->fg);
            SelectObject(dc, paint_font());
        }
    }

    /* три графика */
    g.r.left = S(16);
    g.r.right = rc.right - S(16);
    g.t0 = 0;
    g.t1 = 1;
    g.ymax = 1;
    g.r.top = S(190);
    g.r.bottom = S(300);
    draw_gpu(dc, &g, g_snap, n, now);
    g.r.top = S(330);
    g.r.bottom = S(440);
    draw_latency(dc, &g, g_snap, n, now);
    g.r.top = S(470);
    g.r.bottom = S(580);
    draw_power(dc, &g, g_psnap, np, now);

    /* итоги прогона */
    y = S(590);
    text(dc, S(16), y, ff_tr(S_DIAG_TABLE), pal->fg);
    SelectObject(dc, g_mono);
    _snwprintf(line, 200, L"%-8ls %7ls %8ls %7ls %8ls %9ls %8ls %6ls %7ls %5ls %6ls", L"mode", L"cyc/s",
               L"gpu avg", L"p95", L"cpu avg", L"present", L"gap avg", L"W", L"dW", L"gpu%", L"MHz");
    text(dc, S(16), y + lh, line, pal->muted);
    for (i = 0; i < T.nrows; i++) {
        format_row(line, 200, &T.rows[i], T.nrows > 0 && T.rows[0].engine == FF_ENGINE_OFF ? &T.rows[0] : NULL);
        text(dc, S(16), y + (i + 2) * lh, line, pal->fg);
    }

    BitBlt(wdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, old_bmp);
    DeleteObject(bmp);
    DeleteDC(dc);
    EndPaint(h, &ps);
}

static void apply_engine(int engine)
{
    ui.app->cfg.engine = engine;
    app_apply(ui.app);
}

static void begin_step(void)
{
    int e = STEP_ENGINE[T.step];

    if (e == FF_ENGINE_OFF) {
        bench_set_force(0);
        app_set_enabled(ui.app, 0);
    } else {
        apply_engine(e);
        app_set_enabled(ui.app, 1);
        bench_set_force(1);
    }
    T.measuring = 0;
    T.phase_t0 = bench_now_ms();
}

static void end_test(int finished)
{
    if (!T.running) {
        return;
    }
    T.running = 0;
    bench_set_force(T.saved_force);
    apply_engine(T.saved_engine);
    app_set_enabled(ui.app, T.saved_enabled);
    PostMessageW(ui.app->hwnd, WM_APP_STATE, 0, 0);
    if (g_win) {
        SendMessageW(g_engine, CB_SETCURSEL, (WPARAM)T.saved_engine, 0);
        SetWindowTextW(GetDlgItem(g_win, ID_D_RUN), ff_tr(S_DIAG_RUN));
        EnableWindow(g_engine, TRUE);
        EnableWindow(GetDlgItem(g_win, ID_D_FORCE), TRUE);
    }
    T.done = finished;
    set_status(finished ? ff_tr(S_DIAG_DONE) : L"");
}

static void start_test(void)
{
    memset(T.rows, 0, sizeof T.rows);
    T.nrows = 0;
    T.done = 0;
    T.step = 0;
    T.running = 1;
    T.saved_engine = ui.app->cfg.engine;
    T.saved_enabled = ui.app->enabled != 0;
    T.saved_force = bench_force();
    bench_clear();
    SetWindowTextW(GetDlgItem(g_win, ID_D_RUN), ff_tr(S_DIAG_STOP));
    EnableWindow(g_engine, FALSE);
    EnableWindow(GetDlgItem(g_win, ID_D_FORCE), FALSE);
    begin_step();
}

/* замеры фазы собираются из колец; кадры вне окна времени и чужого движка отбрасывает сводка */
static void finish_step(double now)
{
    unsigned n = bench_copy(g_snap, FF_RING_CAP);
    unsigned np = bench_copy_power(g_psnap, FF_PRING_CAP);

    ff_run_summarize(g_snap, n, g_psnap, np, T.meas_t0, now, STEP_ENGINE[T.step], &T.rows[T.step]);
    T.nrows = T.step + 1;
    T.step++;
    if (T.step >= NSTEPS) {
        end_test(1);
    } else {
        begin_step();
    }
}

static void tick(void)
{
    ff_power_sample ps;
    double now = bench_now_ms();

    power_read(&ps);
    ps.t_ms = now;
    bench_push_power(&ps);
    if (T.running) {
        int off = STEP_ENGINE[T.step] == FF_ENGINE_OFF;
        double warm = off ? OFF_WARM_MS : WARM_MS;
        double meas = off ? OFF_MEAS_MS : MEAS_MS;
        double el = now - T.phase_t0;
        wchar_t what[40];
        wchar_t msg[96];

        if (!T.measuring && el >= warm) {
            T.measuring = 1;
            T.meas_t0 = now;
            T.phase_t0 = now;
            el = 0.0;
        }
        if (T.measuring && el >= meas) {
            finish_step(now);
        } else {
            double left = (T.measuring ? meas : warm + meas) - el;

            _snwprintf(what, 40, L"%ls", off ? L"off" : (STEP_ENGINE[T.step] == 0 ? L"classic"
                                                       : (STEP_ENGINE[T.step] == 1 ? L"compute" : L"kernel")));
            _snwprintf(msg, 96, ff_tr(S_DIAG_PHASE), what, (int)(left / 1000.0) + 1);
            set_status(msg);
        }
    }
    InvalidateRect(g_win, NULL, FALSE);
}

static void save_csv(void)
{
    OPENFILENAMEW o;
    wchar_t path[MAX_PATH] = L"fringefix-bench.csv";
    char *buf;
    size_t cap = 1536 * 1024;
    int len;
    int len2;
    unsigned n;
    ff_run_row live;
    double now = bench_now_ms();
    int e1;
    int e2;
    int e3;
    char note[8];

    memset(&o, 0, sizeof o);
    o.lStructSize = sizeof o;
    o.hwndOwner = g_win;
    o.lpstrFile = path;
    o.nMaxFile = MAX_PATH;
    o.lpstrFilter = L"CSV\0*.csv\0";
    o.lpstrDefExt = L"csv";
    o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&o)) {
        return;
    }
    buf = malloc(cap);
    if (!buf) {
        set_status(ff_tr(S_SAVE_FAILED));
        return;
    }
    n = bench_copy(g_snap, FF_RING_CAP);
    bench_get_engine_info(&e1, &e2, &e3, note, sizeof note);
    if (T.nrows > 0) {
        len = ff_run_csv(T.rows, T.nrows, buf, cap);
    } else {
        /* прогона не было: в итоги идёт сводка по последним секундам действующего движка */
        ff_run_summarize(g_snap, n, g_psnap, bench_copy_power(g_psnap, FF_PRING_CAP), now - LIVE_MS, now, e2,
                         &live);
        len = ff_run_csv(&live, 1, buf, cap);
    }
    if (len < 0 || (size_t)len + 16 >= cap) {
        free(buf);
        set_status(ff_tr(S_SAVE_FAILED));
        return;
    }
    len += snprintf(buf + len, cap - (size_t)len, "\n# samples\n");
    len2 = ff_samples_csv(g_snap, n, buf + len, cap - (size_t)len);
    if (len2 < 0) {
        /* сырые кадры не поместились: итоги всё равно сохраняются */
        buf[len] = '\0';
        len2 = 0;
    }
    set_status(ff_write_file(path, buf, (size_t)(len + len2)) == 0 ? ff_tr(S_SAVED) : ff_tr(S_SAVE_FAILED));
    free(buf);
}

static void on_command(int id, int code)
{
    if (id == ID_D_ENGINE && code == CBN_SELCHANGE && !T.running) {
        apply_engine((int)SendMessageW(g_engine, CB_GETCURSEL, 0, 0));
        app_save(ui.app);
    } else if (id == ID_D_FORCE && !T.running) {
        bench_set_force(SendMessageW(GetDlgItem(g_win, ID_D_FORCE), BM_GETCHECK, 0, 0) == BST_CHECKED);
        app_wake(ui.app);
    } else if (id == ID_D_RUN) {
        if (T.running) {
            end_test(0);
        } else {
            start_test();
        }
    } else if (id == ID_D_SAVE) {
        save_csv();
    } else if (id == ID_D_CLOSE) {
        DestroyWindow(g_win);
    }
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT:
        paint(h);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_COMMAND:
        on_command(LOWORD(w), HIWORD(w));
        return 0;
    case WM_TIMER:
        if (w == TIMER_ID) {
            tick();
        }
        return 0;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
        /* поля и список красятся как поле, подсказка и флажок как фон */
        return paint_ctlcolor(m, (HDC)w, (HWND)l, m == WM_CTLCOLORSTATIC || (HWND)l == GetDlgItem(h, ID_D_FORCE)
                                                      ? NULL : (HWND)l);
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        KillTimer(h, TIMER_ID);
        /* окно закрыто посреди прогона: движок и фильтр возвращаются как были */
        end_test(0);
        bench_set_wanted(0);
        bench_set_force(0);
        power_close();
        if (g_mono) {
            DeleteObject(g_mono);
            g_mono = NULL;
        }
        g_win = NULL;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void ui_bench_show(ff_app *a)
{
    WNDCLASSW wc;
    DWORD style = WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    RECT r;
    int bw = 190;
    int by = DIAG_H - 46;
    static const str_id ENGINES[] = { S_ENG_0, S_ENG_1, S_ENG_2 };
    char gpu[96];
    HWND force;
    int i;

    ui.app = a;
    if (g_win) {
        SetForegroundWindow(g_win);
        return;
    }
    if (ui.dpi == 0) {
        HDC dc = GetDC(NULL);

        ui.dpi = GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(NULL, dc);
        paint_init(ui.dpi);
    }
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = a->inst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    RegisterClassW(&wc);

    SetRect(&r, 0, 0, S(DIAG_W), S(DIAG_H));
    AdjustWindowRect(&r, style, FALSE);
    g_win = CreateWindowExW(WS_EX_CONTROLPARENT, CLASS_NAME, ff_tr(S_DIAG_TITLE), style, CW_USEDEFAULT,
                            CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, a->inst, NULL);
    SendMessageW(g_win, WM_SETICON, ICON_BIG,
                 (LPARAM)LoadImageW(a->inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));

    g_mono = CreateFontW(-S(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    /* верхняя строка: движок и нагрузочный режим; под ней подсказка и состояние рисуются в paint */
    g_engine = child(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 80, 12, 280, 200, ID_D_ENGINE);
    for (i = 0; i < 3; i++) {
        SendMessageW(g_engine, CB_ADDSTRING, 0, (LPARAM)ff_tr(ENGINES[i]));
    }
    SendMessageW(g_engine, CB_SETCURSEL, (WPARAM)a->cfg.engine, 0);
    force = child(L"BUTTON", ff_tr(S_DIAG_FORCE), BS_AUTOCHECKBOX | WS_TABSTOP, 380, 12, 420, 24, ID_D_FORCE);
    SendMessageW(force, BM_SETCHECK, bench_force() ? BST_CHECKED : BST_UNCHECKED, 0);
    child(L"BUTTON", ff_tr(S_DIAG_RUN), BS_PUSHBUTTON | WS_TABSTOP, 16, by, bw, 30, ID_D_RUN);
    child(L"BUTTON", ff_tr(S_DIAG_SAVE), BS_PUSHBUTTON | WS_TABSTOP, 16 + bw + 8, by, bw, 30, ID_D_SAVE);
    child(L"BUTTON", ff_tr(S_VER_CLOSE), BS_PUSHBUTTON | WS_TABSTOP, DIAG_W - 16 - 120, by, 120, 30, ID_D_CLOSE);

    /* подсказка отдельным статическим текстом, она длиннее строки состояния */
    child(L"STATIC", ff_tr(S_DIAG_HINT), SS_LEFT, 16, 42, DIAG_W - 32, 32, 0);

    bench_set_wanted(1);
    bench_get_gpu_name(gpu, sizeof gpu);
    g_power_ok = power_open(gpu) == 0;
    g_status[0] = L'\0';
    ctl_restyle_window(g_win);
    SetTimer(g_win, TIMER_ID, TICK_MS, NULL);
    ShowWindow(g_win, SW_SHOW);
    SetForegroundWindow(g_win);
}
