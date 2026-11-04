#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "i18n.h"
#include "layout.h"
#include "pattern.h"
#include "resource.h"
#include "slider.h"
#include "theme.h"
#include "ui.h"
#include "ui_paint.h"
#include "ui_state.h"

#define PAGES 3
#define WIZ_W 520
#define WIZ_H 360
#define K_LIMIT 3.0
#define EDIT_CHARS 24

enum { ID_W_TYPE = 500, ID_W_SLIDER, ID_W_EDIT, ID_W_PATTERN, ID_W_BACK, ID_W_NEXT, ID_W_CANCEL };

static const wchar_t CLASS_NAME[] = L"fringefix_wizard";
static HWND g_win;
static HWND g_slider;
static HWND g_edit;
static HWND g_type;
static int g_page;
static int g_sync;
static ff_layout g_layout = FF_LAYOUT_QD_TRIANGLE;
static double g_k[2] = { 1.0, 1.0 };
/* сдвиги до начала мастера: Отмена возвращает их */
static float g_backup[3][2];

static double *cur_k(void)
{
    return &g_k[g_page == 1 ? 1 : 0];
}

/* считает сдвиги по раскладке и силам, кладёт в настройки и показывает на экране */
static void apply(void)
{
    ff_layout_shifts(g_layout, g_k[0], g_k[1], ui.app->cfg.shift);
    app_apply(ui.app);
    ui_settings_refresh();
}

static void show_edit(void)
{
    wchar_t t[EDIT_CHARS];

    _snwprintf(t, EDIT_CHARS, L"%.2f", *cur_k());
    t[EDIT_CHARS - 1] = L'\0';
    g_sync++;
    SetWindowTextW(g_edit, t);
    g_sync--;
}

static void edit_changed(void)
{
    wchar_t t[EDIT_CHARS];
    wchar_t *end;
    wchar_t *c;
    double d;

    GetWindowTextW(g_edit, t, EDIT_CHARS);
    for (c = t; *c; c++) {
        if (*c == L',') {
            *c = L'.';
        }
    }
    d = wcstod(t, &end);
    if (end == t || *end != L'\0' || !isfinite(d)) {
        return;
    }
    *cur_k() = d < -K_LIMIT ? -K_LIMIT : (d > K_LIMIT ? K_LIMIT : d);
    SendMessageW(g_slider, FFS_SETPOS, (WPARAM)lround(*cur_k() * 100.0), 0);
    apply();
}

static HWND child(const wchar_t *cls, const wchar_t *text, DWORD style, DWORD ex, int x, int y,
                  int w, int h, int id)
{
    return CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h),
                           g_win, (HMENU)(INT_PTR)id, ui.app->inst, NULL);
}

static void build_page(void)
{
    HWND c;

    while ((c = GetWindow(g_win, GW_CHILD)) != NULL) {
        DestroyWindow(c);
    }
    g_slider = g_edit = g_type = NULL;
    if (g_page == 0) {
        g_type = child(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, 24, 196,
                       472, 120, ID_W_TYPE);
        SendMessageW(g_type, CB_ADDSTRING, 0, (LPARAM)ff_tr(S_WIZ_TYPE_QD));
        SendMessageW(g_type, CB_ADDSTRING, 0, (LPARAM)ff_tr(S_WIZ_TYPE_NONE));
        SendMessageW(g_type, CB_SETCURSEL, g_layout == FF_LAYOUT_QD_TRIANGLE ? 0 : 1, 0);
    } else {
        g_slider = child(FF_SLIDER_CLASS, L"", WS_TABSTOP, 0, 190, 196, 220, 28, ID_W_SLIDER);
        SendMessageW(g_slider, FFS_SETRANGE, (WPARAM)-(int)(K_LIMIT * 100), (int)(K_LIMIT * 100));
        SendMessageW(g_slider, FFS_SETPOS, (WPARAM)lround(*cur_k() * 100.0), 0);
        g_edit = child(L"EDIT", L"", ES_RIGHT | ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, 420,
                       198, 70, 24, ID_W_EDIT);
        show_edit();
    }
    child(L"BUTTON", ff_tr(S_BTN_PATTERN), BS_PUSHBUTTON | WS_TABSTOP, 0, 24, 246, 160, 30,
          ID_W_PATTERN);
    child(L"BUTTON", ff_tr(S_WIZ_BACK), BS_PUSHBUTTON | WS_TABSTOP | (g_page == 0 ? WS_DISABLED : 0),
          0, 112, 310, 120, 30, ID_W_BACK);
    child(L"BUTTON", ff_tr(g_page == PAGES - 1 ? S_WIZ_FINISH : S_WIZ_NEXT),
          BS_PUSHBUTTON | WS_TABSTOP, 0, 244, 310, 120, 30, ID_W_NEXT);
    child(L"BUTTON", ff_tr(S_WIZ_CANCEL), BS_PUSHBUTTON | WS_TABSTOP, 0, 376, 310, 120, 30,
          ID_W_CANCEL);
    ctl_restyle_window(g_win);
}

static void paint(HWND h)
{
    static const str_id TITLES[PAGES] = { S_WIZ_T0, S_WIZ_T1, S_WIZ_T2 };
    static const str_id BODIES[PAGES] = { S_WIZ_B0, S_WIZ_B1, S_WIZ_B2 };
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    const ff_palette *p = theme_get();
    RECT r;
    RECT body;
    wchar_t step[48];

    GetClientRect(h, &r);
    FillRect(dc, &r, paint_bg());
    SetBkMode(dc, TRANSPARENT);

    SelectObject(dc, paint_font());
    _snwprintf(step, 48, ff_tr(S_WIZ_STEP), g_page + 1);
    step[47] = L'\0';
    SetTextColor(dc, p->muted);
    TextOutW(dc, S(24), S(18), step, (int)wcslen(step));
    SelectObject(dc, paint_bold());
    SetTextColor(dc, p->accent);
    TextOutW(dc, S(24), S(38), ff_tr(TITLES[g_page]), (int)wcslen(ff_tr(TITLES[g_page])));

    SelectObject(dc, paint_font());
    SetTextColor(dc, p->fg);
    SetRect(&body, S(24), S(74), S(WIZ_W - 24), S(180));
    DrawTextW(dc, ff_tr(BODIES[g_page]), -1, &body, DT_WORDBREAK);
    if (g_page > 0) {
        const wchar_t *label = ff_tr(g_page == 1 ? S_WIZ_STRENGTH_V : S_WIZ_STRENGTH_H);

        TextOutW(dc, S(24), S(201), label, (int)wcslen(label));
    }
    EndPaint(h, &ps);
}

static void close_wizard(int keep)
{
    if (!keep) {
        memcpy(ui.app->cfg.shift, g_backup, sizeof g_backup);
        app_apply(ui.app);
        ui_settings_refresh();
    } else {
        app_save(ui.app);
    }
    pattern_close();
    app_set_fast(ui.app, 0);
    DestroyWindow(g_win);
}

static void go(int page)
{
    g_page = page;
    build_page();
    /* на шагах подгонки паттерн нужен сразу, он ляжет под окном мастера */
    if (page > 0) {
        pattern_show(ui.app->inst, ui.app->cfg.monitor);
    }
    InvalidateRect(g_win, NULL, TRUE);
}

static void on_command(int id, int code)
{
    switch (id) {
    case ID_W_TYPE:
        if (code == CBN_SELCHANGE) {
            g_layout = SendMessageW(g_type, CB_GETCURSEL, 0, 0) == 0 ? FF_LAYOUT_QD_TRIANGLE
                                                                      : FF_LAYOUT_NONE;
            apply();
        }
        break;
    case ID_W_SLIDER:
        if (code == FFN_CHANGED) {
            *cur_k() = (double)SendMessageW(g_slider, FFS_GETPOS, 0, 0) / 100.0;
            show_edit();
            apply();
        }
        break;
    case ID_W_EDIT:
        if (code == EN_CHANGE && !g_sync) {
            edit_changed();
        } else if (code == EN_KILLFOCUS) {
            show_edit();
        }
        break;
    case ID_W_PATTERN:
        pattern_show(ui.app->inst, ui.app->cfg.monitor);
        break;
    case ID_W_BACK:
        go(g_page - 1);
        break;
    case ID_W_NEXT:
        if (g_page == PAGES - 1) {
            close_wizard(1);
        } else {
            go(g_page + 1);
        }
        break;
    case ID_W_CANCEL:
        close_wizard(0);
        break;
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
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
        return paint_ctlcolor(m, (HDC)w, (HWND)l, (HWND)l == g_type ? g_type : NULL);
    case WM_CLOSE:
        close_wizard(0);
        return 0;
    case WM_DESTROY:
        g_win = NULL;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void ui_wizard_show(ff_app *a)
{
    WNDCLASSW wc;
    DWORD style = WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    RECT r;

    ui.app = a;
    if (g_win) {
        SetForegroundWindow(g_win);
        return;
    }
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = a->inst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    RegisterClassW(&wc);

    memcpy(g_backup, a->cfg.shift, sizeof g_backup);
    g_page = 0;
    g_layout = FF_LAYOUT_QD_TRIANGLE;
    g_k[0] = g_k[1] = 1.0;
    SetRect(&r, 0, 0, S(WIZ_W), S(WIZ_H));
    AdjustWindowRect(&r, style, FALSE);
    /* поверх паттерна, который закрывает весь монитор */
    g_win = CreateWindowExW(WS_EX_TOPMOST | WS_EX_CONTROLPARENT, CLASS_NAME, ff_tr(S_WIZ_TITLE),
                            style, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                            NULL, NULL, a->inst, NULL);
    SendMessageW(g_win, WM_SETICON, ICON_BIG,
                 (LPARAM)LoadImageW(a->inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0));
    SendMessageW(g_win, WM_SETICON, ICON_SMALL,
                 (LPARAM)LoadImageW(a->inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    app_set_fast(a, 1);
    /* сразу показываем расчёт по геометрии, силы стоят в единице */
    apply();
    build_page();
    ShowWindow(g_win, SW_SHOW);
    SetForegroundWindow(g_win);
}
