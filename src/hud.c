#include "hud.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "bench.h"
#include "i18n.h"

#define HUD_W 500
#define HUD_MARGIN 12
#define HUD_TICK_MS 250
#define HUD_TIMER 1
/* окно "живых" графиков и сводки, а потолок потребления шире: мощность меняется медленно */
#define LIVE_MS 4000.0
#define POWER_SPAN_MS 60000.0
/* после сбоя создания окна повтор не чаще, чем раз в столько мс: цикл захвата зовёт hud_sync на каждом кадре */
#define RETRY_MS 5000
/* высота одного графика с заголовком и промежутком */
#define GRAPH_H 76
#define GRAPH_GAP 18

static const wchar_t CLASS_NAME[] = L"fringefix_hud";

/* палитра панели: она всегда тёмная, чтобы читаться поверх любой картинки */
#define C_BG RGB(18, 20, 26)
#define C_FIELD RGB(10, 11, 15)
#define C_BORDER RGB(70, 75, 85)
#define C_FG RGB(232, 234, 240)
#define C_MUTED RGB(150, 156, 168)
#define C_WARN RGB(255, 112, 96)
#define C_ACCENT RGB(110, 168, 254)

typedef struct {
    RECT r;
    double t0;
    double t1;
    double ymax;
} plot;

/* dpi текущей отрисовки; панель рисуется из одного потока, поэтому статика безопасна */
static int g_dpi = 96;
static HFONT g_font;
static HFONT g_bold;
static HFONT g_mono;
static int g_font_dpi;

static int sc(int v)
{
    return MulDiv(v, g_dpi, 96);
}

void hud_release_fonts(void)
{
    if (g_font) {
        DeleteObject(g_font);
    }
    if (g_bold) {
        DeleteObject(g_bold);
    }
    if (g_mono) {
        DeleteObject(g_mono);
    }
    g_font = g_bold = g_mono = NULL;
    g_font_dpi = 0;
}

/* шрифты создаются под dpi и пересоздаются при его смене */
static void make_fonts(void)
{
    if (g_font && g_font_dpi == g_dpi) {
        return;
    }
    hud_release_fonts();
    g_font = CreateFontW(-sc(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_bold = CreateFontW(-sc(13), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    g_mono = CreateFontW(-sc(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    g_font_dpi = g_dpi;
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

/* рамка графика с заголовком над ней, подписью верхней границы и нулём */
static void frame(HDC dc, const plot *g, const wchar_t *title, const wchar_t *unit)
{
    HBRUSH br = CreateSolidBrush(C_FIELD);
    HPEN pen = CreatePen(PS_SOLID, 1, C_BORDER);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_br = SelectObject(dc, br);
    wchar_t top[48];
    int h = sc(15);

    Rectangle(dc, g->r.left, g->r.top, g->r.right, g->r.bottom);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_br);
    DeleteObject(pen);
    DeleteObject(br);
    SelectObject(dc, g_font);
    text(dc, g->r.left, g->r.top - h - sc(1), title, C_FG);
    _snwprintf(top, 48, L"%g %ls", g->ymax, unit);
    text(dc, g->r.left + sc(4), g->r.top + sc(1), top, C_MUTED);
    text(dc, g->r.left + sc(4), g->r.bottom - h, L"0", C_MUTED);
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

/* цветные подписи рядов в строке заголовка справа */
static void legend(HDC dc, const plot *g, const wchar_t *const *names, const COLORREF *colors, int n)
{
    int x = g->r.right - sc(6);
    int i;

    for (i = n - 1; i >= 0; i--) {
        SIZE sz;

        GetTextExtentPoint32W(dc, names[i], (int)wcslen(names[i]), &sz);
        x -= sz.cx;
        text(dc, x, g->r.top - sc(16), names[i], colors[i]);
        x -= sc(10);
    }
}

/* накопленные столбики этапов видеокарты за последние LIVE_MS */
static void draw_gpu(HDC dc, const plot *g0, const ff_sample *s, unsigned n, double now)
{
    static const wchar_t *const NAMES[] = { L"copy", L"diff", L"draw" };
    static const COLORREF COLS[] = { RGB(90, 150, 255), RGB(255, 160, 40), RGB(70, 200, 110) };
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
    static const COLORREF COLS[] = { RGB(200, 110, 230), RGB(170, 170, 170), RGB(255, 100, 96), RGB(40, 200, 180) };
    static double t[FF_RING_CAP];
    static float cpu[FF_RING_CAP];
    static float pres[FF_RING_CAP];
    static float age[FF_RING_CAP];
    static float gap[FF_RING_CAP];
    plot g = *g0;
    double peak = 1.0;
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
    static const COLORREF COLS[] = { RGB(255, 120, 70), RGB(90, 150, 255), RGB(170, 170, 170) };
    static double t[FF_PRING_CAP];
    static float w[FF_PRING_CAP];
    static float gpu[FF_PRING_CAP];
    static float cpu[FF_PRING_CAP];
    plot g = *g0;
    double peak = 10.0;
    unsigned i;
    int m = 0;

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
    g.ymax = nice_max(peak);
    frame(dc, &g, ff_tr(S_DIAG_G_PWR), L"W");
    legend(dc, &g, NAMES, COLS, 3);
    line_series(dc, &g, COLS[0], t, w, m);
    /* проценты 0..100 приводятся к шкале ватт: 100 % это верх графика */
    for (i = 0; i < (unsigned)m; i++) {
        gpu[i] = gpu[i] < 0.0f ? -1.0f : (float)((double)gpu[i] * g.ymax / 100.0);
        cpu[i] = cpu[i] < 0.0f ? -1.0f : (float)((double)cpu[i] * g.ymax / 100.0);
    }
    line_series(dc, &g, COLS[1], t, gpu, m);
    line_series(dc, &g, COLS[2], t, cpu, m);
}

void hud_size(int dpi, int *w, int *h)
{
    g_dpi = dpi > 0 ? dpi : 96;
    *w = sc(HUD_W);
    *h = sc(8 + 18 + 5 * 15 + 18 + 2 * (GRAPH_H + GRAPH_GAP) + GRAPH_H + 8);
}

void hud_paint(HDC dc, int w, int h, int dpi, const ff_sample *s, unsigned ns, const ff_power_sample *p,
               unsigned np, double now_ms, const ff_hud_info *info)
{
    static const wchar_t *const PRESENT[] = { L"vsync", L"tearing" };
    RECT rc;
    HBRUSH bg = CreateSolidBrush(C_BG);
    HPEN pen = CreatePen(PS_SOLID, 1, C_BORDER);
    HGDIOBJ old_pen;
    HGDIOBJ old_br;
    wchar_t line[200];
    ff_win_stats ws;
    ff_run_row r;
    plot g;
    int lh;
    int y;
    int x0;

    g_dpi = dpi > 0 ? dpi : 96;
    make_fonts();
    lh = sc(15);
    x0 = sc(10);
    SetRect(&rc, 0, 0, w, h);
    FillRect(dc, &rc, bg);
    old_pen = SelectObject(dc, pen);
    old_br = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, 0, 0, w, h);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_br);
    DeleteObject(pen);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);

    y = sc(8);
    SelectObject(dc, g_bold);
    text(dc, x0, y, ff_tr(S_HUD_TITLE), C_ACCENT);
    y += sc(18);
    SelectObject(dc, g_mono);

    /* строка 1: что действует. если система не дала заказанное, рядом пометка want */
    {
        wchar_t we[24] = L"";
        wchar_t wp[32] = L"";
        wchar_t wq[24] = L"";

        if (info->engine_req != info->engine_eff) {
            _snwprintf(we, 24, L" (want %d)", info->engine_req);
        }
        if (info->present_req != info->present_eff) {
            _snwprintf(wp, 32, L" (want %ls)", PRESENT[info->present_req ? 1 : 0]);
        }
        if (info->prio_req != info->prio_eff) {
            _snwprintf(wq, 24, L" (want %d)", info->prio_req);
        }
        _snwprintf(line, 200, L"engine %d%ls  present %ls%ls  prio %d%ls%ls", info->engine_eff, we,
                   PRESENT[info->present_eff ? 1 : 0], wp, info->prio_eff, wq, info->force ? L"  STRESS" : L"");
    }
    text(dc, x0, y, line, C_FG);
    y += lh;

    /* строка 2: счётчики кадров. dwm это обновления рабочего стола, которые принёс DWM, got то, что успела взять
     * программа, merged доля склеенных; в dwm входят и обновления от собственного окна оверлея */
    ff_win_stats_compute(s, ns, now_ms - LIVE_MS, now_ms, &ws);
    _snwprintf(line, 200, L"per s: dwm %.1f  got %.1f  drawn %.1f  merged %.1f%%  echo %u", (double)ws.source_per_s,
               (double)ws.acquired_per_s, (double)ws.draws_per_s, (double)ws.merged_pct, ws.echo);
    text(dc, x0, y, line, ws.merged_pct >= 25.0f ? C_WARN : C_FG);
    y += lh;

    /* строки 3 и 4: время этапов и задержки по действующему движку */
    ff_run_summarize(s, ns, p, np, now_ms - LIVE_MS, now_ms, info->engine_eff, &r);
    if (r.cycles == 0) {
        text(dc, x0, y, ff_tr(S_DIAG_NODATA), C_MUTED);
        y += 2 * lh;
    } else {
        _snwprintf(line, 200, L"gpu ms avg/p95: copy %.2f/%.2f diff %.2f/%.2f draw %.2f/%.2f", (double)r.copy.mean,
                   (double)r.copy.p95, (double)r.diff.mean, (double)r.diff.p95, (double)r.draw.mean,
                   (double)r.draw.p95);
        text(dc, x0, y, line, C_FG);
        y += lh;
        _snwprintf(line, 200, L"cpu %.2f  pres %.2f  age %.2f  gap %.1f/%.1f (avg/p99 ms)", (double)r.cpu.mean,
                   (double)r.present.mean, (double)r.age.mean, (double)r.interval.mean, (double)r.interval.p99);
        text(dc, x0, y, line, C_FG);
        y += lh;
    }

    /* пробная настройка: откат идёт сам, подтверждается кнопкой в окне диагностики */
    if (info->trial_left >= 0) {
        _snwprintf(line, 200, ff_tr(S_HUD_TRIAL), info->trial_left);
        SelectObject(dc, g_bold);
        text(dc, x0, y, line, C_WARN);
        SelectObject(dc, g_mono);
    }
    y += lh + sc(18);

    g.r.left = x0;
    g.r.right = w - x0;
    g.t0 = 0;
    g.t1 = 1;
    g.ymax = 1;
    g.r.top = y;
    g.r.bottom = y + sc(GRAPH_H);
    draw_gpu(dc, &g, s, ns, now_ms);
    g.r.top += sc(GRAPH_H + GRAPH_GAP);
    g.r.bottom = g.r.top + sc(GRAPH_H);
    draw_latency(dc, &g, s, ns, now_ms);
    g.r.top += sc(GRAPH_H + GRAPH_GAP);
    g.r.bottom = g.r.top + sc(GRAPH_H);
    draw_power(dc, &g, p, np, now_ms);
}

/* окно */

static HWND g_hud;
static ULONGLONG g_retry_at;
static RECT g_mon;
static ff_sample g_ss[FF_RING_CAP];
static ff_power_sample g_pp[FF_PRING_CAP];
static unsigned g_ns;
static unsigned g_np;
static double g_now;
static ff_hud_info g_info;

/* снимок колец и шапки раз в тик; рисование берёт готовые копии */
static void snapshot(void)
{
    char note[8];
    int e_cs;
    int pq;
    int pe;
    int qq;
    int qe;
    char vnote[8];

    g_ns = bench_copy(g_ss, FF_RING_CAP);
    g_np = bench_copy_power(g_pp, FF_PRING_CAP);
    g_now = bench_now_ms();
    memset(&g_info, 0, sizeof g_info);
    bench_get_engine_info(&g_info.engine_req, &g_info.engine_eff, &e_cs, note, sizeof note);
    bench_get_variant_info(&pq, &pe, &qq, &qe, vnote, sizeof vnote);
    g_info.present_req = pq;
    g_info.present_eff = pe;
    g_info.prio_req = qq;
    g_info.prio_eff = qe;
    g_info.trial_left = bench_trial_left();
    g_info.force = bench_force();
}

static int screen_dpi(void)
{
    HDC dc = GetDC(NULL);
    int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;

    if (dc) {
        ReleaseDC(NULL, dc);
    }
    return dpi > 0 ? dpi : 96;
}

static void paint(HWND h)
{
    PAINTSTRUCT ps;
    HDC wdc = BeginPaint(h, &ps);
    HDC dc = CreateCompatibleDC(wdc);
    RECT rc;
    HBITMAP bmp;
    HGDIOBJ old;

    GetClientRect(h, &rc);
    bmp = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
    if (dc && bmp) {
        old = SelectObject(dc, bmp);
        hud_paint(dc, rc.right, rc.bottom, screen_dpi(), g_ss, g_ns, g_pp, g_np, g_now, &g_info);
        BitBlt(wdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, old);
    }
    if (bmp) {
        DeleteObject(bmp);
    }
    if (dc) {
        DeleteDC(dc);
    }
    EndPaint(h, &ps);
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT:
        paint(h);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER:
        if (w == HUD_TIMER) {
            snapshot();
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    case WM_NCHITTEST:
        /* окно пропускает мышь насквозь: оно не должно мешать игре и рабочему столу */
        return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_DESTROY:
        KillTimer(h, HUD_TIMER);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void close_window(void)
{
    if (g_hud) {
        DestroyWindow(g_hud);
        g_hud = NULL;
    }
    hud_release_fonts();
}

static void open_window(const RECT *mon)
{
    static int registered;
    int w;
    int hh;
    int dpi = screen_dpi();

    if (!registered) {
        WNDCLASSW wc;

        memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = proc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = CLASS_NAME;
        RegisterClassW(&wc);
        registered = 1;
    }
    hud_size(dpi, &w, &hh);
    g_hud = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                            CLASS_NAME, L"fringefix hud", WS_POPUP, mon->left + sc(HUD_MARGIN),
                            mon->top + sc(HUD_MARGIN), w, hh, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!g_hud) {
        g_retry_at = GetTickCount64() + RETRY_MS;
        return;
    }
    /* без исключения из захвата панель попала бы в кадр: фильтр её исказил бы, а каждое её обновление рождало
     * бы кадр-эхо. если система не умеет, панели нет вовсе */
    if (!SetWindowDisplayAffinity(g_hud, WDA_EXCLUDEFROMCAPTURE)) {
        close_window();
        g_retry_at = GetTickCount64() + RETRY_MS;
        return;
    }
    SetLayeredWindowAttributes(g_hud, 0, 235, LWA_ALPHA);
    snapshot();
    SetTimer(g_hud, HUD_TIMER, HUD_TICK_MS, NULL);
    ShowWindow(g_hud, SW_SHOWNOACTIVATE);
    g_mon = *mon;
}

void hud_sync(const RECT *monitor, int want)
{
    if (!want || !monitor) {
        close_window();
        return;
    }
    if (g_hud) {
        /* монитор сдвинулся или сменил режим: панель переезжает вслед за ним */
        if (memcmp(&g_mon, monitor, sizeof g_mon) != 0) {
            int w;
            int hh;

            hud_size(screen_dpi(), &w, &hh);
            SetWindowPos(g_hud, HWND_TOPMOST, monitor->left + sc(HUD_MARGIN), monitor->top + sc(HUD_MARGIN), w, hh,
                         SWP_NOACTIVATE);
            g_mon = *monitor;
        }
        return;
    }
    if (GetTickCount64() < g_retry_at) {
        return;
    }
    open_window(monitor);
}

void hud_reassert(void)
{
    if (g_hud) {
        SetWindowPos(g_hud, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW);
    }
}
