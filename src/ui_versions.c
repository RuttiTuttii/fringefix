#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "i18n.h"
#include "resource.h"
#include "theme.h"
#include "ui.h"
#include "ui_paint.h"
#include "ui_state.h"
#include "version.h"

#define VERS_W 520
#define VERS_H 250
#define ITEM_CHARS 96
#define TEXT_CHARS 320

enum { ID_V_LIST = 600, ID_V_INSTALL, ID_V_CLOSE };

static const wchar_t CLASS_NAME[] = L"fringefix_versions";
static HWND g_win;
static HWND g_list;
static HWND g_install;
static ff_release_list *g_data;
static wchar_t g_status[TEXT_CHARS];
static int g_cur[3];
static int g_cur_ok;

static void set_status(const wchar_t *text)
{
    wcsncpy(g_status, text, TEXT_CHARS - 1);
    g_status[TEXT_CHARS - 1] = L'\0';
    if (g_win) {
        InvalidateRect(g_win, NULL, FALSE);
    }
}

/* знак сравнения выбранной версии с установленной: больше нуля новее; dev-сборка считается старее любой */
static int compare_selected(int sel)
{
    int v[3];

    if (sel < 0 || !g_data || sel >= g_data->count || ff_version_parse(g_data->rel[sel].tag, v) != 0) {
        return 0;
    }
    return g_cur_ok ? ff_tag_cmp(g_data->rel[sel].tag, FF_VERSION) : 1;
}

/* подпись кнопки зависит от выбора: установить, откатить или версия уже стоит */
static void update_button(void)
{
    int sel = (int)SendMessageW(g_list, CB_GETCURSEL, 0, 0);
    int cmp = compare_selected(sel);
    SetWindowTextW(g_install, ff_tr(cmp > 0 ? S_VER_INSTALL : (cmp < 0 ? S_VER_ROLLBACK : S_VER_SAME)));
    EnableWindow(g_install, sel >= 0 && cmp != 0);
}

static void fill_list(void)
{
    int i;
    int current = -1;

    SendMessageW(g_list, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < g_data->count; i++) {
        wchar_t tag[48];
        wchar_t item[ITEM_CHARS];
        int is_current = g_cur_ok && compare_selected(i) == 0;

        ui_utf8(g_data->rel[i].tag, tag, 48);
        _snwprintf(item, ITEM_CHARS, L"%ls%ls%ls", tag, i == 0 ? ff_tr(S_VER_LATEST) : L"",
                   is_current ? ff_tr(S_VER_CURRENT) : L"");
        item[ITEM_CHARS - 1] = L'\0';
        SendMessageW(g_list, CB_ADDSTRING, 0, (LPARAM)item);
        if (is_current) {
            current = i;
        }
    }
    SendMessageW(g_list, CB_SETCURSEL, (WPARAM)(current >= 0 ? current : 0), 0);
    EnableWindow(g_list, g_data->count > 0);
    update_button();
}

static void do_install(void)
{
    int sel = (int)SendMessageW(g_list, CB_GETCURSEL, 0, 0);
    int cmp = compare_selected(sel);
    wchar_t tag[48];
    wchar_t cur[48];
    wchar_t text[TEXT_CHARS];
    ff_update_info info;

    if (sel < 0 || cmp == 0) {
        return;
    }
    ui_utf8(g_data->rel[sel].tag, tag, 48);
    ui_utf8(FF_VERSION, cur, 48);
    _snwprintf(text, TEXT_CHARS, ff_tr(cmp > 0 ? S_VER_CONFIRM_UP : S_VER_CONFIRM_DOWN), tag, cur);
    text[TEXT_CHARS - 1] = L'\0';
    if (MessageBoxW(g_win, text, L"fringefix",
                    MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND | MB_TOPMOST) != IDYES) {
        return;
    }
    /* дальше обычная установка: хеш, подмена exe, перезапуск; версии при этом не сравниваются */
    memset(&info, 0, sizeof info);
    info.rel = g_data->rel[sel];
    info.newer = cmp > 0;
    set_status(ff_tr(S_DOWNLOADING));
    EnableWindow(g_install, FALSE);
    update_install_async(ui.app->hwnd, &info);
}

static void paint(HWND h)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    const ff_palette *p = theme_get();
    RECT r;
    RECT body;

    GetClientRect(h, &r);
    FillRect(dc, &r, paint_bg());
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, paint_font());
    SetTextColor(dc, p->fg);
    SetRect(&body, S(24), S(20), S(VERS_W - 24), S(90));
    DrawTextW(dc, ff_tr(S_VER_HINT), -1, &body, DT_WORDBREAK);
    SetTextColor(dc, p->muted);
    SetRect(&body, S(24), S(150), S(VERS_W - 24), S(190));
    DrawTextW(dc, g_status, -1, &body, DT_WORDBREAK);
    EndPaint(h, &ps);
}

static void build(void)
{
    g_list = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                             S(24), S(98), S(472), S(260), g_win, (HMENU)(INT_PTR)ID_V_LIST,
                             ui.app->inst, NULL);
    g_install = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | WS_DISABLED,
                                S(244), S(196), S(120), S(30), g_win, (HMENU)(INT_PTR)ID_V_INSTALL,
                                ui.app->inst, NULL);
    CreateWindowExW(0, L"BUTTON", ff_tr(S_VER_CLOSE), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    S(376), S(196), S(120), S(30), g_win, (HMENU)(INT_PTR)ID_V_CLOSE, ui.app->inst, NULL);
    EnableWindow(g_list, FALSE);
    ctl_restyle_window(g_win);
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
        if (LOWORD(w) == ID_V_LIST && HIWORD(w) == CBN_SELCHANGE) {
            update_button();
        } else if (LOWORD(w) == ID_V_INSTALL) {
            do_install();
        } else if (LOWORD(w) == ID_V_CLOSE) {
            DestroyWindow(h);
        }
        return 0;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
        return paint_ctlcolor(m, (HDC)w, (HWND)l, (HWND)l == g_list ? g_list : NULL);
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        g_win = NULL;
        free(g_data);
        g_data = NULL;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void ui_versions_show(ff_app *a)
{
    WNDCLASSW wc;
    DWORD style = WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    RECT r;

    ui.app = a;
    if (g_win) {
        SetForegroundWindow(g_win);
        return;
    }
    if (!FF_REPO[0]) {
        MessageBoxW(NULL, ff_tr(S_NO_REPO), L"fringefix", MB_ICONINFORMATION | MB_TOPMOST);
        return;
    }
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = a->inst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    RegisterClassW(&wc);

    g_cur_ok = ff_version_parse(FF_VERSION, g_cur) == 0;
    SetRect(&r, 0, 0, S(VERS_W), S(VERS_H));
    AdjustWindowRect(&r, style, FALSE);
    g_win = CreateWindowExW(WS_EX_TOPMOST | WS_EX_CONTROLPARENT, CLASS_NAME, ff_tr(S_VER_TITLE), style,
                            CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL,
                            NULL, a->inst, NULL);
    SendMessageW(g_win, WM_SETICON, ICON_BIG,
                 (LPARAM)LoadImageW(a->inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));
    SendMessageW(g_win, WM_SETICON, ICON_SMALL,
                 (LPARAM)LoadImageW(a->inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
    build();
    set_status(ff_tr(S_VER_LOADING));
    ShowWindow(g_win, SW_SHOW);
    SetForegroundWindow(g_win);
    update_list_async(a->hwnd, FF_REPO);
}

/* список пришёл из фонового потока; окно могли закрыть раньше, тогда он просто освобождается */
void ui_versions_on_list(ff_app *a, ff_release_list *list)
{
    (void)a;
    if (!g_win) {
        free(list);
        return;
    }
    free(g_data);
    g_data = list;
    if (list->err[0] && list->count == 0) {
        wchar_t err[200];
        wchar_t text[TEXT_CHARS];

        ui_utf8(list->err, err, 200);
        _snwprintf(text, TEXT_CHARS, ff_tr(S_VER_FAILED), err);
        text[TEXT_CHARS - 1] = L'\0';
        set_status(text);
        return;
    }
    set_status(L"");
    fill_list();
}
