#include "applist.h"

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
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
    /* панель NVIDIA App на Electron грузит d3d11 и без этой строки считалась бы игрой */
    L"nvidia app.exe", L"nvcplui.exe",
    /* IDE на Java2D, WinUI и XAML-программы Windows 11, запись экрана: грузят d3d9 или d3d11 ради интерфейса */
    L"idea64.exe", L"pycharm64.exe", L"rider64.exe", L"clion64.exe", L"goland64.exe",
    L"webstorm64.exe", L"phpstorm64.exe", L"datagrip64.exe", L"taskmgr.exe", L"mspaint.exe",
    L"snippingtool.exe", L"obs64.exe",
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

/* имя в ASCII нижнего регистра; символы вне ASCII заменяются знаком вопроса и в списке не совпадут */
static void narrow(const wchar_t *w, char *out, size_t n)
{
    size_t i = 0;

    for (; *w && i + 1 < n; w++) {
        out[i++] = *w < 128 ? (char)*w : '?';
    }
    out[i] = '\0';
}

static int item_matches(const char *item, size_t len, const char *name)
{
    /* имя до NAME_MAX_W - 1 символов плюс ".exe" для элемента без точки */
    char buf[NAME_MAX_W + 5];
    size_t i;

    if (len == 0 || len + 5 >= sizeof buf) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        buf[i] = (char)tolower((unsigned char)item[i]);
    }
    buf[len] = '\0';
    if (strcmp(buf, name) == 0) {
        return 1;
    }
    /* элемент без точки считается именем без .exe */
    return strchr(buf, '.') == NULL && (strcat(buf, ".exe"), strcmp(buf, name) == 0);
}

int ff_app_list_has(const char *list, const wchar_t *exe)
{
    wchar_t base[NAME_MAX_W];
    char name[NAME_MAX_W];
    const char *p = list;

    if (!list || !exe || !base_lower(exe, base)) {
        return 0;
    }
    narrow(base, name, sizeof name);
    while (*p) {
        const char *end = strchr(p, ';');
        const char *a = p;
        const char *b = end ? end : p + strlen(p);

        while (a < b && (*a == ' ' || *a == '\t')) {
            a++;
        }
        while (b > a && (b[-1] == ' ' || b[-1] == '\t')) {
            b--;
        }
        if (item_matches(a, (size_t)(b - a), name)) {
            return 1;
        }
        p = end ? end + 1 : p + strlen(p);
    }
    return 0;
}

/* копирует элементы списка, кроме совпавших с name, в out; возвращает число пропущенных */
static int copy_without(const char *list, const char *name, char *out, size_t n)
{
    const char *p = list;
    size_t used = 0;
    int dropped = 0;

    out[0] = '\0';
    while (*p) {
        const char *end = strchr(p, ';');
        const char *a = p;
        const char *b = end ? end : p + strlen(p);

        while (a < b && (*a == ' ' || *a == '\t')) {
            a++;
        }
        while (b > a && (b[-1] == ' ' || b[-1] == '\t')) {
            b--;
        }
        if (b > a && item_matches(a, (size_t)(b - a), name)) {
            dropped++;
        } else if (b > a && used + (size_t)(b - a) + 2 <= n) {
            used += (size_t)snprintf(out + used, n - used, "%s%.*s", used ? ";" : "", (int)(b - a), a);
        }
        p = end ? end + 1 : p + strlen(p);
    }
    return dropped;
}

int ff_app_list_add(char *list, size_t n, const wchar_t *exe)
{
    wchar_t base[NAME_MAX_W];
    char name[NAME_MAX_W];
    size_t len;
    size_t have;

    if (!exe || !base_lower(exe, base)) {
        return -1;
    }
    narrow(base, name, sizeof name);
    /* знак вопроса это заменённый не-ASCII символ: такое имя в списке никогда не совпадёт;
     * точка с запятой в имени разорвала бы список */
    if (strchr(name, '?') || strchr(name, ';')) {
        return -1;
    }
    if (ff_app_list_has(list, exe)) {
        return 0;
    }
    len = strlen(name);
    have = strlen(list);
    if (have + (have ? 1 : 0) + len + 1 > n) {
        return -1;
    }
    snprintf(list + have, n - have, "%s%s", have ? ";" : "", name);
    return 1;
}

int ff_app_list_remove(char *list, size_t n, const wchar_t *exe)
{
    wchar_t base[NAME_MAX_W];
    char name[NAME_MAX_W];
    char out[FF_APPS_MAX];
    int dropped;

    if (!exe || !base_lower(exe, base) || n > sizeof out) {
        return 0;
    }
    narrow(base, name, sizeof name);
    dropped = copy_without(list, name, out, sizeof out);
    if (dropped) {
        snprintf(list, n, "%s", out);
    }
    return dropped > 0;
}

int ff_app_list_item(const char *list, int index, char *out, size_t n)
{
    const char *p = list;

    while (list && *p) {
        const char *end = strchr(p, ';');
        const char *a = p;
        const char *b = end ? end : p + strlen(p);

        while (a < b && (*a == ' ' || *a == '\t')) {
            a++;
        }
        while (b > a && (b[-1] == ' ' || b[-1] == '\t')) {
            b--;
        }
        if (b > a && index-- == 0) {
            if ((size_t)(b - a) + 1 > n) {
                return 0;
            }
            memcpy(out, a, (size_t)(b - a));
            out[b - a] = '\0';
            return 1;
        }
        p = end ? end + 1 : p + strlen(p);
    }
    return 0;
}

/* нижний регистр для ASCII и кириллицы: towlower в C-локали остальное не трогает, а заголовки окон бывают русскими.
 * shortcut: другие алфавиты (греческий, латиница с диакритикой) различаются по регистру, добавить таблицу при жалобах */
static wchar_t lower(wchar_t c)
{
    if (c >= L'A' && c <= L'Z') {
        return (wchar_t)(c + 0x20);
    }
    if (c >= 0x410 && c <= 0x42F) {
        return (wchar_t)(c + 0x20);
    }
    return c == 0x401 ? (wchar_t)0x451 : c;
}

/* вхождение needle (len символов) в hay без учёта регистра */
static int contains_ci(const wchar_t *hay, const wchar_t *needle, size_t len)
{
    size_t i;

    if (!hay) {
        return 0;
    }
    for (; *hay; hay++) {
        for (i = 0; i < len && hay[i] && lower(hay[i]) == lower(needle[i]); i++) {
        }
        if (i == len) {
            return 1;
        }
    }
    return 0;
}

int ff_app_query_match(const wchar_t *query, const wchar_t *exe, const wchar_t *title)
{
    size_t len;

    while (*query == L' ' || *query == L'\t') {
        query++;
    }
    len = wcslen(query);
    while (len > 0 && (query[len - 1] == L' ' || query[len - 1] == L'\t')) {
        len--;
    }
    return len == 0 || contains_ci(exe, query, len) || contains_ci(title, query, len);
}
