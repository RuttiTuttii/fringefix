#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "fileio.h"

#define INI_MAX 4096

/* настройки лежат в профиле пользователя, чтобы exe можно было обновлять в любой папке */
static void resolve_ini(wchar_t out[MAX_PATH])
{
    wchar_t dir[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"APPDATA", dir, MAX_PATH);

    if (n == 0 || n >= MAX_PATH - 32) {
        wcscpy(out, L"fringefix.ini");
        return;
    }
    _snwprintf(out, MAX_PATH, L"%ls\\fringefix", dir);
    out[MAX_PATH - 1] = L'\0';
    CreateDirectoryW(out, NULL);
    _snwprintf(out, MAX_PATH, L"%ls\\fringefix\\fringefix.ini", dir);
    out[MAX_PATH - 1] = L'\0';
}

static void load_ini(ff_app *a)
{
    char buf[INI_MAX];
    DWORD got = 0;
    HANDLE f = CreateFileW(a->ini, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);

    if (f == INVALID_HANDLE_VALUE) {
        return;
    }
    if (!ReadFile(f, buf, sizeof buf - 1, &got, NULL)) {
        got = 0;
    }
    CloseHandle(f);
    buf[got] = '\0';
    ff_config_parse(buf, &a->cfg);
}

int app_init(ff_app *a, HINSTANCE inst)
{
    memset(a, 0, sizeof *a);
    a->inst = inst;
    ff_config_defaults(&a->cfg);
    a->wake = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!a->wake) {
        return -1;
    }
    resolve_ini(a->ini);
    load_ini(a);
    app_profile_install_bundled(a);
    a->enabled = 1;
    return 0;
}

void app_shutdown(ff_app *a)
{
    capture_stop(a);
    if (a->wake) {
        CloseHandle(a->wake);
        a->wake = NULL;
    }
}

int app_save(ff_app *a)
{
    char buf[INI_MAX];
    int n = ff_config_format(&a->cfg, buf, sizeof buf);

    return n < 0 ? -1 : ff_write_file(a->ini, buf, (size_t)n);
}

void app_apply(ff_app *a)
{
    InterlockedExchange(&a->dirty, 1);
    SetEvent(a->wake);
}

void app_set_enabled(ff_app *a, int on)
{
    InterlockedExchange(&a->enabled, on ? 1 : 0);
    SetEvent(a->wake);
}

void app_reinit(ff_app *a)
{
    InterlockedExchange(&a->reinit, 1);
    SetEvent(a->wake);
}

void app_wake(ff_app *a)
{
    SetEvent(a->wake);
}

void app_set_fast(ff_app *a, int on)
{
    InterlockedExchange(&a->fast, on ? 1 : 0);
}
