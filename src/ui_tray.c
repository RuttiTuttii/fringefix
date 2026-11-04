#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "gfx.h"
#include "i18n.h"
#include "pattern.h"
#include "resource.h"
#include "ui.h"
#include "version.h"

#define HK_TOGGLE 1
#define HK_QUIT 2
#define MAX_PROFILES 24

enum { IDM_SETTINGS = 1, IDM_TOGGLE, IDM_PATTERN, IDM_UPDATE, IDM_QUIT, IDM_PROFILE = 100 };

static const wchar_t CLASS_NAME[] = L"fringefix_main";
static UINT g_taskbar_created;
static HICON g_icon_on;
static HICON g_icon_off;
/* имена профилей на момент показа меню: по номеру пункта находим файл */
static wchar_t g_profiles[MAX_PROFILES][PROFILE_NAME_LEN];

void ui_utf8(const char *s, wchar_t *out, int n)
{
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, out, n) == 0) {
        out[0] = L'\0';
    }
    out[n - 1] = L'\0';
}

static HICON load_icon(HINSTANCE inst, int id)
{
    return (HICON)LoadImageW(inst, MAKEINTRESOURCEW(id), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                             GetSystemMetrics(SM_CYSMICON), 0);
}

static void tray_notify(ff_app *a, DWORD msg)
{
    NOTIFYICONDATAW nid;

    memset(&nid, 0, sizeof nid);
    nid.cbSize = sizeof nid;
    nid.hWnd = a->hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_APP_TRAY;
    nid.hIcon = a->enabled && !a->suspended ? g_icon_on : g_icon_off;
    _snwprintf(nid.szTip, 128, L"fringefix %ls: %ls", L"" FF_VERSION,
               ff_tr(!a->enabled ? S_TIP_OFF : (a->suspended ? S_TIP_PAUSED : S_TIP_ON)));
    nid.szTip[127] = L'\0';
    Shell_NotifyIconW(msg, &nid);
}

static void balloon(ff_app *a, const wchar_t *title, const wchar_t *text)
{
    NOTIFYICONDATAW nid;

    memset(&nid, 0, sizeof nid);
    nid.cbSize = sizeof nid;
    nid.hWnd = a->hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_ERROR;
    wcsncpy(nid.szInfoTitle, title, 63);
    wcsncpy(nid.szInfo, text, 255);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

/* подменю профилей: по одному пункту на файл из папки профилей */
static HMENU profiles_menu(ff_app *a)
{
    HMENU sub = CreatePopupMenu();
    int n = app_profile_list(a, g_profiles, MAX_PROFILES);
    int i;

    for (i = 0; i < n; i++) {
        AppendMenuW(sub, MF_STRING, IDM_PROFILE + i, g_profiles[i]);
    }
    if (n == 0) {
        AppendMenuW(sub, MF_STRING | MF_GRAYED, 0, ff_tr(S_MENU_NO_PROFILES));
    }
    return sub;
}

static void show_menu(ff_app *a)
{
    HMENU m = CreatePopupMenu();
    POINT pt;

    AppendMenuW(m, MF_STRING, IDM_SETTINGS, ff_tr(S_MENU_SETTINGS));
    AppendMenuW(m, MF_STRING, IDM_TOGGLE, ff_tr(a->enabled ? S_MENU_DISABLE : S_MENU_ENABLE));
    AppendMenuW(m, MF_POPUP, (UINT_PTR)profiles_menu(a), ff_tr(S_MENU_PROFILES));
    AppendMenuW(m, MF_STRING, IDM_PATTERN, ff_tr(S_MENU_PATTERN));
    AppendMenuW(m, MF_STRING, IDM_UPDATE, ff_tr(S_MENU_UPDATES));
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_QUIT, ff_tr(S_MENU_QUIT));
    GetCursorPos(&pt);
    /* без SetForegroundWindow меню не закрывается по клику мимо */
    SetForegroundWindow(a->hwnd);
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, a->hwnd, NULL);
    PostMessageW(a->hwnd, WM_NULL, 0, 0);
    /* уничтожение главного меню уничтожает и подменю */
    DestroyMenu(m);
}

static void toggle(ff_app *a)
{
    app_set_enabled(a, !a->enabled);
    tray_notify(a, NIM_MODIFY);
}

static void check_updates(ff_app *a)
{
    if (!FF_REPO[0]) {
        MessageBoxW(NULL, ff_tr(S_NO_REPO), L"fringefix", MB_ICONINFORMATION | MB_TOPMOST);
        return;
    }
    ui_settings_set_status(ff_tr(S_CHECKING));
    update_check_async(a->hwnd, FF_REPO, 1);
}

static void load_profile(ff_app *a, int n)
{
    wchar_t path[MAX_PATH];

    app_profile_dir(a, path);
    if (wcslen(path) + wcslen(g_profiles[n]) + 7 >= MAX_PATH) {
        return;
    }
    wcscat(path, L"\\");
    wcscat(path, g_profiles[n]);
    wcscat(path, L".json");
    if (app_profile_load(a, path) == 0) {
        ui_settings_refresh();
    } else {
        balloon(a, L"fringefix", ff_tr(S_PROFILE_BAD));
    }
}

static void on_command(ff_app *a, int id)
{
    if (id >= IDM_PROFILE && id < IDM_PROFILE + MAX_PROFILES) {
        load_profile(a, id - IDM_PROFILE);
        return;
    }
    switch (id) {
    case IDM_SETTINGS:
        ui_settings_show(a);
        break;
    case IDM_TOGGLE:
        toggle(a);
        break;
    case IDM_PATTERN:
        pattern_show(a->inst, a->cfg.monitor);
        break;
    case IDM_UPDATE:
        check_updates(a);
        break;
    case IDM_QUIT:
        DestroyWindow(a->hwnd);
        break;
    }
}

/* поток захвата не смог стартовать: оверлей уже выключен им, показываем причину */
static void on_gfx_error(ff_app *a, char *msg)
{
    wchar_t text[GFX_ERR_LEN];

    ui_utf8(msg, text, GFX_ERR_LEN);
    free(msg);
    tray_notify(a, NIM_MODIFY);
    balloon(a, ff_tr(S_START_FAILED_TITLE), text);
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    ff_app *a = (ff_app *)GetWindowLongPtrW(h, GWLP_USERDATA);

    switch (m) {
    case WM_CREATE:
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW *)l)->lpCreateParams);
        return 0;
    case WM_HOTKEY:
        if (w == HK_TOGGLE) {
            toggle(a);
        } else if (w == HK_QUIT) {
            DestroyWindow(h);
        }
        return 0;
    case WM_APP_TRAY:
        if (l == WM_LBUTTONDBLCLK) {
            ui_settings_show(a);
        } else if (l == WM_RBUTTONUP) {
            show_menu(a);
        }
        return 0;
    case WM_COMMAND:
        on_command(a, LOWORD(w));
        return 0;
    case WM_APP_UPDATE_INFO:
        ui_update_on_info(a, (ff_update_info *)l);
        return 0;
    case WM_APP_RELEASE_LIST:
        ui_versions_on_list(a, (ff_release_list *)l);
        return 0;
    case WM_APP_INSTALL_DONE:
        ui_update_on_done(a, (ff_install_result *)l);
        return 0;
    case WM_APP_GFX_ERROR:
        on_gfx_error(a, (char *)l);
        return 0;
    case WM_APP_STATE:
        tray_notify(a, NIM_MODIFY);
        return 0;
    case WM_DESTROY: {
        NOTIFYICONDATAW nid;

        memset(&nid, 0, sizeof nid);
        nid.cbSize = sizeof nid;
        nid.hWnd = h;
        nid.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        UnregisterHotKey(h, HK_TOGGLE);
        UnregisterHotKey(h, HK_QUIT);
        PostQuitMessage(0);
        return 0;
    }
    }
    /* проводник перезапустился и потерял значки: добавляем свой заново */
    if (m == g_taskbar_created && a) {
        tray_notify(a, NIM_ADD);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int ui_tray_create(ff_app *a)
{
    WNDCLASSW wc;

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = a->inst;
    wc.lpszClassName = CLASS_NAME;
    if (!RegisterClassW(&wc)) {
        return -1;
    }
    g_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    g_icon_on = load_icon(a->inst, IDI_APP);
    g_icon_off = load_icon(a->inst, IDI_OFF);

    /* не message-only окно: такое не получает TaskbarCreated */
    a->hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, CLASS_NAME, L"fringefix", WS_POPUP, 0, 0, 0, 0,
                              NULL, NULL, a->inst, a);
    if (!a->hwnd) {
        return -1;
    }
    RegisterHotKey(a->hwnd, HK_TOGGLE, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'F');
    RegisterHotKey(a->hwnd, HK_QUIT, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'Q');
    tray_notify(a, NIM_ADD);
    return 0;
}
