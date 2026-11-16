#include <windows.h>
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
