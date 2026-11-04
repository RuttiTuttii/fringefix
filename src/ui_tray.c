#include <windows.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "gfx.h"
#include "pattern.h"
#include "ui.h"
#include "version.h"

#define HK_TOGGLE 1
#define HK_QUIT 2
#define ICON_SIZE 32

enum { IDM_SETTINGS = 1, IDM_TOGGLE, IDM_PATTERN, IDM_UPDATE, IDM_QUIT };

static const wchar_t CLASS_NAME[] = L"fringefix_main";
static UINT g_taskbar_created;
static HICON g_icon_on;
static HICON g_icon_off;

void ui_utf8(const char *s, wchar_t *out, int n)
{
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, out, n) == 0) {
        out[0] = L'\0';
    }
    out[n - 1] = L'\0';
}

/* значок рисуется в памяти: три полосы R, G, B, зелёная выше, как в треугольной раскладке;
 * выключенный оверлей серый */
static HICON make_icon(int on)
{
    static const uint32_t COLORS[3] = { 0xFFE04848, 0xFF48D068, 0xFF4880F0 };
    BITMAPINFO bi;
    ICONINFO ii;
    HDC dc = GetDC(NULL);
    void *bits = NULL;
    HBITMAP color;
    HICON icon = NULL;
    int c;
    int x;
    int y;

    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = ICON_SIZE;
    bi.bmiHeader.biHeight = -ICON_SIZE;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    color = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, dc);
    if (!color) {
        return NULL;
    }
    memset(bits, 0, (size_t)ICON_SIZE * ICON_SIZE * 4);
    for (c = 0; c < 3; c++) {
        int x0 = 3 + c * 9;
        int y0 = c == 1 ? 4 : 9;

        for (y = y0; y < y0 + 19; y++) {
            for (x = x0; x < x0 + 8; x++) {
                ((uint32_t *)bits)[y * ICON_SIZE + x] = on ? COLORS[c] : 0xFF808080;
            }
        }
    }
    memset(&ii, 0, sizeof ii);
    ii.fIcon = TRUE;
    ii.hbmColor = color;
    ii.hbmMask = CreateBitmap(ICON_SIZE, ICON_SIZE, 1, 1, NULL);
    if (ii.hbmMask) {
        icon = CreateIconIndirect(&ii);
        DeleteObject(ii.hbmMask);
    }
    DeleteObject(color);
    return icon;
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
    nid.hIcon = a->enabled ? g_icon_on : g_icon_off;
    _snwprintf(nid.szTip, 128, L"fringefix %ls: %ls", L"" FF_VERSION,
               a->enabled ? L"включён" : L"выключен");
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

static void show_menu(ff_app *a)
{
    HMENU m = CreatePopupMenu();
    POINT pt;

    AppendMenuW(m, MF_STRING, IDM_SETTINGS, L"Настройки");
    AppendMenuW(m, MF_STRING, IDM_TOGGLE,
                a->enabled ? L"Выключить\tCtrl+Alt+F" : L"Включить\tCtrl+Alt+F");
    AppendMenuW(m, MF_STRING, IDM_PATTERN, L"Тестовый паттерн");
    AppendMenuW(m, MF_STRING, IDM_UPDATE, L"Проверить обновления");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_QUIT, L"Выход\tCtrl+Alt+Q");
    GetCursorPos(&pt);
    /* без SetForegroundWindow меню не закрывается по клику мимо */
    SetForegroundWindow(a->hwnd);
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, a->hwnd, NULL);
    PostMessageW(a->hwnd, WM_NULL, 0, 0);
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
        MessageBoxW(NULL, L"В этой сборке не указан репозиторий для обновлений.", L"fringefix",
                    MB_ICONINFORMATION | MB_TOPMOST);
        return;
    }
    ui_settings_set_status(L"Проверяю обновления...");
    update_check_async(a->hwnd, FF_REPO, 1);
}

static void on_command(ff_app *a, int id)
{
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
    balloon(a, L"fringefix не включился", text);
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
    case WM_APP_INSTALL_DONE:
        ui_update_on_done(a, (ff_install_result *)l);
        return 0;
    case WM_APP_GFX_ERROR:
        on_gfx_error(a, (char *)l);
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
    g_icon_on = make_icon(1);
    g_icon_off = make_icon(0);

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
