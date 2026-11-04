#include <windows.h>
#include <psapi.h>
#include <wchar.h>

#include "applist.h"
#include "gamedetect.h"

#define COVER_SLACK 2
#define MAX_MODULES 1024
#define CACHE_MS 5000

static const wchar_t *const SHELL_CLASSES[] = {
    L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd",
};

static const wchar_t *const GFX_MODULES[] = {
    L"d3d9.dll", L"d3d11.dll", L"d3d12.dll", L"vulkan-1.dll", L"opengl32.dll",
};

static int is_shell_window(HWND h)
{
    wchar_t cls[32];
    size_t i;

    if (GetClassNameW(h, cls, 32) == 0) {
        return 0;
    }
    for (i = 0; i < sizeof SHELL_CLASSES / sizeof SHELL_CLASSES[0]; i++) {
        if (wcscmp(cls, SHELL_CLASSES[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* окна на движке Chromium (Electron, CEF, браузеры) грузят d3d11 ради композитинга, игрой они не бывают:
 * иначе любая панель на Electron, например NVIDIA App, считалась бы 3D-приложением */
static int is_chromium_window(HWND h)
{
    wchar_t cls[32];

    return GetClassNameW(h, cls, 32) > 0 && (wcsncmp(cls, L"Chrome_WidgetWin", 16) == 0
                                             || wcsncmp(cls, L"CefBrowserWindow", 16) == 0);
}

/* пустой прямоугольник монитора не покрывает никто: иначе любое окно у левого края сошло бы за полный экран */
static int covers_monitor(HWND h, const RECT *mon)
{
    RECT r;
    LONG style = GetWindowLongW(h, GWL_STYLE);

    if (mon->right <= mon->left || mon->bottom <= mon->top || IsIconic(h)) {
        return 0;
    }
    /* окно с заголовком и рамкой, пусть и развёрнутое, это обычная программа */
    if ((style & WS_CAPTION) == WS_CAPTION || !GetWindowRect(h, &r)) {
        return 0;
    }
    return r.left <= mon->left + COVER_SLACK && r.top <= mon->top + COVER_SLACK
           && r.right >= mon->right - COVER_SLACK && r.bottom >= mon->bottom - COVER_SLACK;
}

static int has_gfx_module(HANDLE proc)
{
    HMODULE mods[MAX_MODULES];
    DWORD need = 0;
    DWORD n;
    DWORD i;
    size_t k;

    if (!EnumProcessModulesEx(proc, mods, sizeof mods, &need, LIST_MODULES_64BIT)) {
        return 0;
    }
    n = need / sizeof(HMODULE);
    if (n > MAX_MODULES) {
        n = MAX_MODULES;
    }
    for (i = 0; i < n; i++) {
        wchar_t name[64];

        if (GetModuleBaseNameW(proc, mods[i], name, 64) == 0) {
            continue;
        }
        for (k = 0; k < sizeof GFX_MODULES / sizeof GFX_MODULES[0]; k++) {
            if (_wcsicmp(name, GFX_MODULES[k]) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

/* игра по признакам процесса; результат на 5 секунд запоминается по pid, перебор модулей не бесплатный */
static int is_3d_app(DWORD pid)
{
    static DWORD cached_pid;
    static int cached;
    static ULONGLONG cached_at;
    wchar_t path[MAX_PATH];
    DWORD len = MAX_PATH;
    HANDLE p;
    int result = 0;

    if (pid == cached_pid && GetTickCount64() - cached_at < CACHE_MS) {
        return cached;
    }
    p = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (p) {
        if (QueryFullProcessImageNameW(p, 0, path, &len) && !ff_app_is_known_nongame(path)) {
            result = has_gfx_module(p);
        }
        CloseHandle(p);
    }
    cached_pid = pid;
    cached = result;
    cached_at = GetTickCount64();
    return result;
}

/* имя файла из пути в UTF-8; без пути и без папок, в отчёт попадает только оно */
static void base_name_utf8(const wchar_t *path, char out[64])
{
    const wchar_t *slash = wcsrchr(path, L'\\');
    const wchar_t *name = slash ? slash + 1 : path;

    out[0] = '\0';
    if (*name) {
        WideCharToMultiByte(CP_UTF8, 0, name, -1, out, 64, NULL, NULL);
        out[63] = '\0';
    }
}

/* путь к exe процесса активного окна; пустой, если процесс не открылся */
static void exe_path(DWORD pid, wchar_t path[MAX_PATH])
{
    DWORD len = MAX_PATH;
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

    path[0] = L'\0';
    if (p) {
        if (!QueryFullProcessImageNameW(p, 0, path, &len)) {
            path[0] = L'\0';
        }
        CloseHandle(p);
    }
}

int game_detect(const ff_config *cfg, const RECT *monitor, char exe[64])
{
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    wchar_t path[MAX_PATH];
    RECT wr;
    RECT overlap;

    exe[0] = '\0';
    if (cfg->auto_pause <= 0 || !fg) {
        return GAME_NONE;
    }
    GetWindowThreadProcessId(fg, &pid);
    if (pid == GetCurrentProcessId() || is_shell_window(fg)) {
        return GAME_NONE;
    }
    exe_path(pid, path);
    base_name_utf8(path, exe);
    if (path[0] && ff_app_list_has(cfg->ignore_apps, path)) {
        return GAME_NONE;
    }
    if (path[0] && ff_app_list_has(cfg->pause_apps, path)) {
        return GAME_LISTED;
    }
    /* режимы: 1 полный экран, 2 полный экран и 3D, 3 только 3D */
    if (cfg->auto_pause != 3 && covers_monitor(fg, monitor)) {
        return GAME_FULLSCREEN;
    }
    /* 3D-приложение считается, только если его окно лежит на мониторе оверлея */
    if (cfg->auto_pause >= 2 && !is_chromium_window(fg) && GetWindowRect(fg, &wr)
        && IntersectRect(&overlap, &wr, monitor) && is_3d_app(pid)) {
        return GAME_3D;
    }
    return GAME_NONE;
}
