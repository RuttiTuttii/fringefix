#include <windows.h>
#include <commctrl.h>
#include <math.h>
#include <stdio.h>

#include "output.h"
#include "pattern.h"
#include "ui.h"
#include "version.h"

#define MAX_OUTPUTS 8
#define SHARPEN_IDX 6
#define CLIENT_W 476
#define CLIENT_H 470

enum {
    ID_SLIDER = 100,
    ID_VALUE = 200,
    ID_MONITOR = 300,
    ID_CHECK,
    ID_PATTERN,
    ID_RESET,
    ID_UPDATE,
    ID_SAVE,
    ID_STATUS,
};

/* idx 0..5 это shift[idx / 2][idx % 2], 6 это резкость; y в пикселях при 96 dpi */
typedef struct {
    const wchar_t *label;
    int idx;
    int y;
} row_t;

static const row_t ROWS[] = {
    { L"Зелёный, по вертикали", 3, 36 },
    { L"Красный, по горизонтали", 0, 68 },
    { L"Синий, по горизонтали", 4, 100 },
    { L"Зелёный, по горизонтали", 2, 164 },
    { L"Красный, по вертикали", 1, 196 },
    { L"Синий, по вертикали", 5, 228 },
    { L"Резкость", SHARPEN_IDX, 292 },
};
#define NROWS ((int)(sizeof ROWS / sizeof ROWS[0]))

static const wchar_t CLASS_NAME[] = L"fringefix_settings";
static ff_app *g_app;
static HWND g_win;
static HWND g_slider[NROWS];
static HWND g_value[NROWS];
static HWND g_monitor;
static HWND g_check;
static HWND g_status;
static HFONT g_font;
static HFONT g_bold;
static int g_dpi = 96;

#define S(v) MulDiv((v), g_dpi, 96)

static float *row_value(ff_config *c, int idx)
{
    return idx == SHARPEN_IDX ? &c->sharpen : &c->shift[idx / 2][idx % 2];
}

static void show_value(int i)
{
    wchar_t t[32];
    float v = *row_value(&g_app->cfg, ROWS[i].idx);

    if (ROWS[i].idx == SHARPEN_IDX) {
        _snwprintf(t, 32, L"%.2f", (double)v);
    } else {
        _snwprintf(t, 32, L"%+.2f px", (double)v);
    }
    t[31] = L'\0';
    SetWindowTextW(g_value[i], t);
}

/* значения конфига в элементы окна */
static void sync_controls(void)
{
    int i;

    for (i = 0; i < NROWS; i++) {
        float v = *row_value(&g_app->cfg, ROWS[i].idx);

        SendMessageW(g_slider[i], TBM_SETPOS, TRUE, lroundf(v * 100.0f));
        show_value(i);
    }
    SendMessageW(g_monitor, CB_SETCURSEL, (WPARAM)g_app->cfg.monitor, 0);
    SendMessageW(g_check, BM_SETCHECK, g_app->cfg.check_updates ? BST_CHECKED : BST_UNCHECKED, 0);
}

static HWND add(const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h,
                int id, HFONT font)
{
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h),
                             g_win, (HMENU)(INT_PTR)id, g_app->inst, NULL);

    SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    return c;
}

static void add_section(const wchar_t *title, int y)
{
    add(L"STATIC", title, SS_LEFT, 16, y, 440, 20, -1, g_bold);
}

static void add_row(int i)
{
    int lo = ROWS[i].idx == SHARPEN_IDX ? 0 : -200;
    int y = ROWS[i].y;

    add(L"STATIC", ROWS[i].label, SS_LEFT, 16, y + 4, 166, 20, -1, g_font);
    g_slider[i] = add(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, 186, y, 210, 28,
                      ID_SLIDER + i, g_font);
    SendMessageW(g_slider[i], TBM_SETRANGE, TRUE, MAKELONG(lo, ROWS[i].idx == SHARPEN_IDX ? 100 : 200));
    SendMessageW(g_slider[i], TBM_SETPAGESIZE, 0, 10);
    g_value[i] = add(L"STATIC", L"", SS_RIGHT, 402, y + 4, 64, 20, ID_VALUE + i, g_font);
}

static void create_controls(void)
{
    wchar_t names[MAX_OUTPUTS][OUTPUT_NAME_LEN];
    int n = ff_output_list(names, MAX_OUTPUTS);
    int i;

    add_section(L"Основные", 12);
    add_section(L"Дополнительные", 140);
    add_section(L"Прочее", 268);
    for (i = 0; i < NROWS; i++) {
        add_row(i);
    }

    add(L"STATIC", L"Монитор", SS_LEFT, 16, 334, 166, 20, -1, g_font);
    g_monitor = add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 186, 330, 280, 200,
                    ID_MONITOR, g_font);
    for (i = 0; i < n; i++) {
        SendMessageW(g_monitor, CB_ADDSTRING, 0, (LPARAM)names[i]);
    }
    g_check = add(L"BUTTON", L"Проверять обновления при запуске", BS_AUTOCHECKBOX | WS_TABSTOP, 16,
                  366, 440, 22, ID_CHECK, g_font);
    g_status = add(L"STATIC", L"", SS_LEFTNOWORDWRAP, 16, 396, 444, 22, ID_STATUS, g_font);

    add(L"BUTTON", L"Паттерн", BS_PUSHBUTTON | WS_TABSTOP, 16, 428, 108, 30, ID_PATTERN, g_font);
    add(L"BUTTON", L"Сбросить", BS_PUSHBUTTON | WS_TABSTOP, 132, 428, 108, 30, ID_RESET, g_font);
    add(L"BUTTON", L"Обновления", BS_PUSHBUTTON | WS_TABSTOP, 248, 428, 108, 30, ID_UPDATE, g_font);
    add(L"BUTTON", L"Сохранить", BS_DEFPUSHBUTTON | WS_TABSTOP, 364, 428, 96, 30, ID_SAVE, g_font);
}

static void on_command(int id, int code)
{
    ff_config *c = &g_app->cfg;

    switch (id) {
    case ID_MONITOR:
        if (code == CBN_SELCHANGE) {
            c->monitor = (int)SendMessageW(g_monitor, CB_GETCURSEL, 0, 0);
            app_reinit(g_app);
        }
        break;
    case ID_CHECK:
        c->check_updates = SendMessageW(g_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
        break;
    case ID_PATTERN:
        pattern_show(g_app->inst, c->monitor);
        break;
    case ID_RESET:
        memset(c->shift, 0, sizeof c->shift);
        c->sharpen = 0.0f;
        sync_controls();
        app_apply(g_app);
        break;
    case ID_UPDATE:
        if (FF_REPO[0]) {
            ui_settings_set_status(L"Проверяю обновления...");
            update_check_async(g_app->hwnd, FF_REPO, 1);
        } else {
            ui_settings_set_status(L"В этой сборке не указан репозиторий для обновлений");
        }
        break;
    case ID_SAVE:
        ui_settings_set_status(app_save(g_app) == 0 ? L"Сохранено" : L"Не удалось сохранить файл настроек");
        break;
    }
}

static void on_scroll(HWND src)
{
    int i;

    for (i = 0; i < NROWS; i++) {
        if (src == g_slider[i]) {
            int pos = (int)SendMessageW(src, TBM_GETPOS, 0, 0);

            *row_value(&g_app->cfg, ROWS[i].idx) = (float)pos / 100.0f;
            show_value(i);
            app_apply(g_app);
        }
    }
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_HSCROLL:
        on_scroll((HWND)l);
        return 0;
    case WM_COMMAND:
        on_command(LOWORD(w), HIWORD(w));
        return 0;
    case WM_CLOSE:
        /* окно только прячется, чтобы не пересоздавать элементы */
        ShowWindow(h, SW_HIDE);
        app_set_fast(g_app, 0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static HFONT make_font(int weight)
{
    return CreateFontW(-MulDiv(9, g_dpi, 72), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                       CLEARTYPE_QUALITY, 0, L"Segoe UI");
}

static void create_window(void)
{
    WNDCLASSW wc;
    HDC dc = GetDC(NULL);
    DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT r;

    g_dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = g_app->inst;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    RegisterClassW(&wc);

    SetRect(&r, 0, 0, S(CLIENT_W), S(CLIENT_H));
    AdjustWindowRect(&r, style, FALSE);
    /* поверх паттерна, который тоже занимает весь монитор */
    g_win = CreateWindowExW(WS_EX_TOPMOST | WS_EX_CONTROLPARENT, CLASS_NAME,
                            L"fringefix: настройки", style, CW_USEDEFAULT, CW_USEDEFAULT,
                            r.right - r.left, r.bottom - r.top, NULL, NULL, g_app->inst, NULL);
    g_font = make_font(FW_NORMAL);
    g_bold = make_font(FW_SEMIBOLD);
    create_controls();
    ui_settings_set_status(L"Версия " FF_VERSION);
}

void ui_settings_show(ff_app *a)
{
    g_app = a;
    if (!g_win) {
        create_window();
    }
    sync_controls();
    ShowWindow(g_win, SW_SHOW);
    SetForegroundWindow(g_win);
    app_set_fast(a, 1);
}

void ui_settings_set_status(const wchar_t *text)
{
    if (g_status) {
        SetWindowTextW(g_status, text);
    }
}

int ui_settings_translate(MSG *m)
{
    return g_win && IsWindowVisible(g_win) && IsDialogMessageW(g_win, m);
}
