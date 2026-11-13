#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "i18n.h"
#include "output.h"
#include "slider.h"
#include "theme.h"
#include "ui_paint.h"
#include "ui_state.h"

#define MAX_OUTPUTS 8
#define EDIT_CHARS 24
#define POWER_W 46

/* набор монитора, выбранного в списке окна: все ползунки и поля правят его; в режиме Б повёрнутого монитора
 * это набор его ориентации */
static ff_view *cur_view(void)
{
    return app_edit_view(ui.app);
}

static float *row_value(int idx)
{
    ff_view *v = cur_view();

    return idx == IDX_SHARPEN ? &v->sharpen : &v->shift[idx / 2][idx % 2];
}

/* имена выходов на момент построения окна: список мониторов пересобирается из них при каждой смене состояния */
static wchar_t g_names[MAX_OUTPUTS][OUTPUT_NAME_LEN];
static int g_nout;
/* поворот выбранного монитора, при котором в поля положены значения: в режиме Б при смене поворота набор другой */
static int g_rot_seen = -1;

/* список мониторов с пометкой, где фильтр включён; выбран монитор из настроек */
static void fill_monitors(void)
{
    int i;

    SendMessageW(ui.monitor, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < g_nout; i++) {
        wchar_t item[OUTPUT_NAME_LEN + 64];
        wchar_t rot[24] = L"";

        /* повёрнутый монитор помечается углом */
        if (ui.app->rot[i]) {
            _snwprintf(rot, sizeof rot / sizeof rot[0], L", %ls %ld\u00b0", ff_tr(S_MON_ROT), ui.app->rot[i] * 90L);
        }
        _snwprintf(item, sizeof item / sizeof item[0], L"%ls  [%ls%ls]", g_names[i],
                   ff_tr(ui.app->cfg.view[i].enabled && !app_monitor_failed(ui.app, i) ? S_MON_TAG_ON
                                                                                       : S_MON_TAG_OFF), rot);
        item[sizeof item / sizeof item[0] - 1] = L'\0';
        SendMessageW(ui.monitor, CB_ADDSTRING, 0, (LPARAM)item);
    }
    SendMessageW(ui.monitor, CB_SETCURSEL, (WPARAM)ui.app->cfg.monitor, 0);
}

static double row_limit(int idx)
{
    return idx == IDX_SHARPEN ? FF_SHARPEN_LIMIT : FF_SHIFT_LIMIT;
}

/* позиция ползунка и поле ввода хранят сотые доли */
static LPARAM to_pos(float v)
{
    return lroundf(v * 100.0f);
}

void ctl_edit_show(int idx)
{
    wchar_t t[EDIT_CHARS];

    _snwprintf(t, EDIT_CHARS, L"%.2f", (double)*row_value(idx));
    t[EDIT_CHARS - 1] = L'\0';
    ui.sync++;
    SetWindowTextW(ui.edit[idx], t);
    ui.sync--;
}

static void sync_values(void)
{
    int i;

    g_rot_seen = (int)ui.app->rot[ui.app->cfg.monitor];
    for (i = 0; i <= IDX_SHARPEN; i++) {
        SendMessageW(ui.slider[i], FFS_SETPOS, (WPARAM)to_pos(*row_value(i)), 0);
        ctl_edit_show(i);
    }
}

void ctl_sync(void)
{
    sync_values();
    ctl_sync_power();
}

/* выключатель и список мониторов по настройкам; общий выключатель (хоткей, трей) гасит подпись, но не ползунок:
 * ползунок хранит выбор этого монитора, а общий выключатель действует поверх всех */
void ctl_sync_power(void)
{
    int on = cur_view()->enabled != 0;
    const wchar_t *state = ff_tr(on ? S_STATE_ON : S_STATE_OFF);
    wchar_t label[96];

    /* поток захвата увидел другой поворот монитора: значения в режиме Б теперь из другого набора */
    if ((int)ui.app->rot[ui.app->cfg.monitor] != g_rot_seen) {
        sync_values();
    }
    SendMessageW(ui.power, FFS_SETPOS, (WPARAM)on, 0);
    if (on && app_monitor_failed(ui.app, ui.app->cfg.monitor)) {
        state = ff_tr(S_STATE_FAILED);
    } else if (on && !ui.app->enabled) {
        state = ff_tr(S_FILTER_MASTER_OFF);
    }
    _snwprintf(label, sizeof label / sizeof label[0], L"%ls: %ls", ff_tr(S_FILTER), state);
    SetWindowTextW(ui.power_label, label);
    fill_monitors();
}

void ctl_slider_changed(int idx)
{
    int pos = (int)SendMessageW(ui.slider[idx], FFS_GETPOS, 0, 0);

    *row_value(idx) = (float)pos / 100.0f;
    ctl_edit_show(idx);
    app_apply(ui.app);
}

/* число с клавиатуры: запятая тоже годится, недописанный ввод вроде "-" или "0," молча ждёт */
void ctl_edit_changed(int idx)
{
    wchar_t t[EDIT_CHARS];
    wchar_t *end;
    wchar_t *c;
    double d;
    double lim = row_limit(idx);

    GetWindowTextW(ui.edit[idx], t, EDIT_CHARS);
    for (c = t; *c; c++) {
        if (*c == L',') {
            *c = L'.';
        }
    }
    d = wcstod(t, &end);
    if (end == t || *end != L'\0' || !isfinite(d)) {
        return;
    }
    d = d < -lim ? -lim : (d > lim ? lim : d);
    *row_value(idx) = (float)d;
    SendMessageW(ui.slider[idx], FFS_SETPOS, (WPARAM)to_pos((float)d), 0);
    app_apply(ui.app);
}

void ctl_reset(void)
{
    ff_view_reset(cur_view());
    ctl_sync();
    app_apply(ui.app);
}

void ctl_power_changed(void)
{
    cur_view()->enabled = (int)SendMessageW(ui.power, FFS_GETPOS, 0, 0);
    /* включение даёт монитору, упавшему при запуске, новую попытку */
    app_retry(ui.app);
    ctl_sync_power();
    app_apply(ui.app);
}

void ctl_monitor_changed(void)
{
    int sel = (int)SendMessageW(ui.monitor, CB_GETCURSEL, 0, 0);

    if (sel < 0 || sel >= g_nout) {
        return;
    }
    ui.app->cfg.monitor = sel;
    ctl_sync();
    /* диагностика следует за выбранным монитором: поток нового монитора заново отдаёт ей информацию о движке */
    app_apply(ui.app);
}

static HWND child(const wchar_t *cls, const wchar_t *text, DWORD style, DWORD ex, int x, int y,
                  int w, int h, int id)
{
    return CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h),
                           ui.win, (HMENU)(INT_PTR)id, ui.app->inst, NULL);
}

static void build_row(int i)
{
    int idx = ROWS[i].idx;
    int y = row_y(&ROWS[i]);
    int hi = idx == IDX_SHARPEN ? 100 : 200;

    ui.slider[idx] = child(FF_SLIDER_CLASS, L"", WS_TABSTOP, 0, MARGIN + COL_SLIDER, y + 1,
                           SLIDER_W, CTRL_H, ID_SLIDER + idx);
    SendMessageW(ui.slider[idx], FFS_SETRANGE, (WPARAM)-hi, hi);
    ui.edit[idx] = child(L"EDIT", L"", ES_RIGHT | ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE,
                         MARGIN + COL_EDIT, y + 3, EDIT_W, 24, ID_EDIT + idx);
}

static void build_button(str_id text, int col, int y, int id)
{
    int bw = (CARD_W - 2 * 16) / 3;

    child(L"BUTTON", ff_tr(text), BS_PUSHBUTTON | WS_TABSTOP, 0, MARGIN + col * (bw + 16), y, bw,
          BTN_H, id);
}

/* выключатель фильтра выбранного монитора: переключатель в стиле Windows под левой кнопкой, справа подпись с состоянием */
static void build_power(void)
{
    ui.power = child(FF_SLIDER_CLASS, L"", WS_TABSTOP | FFSS_ON_WINDOW, 0, MARGIN, POWER_Y, POWER_W, CTRL_H, ID_POWER);
    SendMessageW(ui.power, FFS_SETRANGE, 0, 1);
    ui.power_label = child(L"STATIC", L"", SS_LEFT | SS_CENTERIMAGE, 0, MARGIN + POWER_W + 12, POWER_Y,
                           CARD_W - POWER_W - 12, CTRL_H, ID_POWER_LABEL);
}

/* список мониторов в строке таблицы */
static void build_monitor(const row_def *r)
{
    g_nout = ff_output_list(g_names, MAX_OUTPUTS);
    /* монитор из настроек пропал (отключили): окно показывает нулевой, настройки остальных остаются в файле */
    if (ui.app->cfg.monitor >= g_nout) {
        ui.app->cfg.monitor = 0;
    }
    ui.monitor = child(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0,
                       MARGIN + COL_SLIDER, row_y(r) + 2, CARD_W - COL_SLIDER - COL_LABEL, 220,
                       ID_MONITOR);
}

void ctl_build(void)
{
    int i;

    for (i = 0; i < NROWS; i++) {
        if (ROWS[i].idx <= IDX_SHARPEN) {
            build_row(i);
        } else if (ROWS[i].idx == IDX_MONITOR) {
            build_monitor(&ROWS[i]);
        }
    }
    build_button(S_BTN_PATTERN, 0, BTN1_Y, ID_PATTERN);
    build_button(S_BTN_RESET, 1, BTN1_Y, ID_RESET);
    build_button(S_BTN_PROGRAM, 2, BTN1_Y, ID_PROGRAM);
    build_button(S_BTN_PROFILE_LOAD, 0, BTN2_Y, ID_PROF_LOAD);
    build_button(S_BTN_PROFILE_SAVE, 1, BTN2_Y, ID_PROF_SAVE);
    build_button(S_BTN_SAVE, 2, BTN2_Y, ID_SAVE);
    build_power();
    ui.about = child(L"BUTTON", L"?", BS_OWNERDRAW | WS_TABSTOP, 0, MARGIN + CARD_W - ABOUT_D, STATUS_Y - 2, ABOUT_D,
                     ABOUT_D, ID_ABOUT);
}

static BOOL CALLBACK restyle_one(HWND c, LPARAM unused)
{
    wchar_t cls[16];

    (void)unused;
    SendMessageW(c, WM_SETFONT, (WPARAM)paint_font(), TRUE);
    GetClassNameW(c, cls, 16);
    if (wcscmp(cls, L"Button") == 0) {
        theme_apply_control(c, 0);
    } else if (wcscmp(cls, L"Edit") == 0 || wcscmp(cls, L"ComboBox") == 0
               || wcscmp(cls, L"ListBox") == 0) {
        theme_apply_control(c, 1);
    }
    return TRUE;
}

void ctl_restyle_window(HWND w)
{
    theme_apply_window(w);
    EnumChildWindows(w, restyle_one, 0);
    RedrawWindow(w, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

void ctl_restyle(void)
{
    ctl_restyle_window(ui.win);
}

void ctl_rebuild(void)
{
    HWND c;

    while ((c = GetWindow(ui.win, GW_CHILD)) != NULL) {
        DestroyWindow(c);
    }
    ctl_build();
    ctl_sync();
    ctl_restyle();
}
