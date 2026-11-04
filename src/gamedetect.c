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

int game_detect(int mode, const RECT *monitor)
{
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    RECT wr;
    RECT overlap;

    if (mode <= 0 || !fg) {
        return GAME_NONE;
    }
    GetWindowThreadProcessId(fg, &pid);
    if (pid == GetCurrentProcessId() || is_shell_window(fg)) {
        return GAME_NONE;
    }
    if (covers_monitor(fg, monitor)) {
        return GAME_FULLSCREEN;
    }
    /* 3D-приложение считается, только если его окно лежит на мониторе оверлея */
    if (mode >= 2 && GetWindowRect(fg, &wr) && IntersectRect(&overlap, &wr, monitor)
        && is_3d_app(pid)) {
        return GAME_3D;
    }
    return GAME_NONE;
}
