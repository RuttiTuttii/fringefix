#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "sysinfo.h"
#include "version.h"

#define LOG_MAX_BYTES (1024 * 1024)
#define LINE_LEN 512

static HANDLE g_file = INVALID_HANDLE_VALUE;
static wchar_t g_run[MAX_PATH];

void ff_log_path(const wchar_t *name, wchar_t out[MAX_PATH])
{
    wchar_t dir[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"APPDATA", dir, MAX_PATH);

    if (n == 0 || n >= MAX_PATH - 48) {
        wcsncpy(out, name, MAX_PATH - 1);
        out[MAX_PATH - 1] = L'\0';
        return;
    }
    _snwprintf(out, MAX_PATH, L"%ls\\fringefix", dir);
    out[MAX_PATH - 1] = L'\0';
    CreateDirectoryW(out, NULL);
    _snwprintf(out, MAX_PATH, L"%ls\\fringefix\\%ls", dir, name);
    out[MAX_PATH - 1] = L'\0';
}

void ff_log(int mon, const char *fmt, ...)
{
    char line[LINE_LEN];
    SYSTEMTIME st;
    DWORD w;
    int n;
    int body;
    __builtin_va_list ap;

    if (g_file == INVALID_HANDLE_VALUE) {
        return;
    }
    GetLocalTime(&st);
    n = snprintf(line, sizeof line, "%04u-%02u-%02u %02u:%02u:%02u.%03u t%lu ", st.wYear, st.wMonth, st.wDay,
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentThreadId());
    if (mon >= 0) {
        n += snprintf(line + n, sizeof line - (size_t)n, "m%d ", mon);
    }
    __builtin_va_start(ap, fmt);
    body = vsnprintf(line + n, sizeof line - (size_t)n - 2, fmt, ap);
    __builtin_va_end(ap);
    if (body < 0) {
        return;
    }
    n += body;
    /* длинная строка обрезается, а не теряется */
    if (n > (int)sizeof line - 3) {
        n = (int)sizeof line - 3;
    }
    line[n++] = '\r';
    line[n++] = '\n';
    WriteFile(g_file, line, (DWORD)n, &w, NULL);
}

void ff_log_modules(const char *why)
{
    HMODULE mods[1024];
    DWORD need = 0;
    DWORD i;
    DWORD n;
    char line[400];
    size_t used = 0;

    if (!EnumProcessModulesEx(GetCurrentProcess(), mods, sizeof mods, &need, LIST_MODULES_ALL)) {
        ff_log(-1, "modules (%s): enumeration failed (%lu)", why, GetLastError());
        return;
    }
    n = need / sizeof(HMODULE);
    if (n > 1024) {
        n = 1024;
    }
    ff_log(-1, "modules (%s): %lu loaded", why, n);
    line[0] = '\0';
    for (i = 0; i < n; i++) {
        wchar_t w[MAX_PATH];
        char name[96];
        const wchar_t *slash;
        int len;

        if (GetModuleFileNameW(mods[i], w, MAX_PATH) == 0) {
            continue;
        }
        slash = wcsrchr(w, L'\\');
        len = WideCharToMultiByte(CP_UTF8, 0, slash ? slash + 1 : w, -1, name, sizeof name - 1, NULL, NULL);
        if (len <= 0) {
            continue;
        }
        name[len < (int)sizeof name ? len : (int)sizeof name - 1] = '\0';
        if (used + strlen(name) + 3 >= sizeof line) {
            ff_log(-1, "  %s", line);
            line[0] = '\0';
            used = 0;
        }
        used += (size_t)snprintf(line + used, sizeof line - used, "%s ", name);
    }
    if (used > 0) {
        ff_log(-1, "  %s", line);
    }
}

void ff_log_init(void)
{
    wchar_t path[MAX_PATH];
    wchar_t old[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA fa;
    int unclean;
    HANDLE run;

    ff_log_path(L"fringefix.log", path);
    if (GetFileAttributesExW(path, GetFileExInfoStandard, &fa) && fa.nFileSizeHigh == 0
        && fa.nFileSizeLow > LOG_MAX_BYTES) {
        _snwprintf(old, MAX_PATH, L"%ls.1", path);
        old[MAX_PATH - 1] = L'\0';
        MoveFileExW(path, old, MOVEFILE_REPLACE_EXISTING);
    }
    g_file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    ff_log_path(L"fringefix.run", g_run);
    unclean = GetFileAttributesW(g_run) != INVALID_FILE_ATTRIBUTES;
    run = CreateFileW(g_run, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL, NULL);
    if (run != INVALID_HANDLE_VALUE) {
        CloseHandle(run);
    }
    ff_log(-1, "---- fringefix %s started, pid %lu", FF_VERSION, GetCurrentProcessId());
    if (unclean) {
        ff_log(-1, "previous run did not exit cleanly: killed, fast-fail, power loss or a crash without an exception");
    }
    {
        SYSTEM_INFO si;

        GetNativeSystemInfo(&si);
        ff_log(-1, "windows build %u, cpus %lu, monitors in windows %d", sys_windows_build(), si.dwNumberOfProcessors,
               GetSystemMetrics(SM_CMONITORS));
    }
}

void ff_log_shutdown(void)
{
    ff_log(-1, "clean exit");
    if (g_run[0]) {
        DeleteFileW(g_run);
    }
}
