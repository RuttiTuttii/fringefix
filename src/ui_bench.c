#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "bench.h"
#include "benchstat.h"
#include "fileio.h"
#include "hud.h"
#include "i18n.h"
#include "power.h"
#include "resource.h"
#include "theme.h"
#include "trial.h"
#include "ui.h"
#include "ui_layout.h"
#include "ui_paint.h"
#include "ui_state.h"

#define DIAG_W 820
#define DIAG_H 548
#define TICK_MS 250
#define TIMER_ID 1
/* прогон: выключенный фильтр для базового потребления, затем три движка; сначала разогрев, потом замер */
#define NSTEPS 4
#define WARM_MS 3000.0
#define MEAS_MS 12000.0
#define OFF_WARM_MS 4000.0
#define OFF_MEAS_MS 12000.0
/* окно "живой" сводки в csv без прогона; графики и счётчики теперь на панели в оверлее (hud.c) */
#define LIVE_MS 4000.0

enum { ID_D_ENGINE = 800, ID_D_FORCE, ID_D_RUN, ID_D_SAVE, ID_D_CLOSE, ID_D_PRESENT, ID_D_PRIO, ID_D_HUD,
       ID_D_KEEP, ID_D_REVERT, ID_D_TRIALTXT, ID_D_CORNER };

/* приоритет GPU в списке: значения из ini ближе всего к одному из пунктов */
static const int PRIO_VALUES[5] = { -7, -3, 0, 3, 7 };

static const wchar_t CLASS_NAME[] = L"fringefix_diag";
static HWND g_win;
static HWND g_engine;
static HWND g_present;
static HWND g_prio;
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

/* пробная смена движка, вывода или приоритета: действует, но в ini не пишется, пока не нажато "Оставить" */
static ff_trial g_trial;
static unsigned g_fail_base;

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

static void text(HDC dc, int x, int y, const wchar_t *s, COLORREF c)
{
    SetTextColor(dc, c);
    TextOutW(dc, x, y, s, (int)wcslen(s));
}

static void format_row(wchar_t *out, size_t n, const ff_run_row *r, const ff_run_row *off)
{
    const wchar_t *name = r->engine == FF_ENGINE_OFF ? L"off" : (r->engine == 0 ? L"classic"
                          : (r->engine == 1 ? L"compute" : L"kernel"));
    double sec = r->seconds > 0.0 ? r->seconds : 1.0;
    wchar_t delta[16] = L"      -";
    wchar_t merged[16] = L"      -";

    if (off && r->engine != FF_ENGINE_OFF && r->watts >= 0.0f && off->watts >= 0.0f) {
        _snwprintf(delta, 16, L"%+7.1f", (double)(r->watts - off->watts));
    }
    if (r->engine != FF_ENGINE_OFF && r->source > 0) {
        _snwprintf(merged, 16, L"%7.1f", 100.0 * (double)(r->source - r->cycles) / (double)r->source);
    }
    if (r->engine == FF_ENGINE_OFF) {
        _snwprintf(out, n, L"%-8ls %7ls %8ls %7ls %8ls %9ls %8ls %7ls %6.1f %7ls %5.0f %6.0f", name, L"-", L"-",
                   L"-", L"-", L"-", L"-", merged, (double)r->watts, delta, (double)r->util_gpu,
                   (double)r->clock_mhz);
    } else {
        _snwprintf(out, n, L"%-8ls %7.1f %8.3f %7.3f %8.3f %9.3f %8.2f %7ls %6.1f %7ls %5.0f %6.0f", name,
                   (double)r->cycles / sec, (double)r->gpu.mean, (double)r->gpu.p95, (double)r->cpu.mean,
                   (double)r->present.mean, (double)r->interval.mean, merged, (double)r->watts, delta,
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
    int eng_req = 0;
    int eng_eff = 0;
    int cs_ok = 0;
    int p_req = 0;
    int p_eff = 0;
    int q_req = 0;
    int q_eff = 0;
    char note[160];
    char vnote[160];
    wchar_t line[260];
    int lh = S(16);
    int off;
    int y;
    int i;

    GetClientRect(h, &rc);
    bmp = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
    old_bmp = SelectObject(dc, bmp);
    FillRect(dc, &rc, paint_bg());
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, paint_font());

    bench_get_engine_info(&eng_req, &eng_eff, &cs_ok, note, sizeof note);
    bench_get_variant_info(&p_req, &p_eff, &q_req, &q_eff, vnote, sizeof vnote);

    /* параметры лежат на карточке с отступом 16 от краёв окна, как в остальных окнах программы */
    paint_card(dc, ui.dpi, 16, 16, DIAG_W - 32, 134, ff_tr(S_DIAG_PARAMS));
    SelectObject(dc, paint_font());
    text(dc, S(30), S(49), ff_tr(S_ENGINE), pal->fg);
    text(dc, S(30), S(85), ff_tr(S_DIAG_PRESENT), pal->fg);
    text(dc, S(410), S(85), ff_tr(S_DIAG_PRIO), pal->fg);
    text(dc, S(410), S(121), ff_tr(S_DIAG_CORNER), pal->fg);

    /* пока идёт проба, под подсказкой плашка с кнопками, а состояние и итоги уезжают вниз */
    off = ff_trial_active(&g_trial) ? S(72) : 0;

    /* строки состояния: прогон или откат, что действует на самом деле, нет NVML */
    y = S(240) + off;
    if (g_status[0]) {
        text(dc, S(16), y, g_status, pal->accent);
    }
    y += lh + S(2);
    _snwprintf(line, 260, ff_tr(S_DIAG_NOW), eng_req, eng_eff);
    text(dc, S(16), y, line, pal->fg);
    if (eng_req > eng_eff) {
        wchar_t fb[260];

        _snwprintf(fb, 260, ff_tr(S_DIAG_FALLBACK), eng_req, note[0] ? note : "-");
        text(dc, S(260), y, fb, RGB(229, 57, 53));
    } else if (!g_power_ok) {
        text(dc, S(260), y, ff_tr(S_DIAG_NO_NVML), pal->muted);
    }
    y += lh + S(2);
    _snwprintf(line, 260, ff_tr(S_DIAG_VNOW), p_req, p_eff, q_req, q_eff);
    text(dc, S(16), y, line, p_req != p_eff || q_req != q_eff ? RGB(229, 57, 53) : pal->fg);
    if (vnote[0]) {
        _snwprintf(line, 260, L"%hs", vnote);
        text(dc, S(16), y + lh, line, pal->muted);
    }

    /* итоги прогона */
    y = S(318) + off;
    text(dc, S(16), y, ff_tr(S_DIAG_TABLE), pal->fg);
    SelectObject(dc, g_mono);
    _snwprintf(line, 260, L"%-8ls %7ls %8ls %7ls %8ls %9ls %8ls %7ls %6ls %7ls %5ls %6ls", L"mode", L"cyc/s",
               L"gpu avg", L"p95", L"cpu avg", L"present", L"gap avg", L"merged%", L"W", L"dW", L"gpu%", L"MHz");
    text(dc, S(16), y + lh, line, pal->muted);
    for (i = 0; i < T.nrows; i++) {
        format_row(line, 260, &T.rows[i], T.nrows > 0 && T.rows[0].engine == FF_ENGINE_OFF ? &T.rows[0] : NULL);
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

/* текущие значения вывода из настроек */
static ff_variant current_variant(void)
{
    ff_variant v;

    v.engine = ui.app->cfg.engine;
    v.present = ui.app->cfg.present_mode;
    v.priority = ui.app->cfg.gpu_priority;
    return v;
}

static int prio_index(int value)
{
    int best = 0;
    int i;

    for (i = 1; i < 5; i++) {
        if (abs(PRIO_VALUES[i] - value) < abs(PRIO_VALUES[best] - value)) {
            best = i;
        }
    }
    return best;
}

/* значения, выбранные в списках; приоритет из ini вне пунктов списка остаётся как есть, пока его не тронули */
static ff_variant read_variant(void)
{
    ff_variant v = current_variant();
    int sel = (int)SendMessageW(g_engine, CB_GETCURSEL, 0, 0);

    if (sel >= 0 && sel <= 2) {
        v.engine = sel;
    }
    sel = (int)SendMessageW(g_present, CB_GETCURSEL, 0, 0);
    if (sel == 0 || sel == 1) {
        v.present = sel;
    }
    sel = (int)SendMessageW(g_prio, CB_GETCURSEL, 0, 0);
    if (sel >= 0 && sel < 5 && sel != prio_index(v.priority)) {
        v.priority = PRIO_VALUES[sel];
    }
    return v;
}

static void select_variant(const ff_variant *v)
{
    SendMessageW(g_engine, CB_SETCURSEL, (WPARAM)v->engine, 0);
    SendMessageW(g_present, CB_SETCURSEL, (WPARAM)v->present, 0);
    SendMessageW(g_prio, CB_SETCURSEL, (WPARAM)prio_index(v->priority), 0);
}

/* кладёт значения в настройки. способ вывода меняет цепочку, поэтому оверлей пересоздаётся, остальное на ходу */
static void apply_variant(const ff_variant *v)
{
    int reinit = ui.app->cfg.present_mode != v->present;

    ui.app->cfg.engine = v->engine;
    ui.app->cfg.present_mode = v->present;
    ui.app->cfg.gpu_priority = v->priority;
    if (reinit) {
        app_reinit(ui.app);
    } else {
        app_apply(ui.app);
    }
}

static void show_trial(int on)
{
    int cmd = on ? SW_SHOW : SW_HIDE;

    ShowWindow(GetDlgItem(g_win, ID_D_TRIALTXT), cmd);
    ShowWindow(GetDlgItem(g_win, ID_D_KEEP), cmd);
    ShowWindow(GetDlgItem(g_win, ID_D_REVERT), cmd);
    EnableWindow(GetDlgItem(g_win, ID_D_RUN), !on);
}

static void update_trial_text(double now)
{
    static int shown = -1;
    int left = (int)(ff_trial_left(&g_trial, now) / 1000.0) + 1;
    wchar_t msg[256];

    bench_set_trial_left(left);
    if (left != shown) {
        shown = left;
        _snwprintf(msg, 256, ff_tr(S_DIAG_TRIAL), left);
        SetWindowTextW(GetDlgItem(g_win, ID_D_TRIALTXT), msg);
    }
}

/* откат к значениям до пробы; reenable включает оверлей, если ошибка запуска его выключила.
 * файл настроек тоже перезаписывается: другое окно могло сохранить его вместе с пробными значениями */
static void revert_to(const ff_variant *back, str_id msg, int reenable)
{
    apply_variant(back);
    if (reenable) {
        app_set_enabled(ui.app, 1);
    }
    app_save(ui.app);
    bench_set_trial_left(-1);
    if (g_win) {
        select_variant(back);
        show_trial(0);
        set_status(ff_tr(msg));
    }
}

/* значение выбрали в одном из списков: применяется пробно, в ini не пишется, отсчёт десяти секунд идёт заново */
static void change_variant(void)
{
    ff_variant now = current_variant();
    ff_variant want = read_variant();

    if (ff_variant_equal(&now, &want)) {
        return;
    }
    if (!ff_trial_active(&g_trial)) {
        g_fail_base = bench_gfx_failures();
    }
    ff_trial_begin(&g_trial, &now, bench_now_ms());
    apply_variant(&want);
    show_trial(1);
    update_trial_text(bench_now_ms());
    set_status(L"");
}

static void keep_trial(void)
{
    ff_trial_keep(&g_trial);
    app_save(ui.app);
    bench_set_trial_left(-1);
    show_trial(0);
    set_status(ff_tr(S_DIAG_KEPT));
}

static void cancel_trial(str_id msg, int reenable)
{
    ff_variant back;

    if (ff_trial_cancel(&g_trial, &back)) {
        revert_to(&back, msg, reenable);
    }
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
        EnableWindow(g_present, TRUE);
        EnableWindow(g_prio, TRUE);
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
    EnableWindow(g_present, FALSE);
    EnableWindow(g_prio, FALSE);
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
    /* проба: сбой оверлея откатывает сразу, иначе по таймеру, если не нажали "Оставить" */
    if (ff_trial_active(&g_trial)) {
        ff_variant back;

        if (bench_gfx_failures() != g_fail_base) {
            cancel_trial(S_DIAG_FAILED, 1);
        } else if (ff_trial_expired(&g_trial, now, &back)) {
            revert_to(&back, S_DIAG_REVERTED, 0);
        } else {
            update_trial_text(now);
        }
    }
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
    if ((id == ID_D_ENGINE || id == ID_D_PRESENT || id == ID_D_PRIO) && code == CBN_SELCHANGE && !T.running) {
        change_variant();
    } else if (id == ID_D_FORCE && !T.running) {
        bench_set_force(SendMessageW(GetDlgItem(g_win, ID_D_FORCE), BM_GETCHECK, 0, 0) == BST_CHECKED);
        app_wake(ui.app);
    } else if (id == ID_D_CORNER && code == CBN_SELCHANGE) {
        /* угол панели не пробная настройка: действует сразу и пишется в ini, откатывать нечего */
        int sel = (int)SendMessageW(GetDlgItem(g_win, ID_D_CORNER), CB_GETCURSEL, 0, 0);

        if (sel >= 0 && sel <= 3) {
            ui.app->cfg.hud_corner = sel;
            hud_set_corner(sel);
            /* во время пробы в cfg лежат ещё не подтверждённые движок и вывод: писать ini сейчас значило бы оставить их;
             * угол запишется вместе с «Оставить» или следующим сохранением */
            if (!ff_trial_active(&g_trial)) {
                app_save(ui.app);
            }
            app_wake(ui.app);
        }
    } else if (id == ID_D_HUD) {
        bench_set_hud(SendMessageW(GetDlgItem(g_win, ID_D_HUD), BM_GETCHECK, 0, 0) == BST_CHECKED);
        app_wake(ui.app);
    } else if (id == ID_D_KEEP) {
        keep_trial();
    } else if (id == ID_D_REVERT) {
        cancel_trial(S_DIAG_REVERTED, 0);
    } else if (id == ID_D_RUN) {
        if (T.running) {
            end_test(0);
        } else if (!ff_trial_active(&g_trial)) {
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
        /* флажки лежат на карточке, поля и список красятся как поле, подсказка как фон окна */
        if ((HWND)l == GetDlgItem(h, ID_D_FORCE) || (HWND)l == GetDlgItem(h, ID_D_HUD)) {
            return paint_ctlcolor_card((HDC)w);
        }
        return paint_ctlcolor(m, (HDC)w, (HWND)l, m == WM_CTLCOLORSTATIC ? NULL : (HWND)l);
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        KillTimer(h, TIMER_ID);
        /* окно закрыто посреди прогона или пробы: движок, вывод и фильтр возвращаются как были, закрытие это не "Оставить" */
        end_test(0);
        cancel_trial(S_DIAG_REVERTED, 0);
        bench_set_wanted(0);
        bench_set_force(0);
        bench_set_hud(0);
        bench_set_trial_left(-1);
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
    static const str_id PRESENTS[] = { S_PRES_0, S_PRES_1 };
    static const str_id PRIOS[] = { S_PRIO_0, S_PRIO_1, S_PRIO_2, S_PRIO_3, S_PRIO_4 };
    char gpu[96];
    HWND force;
    HWND hud;
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

    /* две строки списков: движок и нагрузочный режим, вывод кадра и приоритет GPU. подписи, состояние и итоги
     * рисуются в paint. каждое новое значение действует пробно (change_variant) */
    g_engine = child(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 130, 44, 250, 200, ID_D_ENGINE);
    for (i = 0; i < 3; i++) {
        SendMessageW(g_engine, CB_ADDSTRING, 0, (LPARAM)ff_tr(ENGINES[i]));
    }
    force = child(L"BUTTON", ff_tr(S_DIAG_FORCE), BS_AUTOCHECKBOX | WS_TABSTOP, 410, 44, 370, 24, ID_D_FORCE);
    SendMessageW(force, BM_SETCHECK, bench_force() ? BST_CHECKED : BST_UNCHECKED, 0);
    g_present = child(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 130, 80, 250, 200, ID_D_PRESENT);
    for (i = 0; i < 2; i++) {
        SendMessageW(g_present, CB_ADDSTRING, 0, (LPARAM)ff_tr(PRESENTS[i]));
    }
    g_prio = child(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 530, 80, 250, 200, ID_D_PRIO);
    for (i = 0; i < 5; i++) {
        SendMessageW(g_prio, CB_ADDSTRING, 0, (LPARAM)ff_tr(PRIOS[i]));
    }
    {
        ff_variant v = current_variant();

        select_variant(&v);
    }
    /* панель в оверлее по умолчанию включена: окно диагностики открыто, значит замеры нужны */
    hud = child(L"BUTTON", ff_tr(S_DIAG_HUD), BS_AUTOCHECKBOX | WS_TABSTOP, 30, 116, 360, 24, ID_D_HUD);
    {
        static const str_id CORNERS[] = { S_CORNER_0, S_CORNER_1, S_CORNER_2, S_CORNER_3 };
        HWND corner = child(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 550, 116, 230, 200, ID_D_CORNER);

        for (i = 0; i < 4; i++) {
            SendMessageW(corner, CB_ADDSTRING, 0, (LPARAM)ff_tr(CORNERS[i]));
        }
        SendMessageW(corner, CB_SETCURSEL, (WPARAM)ui.app->cfg.hud_corner, 0);
    }
    SendMessageW(hud, BM_SETCHECK, BST_CHECKED, 0);
    bench_set_hud(1);
    child(L"BUTTON", ff_tr(S_DIAG_RUN), BS_PUSHBUTTON | WS_TABSTOP, 16, by, bw, 30, ID_D_RUN);
    child(L"BUTTON", ff_tr(S_DIAG_SAVE), BS_PUSHBUTTON | WS_TABSTOP, 16 + bw + 8, by, bw, 30, ID_D_SAVE);
    child(L"BUTTON", ff_tr(S_VER_CLOSE), BS_PUSHBUTTON | WS_TABSTOP, DIAG_W - 16 - 120, by, 120, 30, ID_D_CLOSE);

    /* подсказка отдельным статическим текстом, она длиннее строки состояния */
    child(L"STATIC", ff_tr(S_DIAG_HINT), SS_LEFT, 16, 162, DIAG_W - 32, 64, 0);

    /* плашка пробы с двумя кнопками скрыта, пока нечего подтверждать */
    child(L"STATIC", L"", SS_LEFT, 16, 240, DIAG_W - 32, 34, ID_D_TRIALTXT);
    child(L"BUTTON", ff_tr(S_DIAG_KEEP), BS_PUSHBUTTON | WS_TABSTOP, 16, 278, 120, 28, ID_D_KEEP);
    child(L"BUTTON", ff_tr(S_DIAG_REVERT), BS_PUSHBUTTON | WS_TABSTOP, 144, 278, 120, 28, ID_D_REVERT);
    show_trial(0);

    bench_set_wanted(1);
    bench_get_gpu_name(gpu, sizeof gpu);
    g_power_ok = power_open(gpu) == 0;
    g_status[0] = L'\0';
    ctl_restyle_window(g_win);
    SetTimer(g_win, TIMER_ID, TICK_MS, NULL);
    ShowWindow(g_win, SW_SHOW);
    SetForegroundWindow(g_win);
}
