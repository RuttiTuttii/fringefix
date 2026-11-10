#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "bench.h"
#include "fileio.h"
#include "hud.h"
#include "live.h"
#include "output.h"

#define INI_MAX 8192

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
    int i;

    memset(a, 0, sizeof *a);
    bench_init();
    a->inst = inst;
    ff_config_defaults(&a->cfg);
    for (i = 0; i < FF_MAX_MONITORS; i++) {
        a->wake[i] = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!a->wake[i]) {
            return -1;
        }
    }
    resolve_ini(a->ini);
    load_ini(a);
    app_profile_install_bundled(a);
    a->enabled = 1;
    hud_set_corner(a->cfg.hud_corner);
    /* имена выходов один раз: аддон находит по ним монитор окна игры */
    a->ndev = ff_output_devices(a->dev, FF_MAX_MONITORS);
    /* аддон ReShade читает отсюда живые значения; без памяти он берёт ini, поэтому сбой не фатален */
    if (live_publish_open() == 0) {
        live_publish(&a->cfg, 1, (const wchar_t (*)[FF_LIVE_DEV_LEN])a->dev, a->ndev);
    }
    return 0;
}

static void publish(ff_app *a)
{
    live_publish(&a->cfg, (int)a->enabled, (const wchar_t (*)[FF_LIVE_DEV_LEN])a->dev, a->ndev);
}

static void wake_all(ff_app *a)
{
    int i;

    for (i = 0; i < FF_MAX_MONITORS; i++) {
        SetEvent(a->wake[i]);
    }
}

int app_monitor_failed(const ff_app *a, int index)
{
    return (a->fail_mask & (1L << index)) != 0;
}

int app_monitor_active(const ff_app *a, int index)
{
    return a->enabled && ff_config_view_const(&a->cfg, index)->enabled && !app_monitor_failed(a, index);
}

void app_shutdown(ff_app *a)
{
    int i;

    capture_stop(a);
    live_publish_close();
    for (i = 0; i < FF_MAX_MONITORS; i++) {
        if (a->wake[i]) {
            CloseHandle(a->wake[i]);
            a->wake[i] = NULL;
        }
    }
}

int app_save(ff_app *a)
{
    char buf[INI_MAX];
    int n = ff_config_format(&a->cfg, buf, sizeof buf);

    return n < 0 ? -1 : ff_write_file(a->ini, buf, (size_t)n);
}

void app_retry(ff_app *a)
{
    InterlockedExchange(&a->fail_mask, 0);
    wake_all(a);
}

void app_apply(ff_app *a)
{
    publish(a);
    InterlockedOr(&a->dirty_mask, APP_ALL_MONITORS);
    wake_all(a);
}

void app_set_enabled(ff_app *a, int on)
{
    InterlockedExchange(&a->fail_mask, 0);
    InterlockedExchange(&a->enabled, on ? 1 : 0);
    publish(a);
    wake_all(a);
}

void app_reinit(ff_app *a)
{
    InterlockedExchange(&a->fail_mask, 0);
    InterlockedOr(&a->reinit_mask, APP_ALL_MONITORS);
    wake_all(a);
}

void app_wake(ff_app *a)
{
    wake_all(a);
}

void app_set_fast(ff_app *a, int on)
{
    InterlockedExchange(&a->fast, on ? 1 : 0);
}
