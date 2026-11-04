#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include "theme.h"

#define DWMWA_DARK_MODE 20
#define DWMWA_DARK_MODE_OLD 19

static ff_palette g_pal;

static int apps_use_dark(void)
{
    DWORD v = 1;
    DWORD n = sizeof v;

    /* значения нет на старых системах, тогда тема светлая */
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &v, &n) != ERROR_SUCCESS) {
        return 0;
    }
    return v == 0;
}

static COLORREF system_accent(void)
{
    DWORD c = 0;
    BOOL opaque = FALSE;

    if (FAILED(DwmGetColorizationColor(&c, &opaque))) {
        return RGB(0, 120, 212);
    }
    return RGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

void theme_refresh(void)
{
    g_pal.dark = apps_use_dark();
    g_pal.accent = system_accent();
    if (g_pal.dark) {
        g_pal.bg = RGB(32, 32, 32);
        g_pal.card = RGB(43, 43, 43);
        g_pal.field = RGB(51, 51, 51);
        g_pal.fg = RGB(255, 255, 255);
        g_pal.muted = RGB(160, 160, 160);
        g_pal.border = RGB(66, 66, 66);
    } else {
        g_pal.bg = RGB(243, 243, 243);
        g_pal.card = RGB(255, 255, 255);
        g_pal.field = RGB(255, 255, 255);
        g_pal.fg = RGB(26, 26, 26);
        g_pal.muted = RGB(96, 96, 96);
        g_pal.border = RGB(224, 224, 224);
    }
}

const ff_palette *theme_get(void)
{
    return &g_pal;
}

void theme_apply_window(HWND h)
{
    BOOL dark = g_pal.dark ? TRUE : FALSE;

    /* атрибут 20 на Windows 10 20H1 и новее, 19 на ранних сборках */
    if (FAILED(DwmSetWindowAttribute(h, DWMWA_DARK_MODE, &dark, sizeof dark))) {
        DwmSetWindowAttribute(h, DWMWA_DARK_MODE_OLD, &dark, sizeof dark);
    }
}

void theme_apply_control(HWND c, int field)
{
    const wchar_t *name = field ? (g_pal.dark ? L"DarkMode_CFD" : L"CFD")
                                : (g_pal.dark ? L"DarkMode_Explorer" : L"Explorer");

    SetWindowTheme(c, name, NULL);
}
