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

static float *row_value(ff_config *c, int idx)
{
    return idx == IDX_SHARPEN ? &c->sharpen : &c->shift[idx / 2][idx % 2];
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

    _snwprintf(t, EDIT_CHARS, L"%.2f", (double)*row_value(&ui.app->cfg, idx));
    t[EDIT_CHARS - 1] = L'\0';
    ui.sync++;
    SetWindowTextW(ui.edit[idx], t);
    ui.sync--;
}

void ctl_sync(void)
{
    int i;

    for (i = 0; i <= IDX_SHARPEN; i++) {
        SendMessageW(ui.slider[i], FFS_SETPOS, (WPARAM)to_pos(*row_value(&ui.app->cfg, i)), 0);
        ctl_edit_show(i);
    }
    SendMessageW(ui.monitor, CB_SETCURSEL, (WPARAM)ui.app->cfg.monitor, 0);
    SendMessageW(ui.autopause, CB_SETCURSEL, (WPARAM)ui.app->cfg.auto_pause, 0);
    SendMessageW(ui.gameaction, CB_SETCURSEL, (WPARAM)ui.app->cfg.game_action, 0);
    SendMessageW(ui.check, BM_SETCHECK,
                 ui.app->cfg.check_updates ? BST_CHECKED : BST_UNCHECKED, 0);
}

void ctl_slider_changed(int idx)
{
    int pos = (int)SendMessageW(ui.slider[idx], FFS_GETPOS, 0, 0);

    *row_value(&ui.app->cfg, idx) = (float)pos / 100.0f;
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
    *row_value(&ui.app->cfg, idx) = (float)d;
    SendMessageW(ui.slider[idx], FFS_SETPOS, (WPARAM)to_pos((float)d), 0);
    app_apply(ui.app);
}

void ctl_reset(void)
{
    memset(ui.app->cfg.shift, 0, sizeof ui.app->cfg.shift);
    ui.app->cfg.sharpen = 0.0f;
    ctl_sync();
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

/* выпадающий список в строке таблицы: монитор или автопауза */
static HWND build_combo(const row_def *r, int id)
{
    return child(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0,
                 MARGIN + COL_SLIDER, row_y(r) + 2, CARD_W - COL_SLIDER - COL_LABEL, 220, id);
}

static void build_combos(void)
{
    wchar_t names[MAX_OUTPUTS][OUTPUT_NAME_LEN];
    int n = ff_output_list(names, MAX_OUTPUTS);
    int i;

    for (i = 0; i < NROWS; i++) {
        if (ROWS[i].idx == IDX_MONITOR) {
            ui.monitor = build_combo(&ROWS[i], ID_MONITOR);
        } else if (ROWS[i].idx == IDX_AUTOPAUSE) {
            ui.autopause = build_combo(&ROWS[i], ID_AUTOPAUSE);
        } else if (ROWS[i].idx == IDX_GAMEACTION) {
            ui.gameaction = build_combo(&ROWS[i], ID_GAMEACTION);
        }
    }
    for (i = 0; i < n; i++) {
        SendMessageW(ui.monitor, CB_ADDSTRING, 0, (LPARAM)names[i]);
    }
    SendMessageW(ui.autopause, CB_ADDSTRING, 0, (LPARAM)ff_tr(S_AP_OFF));
    SendMessageW(ui.autopause, CB_ADDSTRING, 0, (LPARAM)ff_tr(S_AP_FULLSCREEN));
    SendMessageW(ui.autopause, CB_ADDSTRING, 0, (LPARAM)ff_tr(S_AP_3D));
    SendMessageW(ui.gameaction, CB_ADDSTRING, 0, (LPARAM)ff_tr(S_GA_PAUSE));
    SendMessageW(ui.gameaction, CB_ADDSTRING, 0, (LPARAM)ff_tr(S_GA_LIGHT));
}

void ctl_build(void)
{
    int i;

    for (i = 0; i < NROWS; i++) {
        if (ROWS[i].idx <= IDX_SHARPEN) {
            build_row(i);
        }
    }
    build_combos();
    ui.check = child(L"BUTTON", ff_tr(S_CHECK_UPDATES), BS_AUTOCHECKBOX | WS_TABSTOP, 0, MARGIN,
                     CHECK_Y, CARD_W, 22, ID_CHECK);

    build_button(S_BTN_WIZARD, 0, BTN1_Y, ID_WIZARD);
    build_button(S_BTN_PATTERN, 1, BTN1_Y, ID_PATTERN);
    build_button(S_BTN_RESET, 2, BTN1_Y, ID_RESET);
    build_button(S_BTN_PROFILE_LOAD, 0, BTN2_Y, ID_PROF_LOAD);
    build_button(S_BTN_PROFILE_SAVE, 1, BTN2_Y, ID_PROF_SAVE);
    build_button(S_BTN_VERSIONS, 2, BTN2_Y, ID_VERSIONS);
    build_button(S_BTN_UPDATES, 0, BTN3_Y, ID_UPDATE);
    build_button(S_BTN_SAVE, 2, BTN3_Y, ID_SAVE);
}

static BOOL CALLBACK restyle_one(HWND c, LPARAM unused)
{
    wchar_t cls[16];

    (void)unused;
    SendMessageW(c, WM_SETFONT, (WPARAM)paint_font(), TRUE);
    GetClassNameW(c, cls, 16);
    if (wcscmp(cls, L"Button") == 0) {
        theme_apply_control(c, 0);
    } else if (wcscmp(cls, L"Edit") == 0 || wcscmp(cls, L"ComboBox") == 0) {
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
