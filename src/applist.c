#include "applist.h"

#include <wctype.h>

#define NAME_MAX_W 64

static const wchar_t *const NONGAME[] = {
    L"chrome.exe", L"firefox.exe", L"msedge.exe", L"brave.exe", L"opera.exe", L"vivaldi.exe",
    L"zen.exe", L"yandex.exe", L"code.exe", L"devenv.exe", L"notepad.exe", L"notepad++.exe",
    L"sublime_text.exe", L"windowsterminal.exe", L"wt.exe", L"conhost.exe", L"powershell.exe",
    L"pwsh.exe", L"cmd.exe", L"explorer.exe", L"applicationframehost.exe", L"acrord32.exe",
    L"sumatrapdf.exe", L"obsidian.exe", L"discord.exe", L"slack.exe", L"teams.exe",
    L"telegram.exe", L"spotify.exe", L"winword.exe", L"excel.exe", L"powerpnt.exe",
    L"outlook.exe", L"onenote.exe", L"steam.exe", L"steamwebhelper.exe",
    L"epicgameslauncher.exe", L"claude.exe", L"blender.exe",
};

/* имя файла без папки в нижнем регистре; слишком длинное имя не может быть в списке */
static int base_lower(const wchar_t *path, wchar_t out[NAME_MAX_W])
{
    const wchar_t *name = path;
    const wchar_t *p;
    int n = 0;

    for (p = path; *p; p++) {
        if (*p == L'\\' || *p == L'/') {
            name = p + 1;
        }
    }
    for (p = name; *p; p++) {
        if (n + 1 >= NAME_MAX_W) {
            return 0;
        }
        out[n++] = (wchar_t)towlower((wint_t)*p);
    }
    out[n] = L'\0';
    return n > 0;
}

static int same(const wchar_t *a, const wchar_t *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

int ff_app_is_known_nongame(const wchar_t *exe)
{
    wchar_t name[NAME_MAX_W];
    size_t i;

    if (!exe || !base_lower(exe, name)) {
        return 0;
    }
    for (i = 0; i < sizeof NONGAME / sizeof NONGAME[0]; i++) {
        if (same(name, NONGAME[i])) {
            return 1;
        }
    }
    return 0;
}
