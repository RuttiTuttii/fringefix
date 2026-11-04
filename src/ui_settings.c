#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "i18n.h"
#include "pattern.h"
#include "resource.h"
#include "slider.h"
#include "theme.h"
#include "ui.h"
#include "ui_paint.h"
#include "ui_state.h"
#include "version.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#define STATUS_CHARS 256

static const wchar_t CLASS_NAME[] = L"fringefix_settings";
static wchar_t g_status[STATUS_CHARS];

ui_state ui = { NULL, NULL, { NULL }, { NULL }, NULL, NULL, 96, 0 };

static void set_status(const wchar_t *text)
{
    RECT r;

    wcsncpy(g_status, text, STATUS_CHARS - 1);
    g_status[STATUS_CHARS - 1] = L'\0';
    if (ui.win) {
        SetRect(&r, 0, S(STATUS_Y) - 2, S(CLIENT_W), S(STATUS_Y) + S(22));
        InvalidateRect(ui.win, &r, FALSE);
    }
}

static void set_status_fmt(str_id id, const wchar_t *arg)
{
    wchar_t t[STATUS_CHARS];

    _snwprintf(t, STATUS_CHARS, ff_tr(id), arg);
    t[STATUS_CHARS - 1] = L'\0';
    set_status(t);
}

static void on_profile(int save)
{
    wchar_t name[PROFILE_NAME_LEN];
    int rc = save ? ui_profile_save_dialog(ui.win, ui.app, name)
                  : ui_profile_load_dialog(ui.win, ui.app, name);

    if (rc == 1) {
        return;
    }
    if (save && rc == 0) {
        set_status_fmt(S_PROFILE_SAVED, name);
    } else if (save) {
        set_status(ff_tr(S_PROFILE_SAVE_FAILED));
    } else if (rc == 0) {
        ctl_sync();
        set_status_fmt(S_PROFILE_LOADED, name);
    } else {
        set_status(ff_tr(S_PROFILE_BAD));
    }
}

static void on_edit(int id, int code)
{
    if (code == EN_CHANGE && !ui.sync) {
        ctl_edit_changed(id - ID_EDIT);
    } else if (code == EN_KILLFOCUS) {
        ctl_edit_show(id - ID_EDIT);
    }
}

/* Enter в поле: привести текст к виду значения и выделить его для следующего ввода */
static void on_enter(void)
{
    int i;

    for (i = 0; i <= IDX_SHARPEN; i++) {
        if (GetFocus() == ui.edit[i]) {
            ctl_edit_show(i);
            SendMessageW(ui.edit[i], EM_SETSEL, 0, -1);
        }
    }
}

static void on_command(int id, int code)
{
    ff_config *c = &ui.app->cfg;

    if (id >= ID_SLIDER && id <= ID_SLIDER + IDX_SHARPEN && code == FFN_CHANGED) {
        ctl_slider_changed(id - ID_SLIDER);
    } else if (id >= ID_EDIT && id <= ID_EDIT + IDX_SHARPEN) {
        on_edit(id, code);
    } else if (id == IDOK) {
        on_enter();
    } else if (id == ID_MONITOR && code == CBN_SELCHANGE) {
        c->monitor = (int)SendMessageW(ui.monitor, CB_GETCURSEL, 0, 0);
        app_reinit(ui.app);
    } else if (id == ID_CHECK) {
        c->check_updates = SendMessageW(ui.check, BM_GETCHECK, 0, 0) == BST_CHECKED;
    } else if (id == ID_PROF_LOAD || id == ID_PROF_SAVE) {
        on_profile(id == ID_PROF_SAVE);
    } else if (id == ID_PATTERN) {
        pattern_show(ui.app->inst, c->monitor);
    } else if (id == ID_RESET) {
        ctl_reset();
    } else if (id == ID_UPDATE && FF_REPO[0]) {
        set_status(ff_tr(S_CHECKING));
        update_check_async(ui.app->hwnd, FF_REPO, 1);
    } else if (id == ID_UPDATE) {
        set_status(ff_tr(S_NO_REPO));
    } else if (id == ID_SAVE) {
        set_status(ff_tr(app_save(ui.app) == 0 ? S_SAVED : S_SAVE_FAILED));
    }
}

static LRESULT on_ctlcolor(UINT m, HDC dc, HWND ctl)
{
    const ff_palette *p = theme_get();
    int field = m == WM_CTLCOLOREDIT || m == WM_CTLCOLORLISTBOX || ctl == ui.monitor;

    SetTextColor(dc, p->fg);
    SetBkColor(dc, field ? p->field : p->bg);
    return (LRESULT)(field ? paint_field() : paint_bg());
}

static LRESULT on_dpi_changed(HWND h, WPARAM w, LPARAM l)
{
    const RECT *r = (const RECT *)l;

    ui.dpi = HIWORD(w);
    paint_init(ui.dpi);
    ctl_rebuild();
    SetWindowPos(h, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    return 0;
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT:
        paint_window(h, ui.dpi, g_status);
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
        return on_ctlcolor(m, (HDC)w, (HWND)l);
    case WM_SETTINGCHANGE:
        /* сменилась тема приложений Windows: перекрасить окно на лету */
        if (l && wcscmp((const wchar_t *)l, L"ImmersiveColorSet") == 0) {
            theme_refresh();
            paint_init(ui.dpi);
            ctl_restyle();
        }
        return 0;
    case WM_DPICHANGED:
        return on_dpi_changed(h, w, l);
    case WM_CLOSE:
        /* окно только прячется, чтобы не пересоздавать элементы */
        ShowWindow(h, SW_HIDE);
        app_set_fast(ui.app, 0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void set_icons(void)
{
    SendMessageW(ui.win, WM_SETICON, ICON_BIG,
                 (LPARAM)LoadImageW(ui.app->inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0));
    SendMessageW(ui.win, WM_SETICON, ICON_SMALL,
                 (LPARAM)LoadImageW(ui.app->inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
}

static void create_window(void)
{
    WNDCLASSW wc;
    HDC dc = GetDC(NULL);
    DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT r;

    ui.dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);
    theme_refresh();
    slider_register(ui.app->inst);

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = ui.app->inst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    RegisterClassW(&wc);

    SetRect(&r, 0, 0, S(CLIENT_W), S(CLIENT_H));
    AdjustWindowRect(&r, style, FALSE);
    /* поверх паттерна, который тоже занимает весь монитор */
    ui.win = CreateWindowExW(WS_EX_TOPMOST | WS_EX_CONTROLPARENT, CLASS_NAME,
                             ff_tr(S_WINDOW_TITLE), style, CW_USEDEFAULT, CW_USEDEFAULT,
                             r.right - r.left, r.bottom - r.top, NULL, NULL, ui.app->inst, NULL);
    paint_init(ui.dpi);
    set_icons();
    ctl_build();
    ctl_restyle();
    set_status_fmt(S_VERSION, L"" FF_VERSION);
}

void ui_settings_show(ff_app *a)
{
    ui.app = a;
    if (!ui.win) {
        create_window();
    }
    ctl_sync();
    ShowWindow(ui.win, SW_SHOW);
    SetForegroundWindow(ui.win);
    app_set_fast(a, 1);
}

void ui_settings_refresh(void)
{
    if (ui.win) {
        ctl_sync();
    }
}

void ui_settings_set_status(const wchar_t *text)
{
    set_status(text);
}

int ui_settings_translate(MSG *m)
{
    return ui.win && IsWindowVisible(ui.win) && IsDialogMessageW(ui.win, m);
}
