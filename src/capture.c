#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "gfx.h"

#define FRAME_WAIT_MS 100
#define FRAME_WAIT_UI_MS 16
#define RETRY_MS 500
#define STOP_WAIT_MS 5000

static void pump_messages(void)
{
    MSG m;

    while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

/* текст ошибки уходит главному потоку, он его освободит */
static void report_error(ff_app *a, const char *err)
{
    char *copy = _strdup(err);

    if (copy && !PostMessageW(a->hwnd, WM_APP_GFX_ERROR, 0, (LPARAM)copy)) {
        free(copy);
    }
}

static DWORD WINAPI capture_main(LPVOID arg)
{
    ff_app *a = arg;
    ff_gfx g;
    char err[GFX_ERR_LEN];
    int started_ok = 0;
    int force = 0;

    memset(&g, 0, sizeof g);
    while (!a->quit) {
        /* окно оверлея принадлежит этому потоку, его сообщения разбираем здесь */
        pump_messages();

        if (!a->enabled || InterlockedExchange(&a->reinit, 0)) {
            gfx_shutdown(&g);
            started_ok = 0;
        }
        if (!a->enabled) {
            WaitForSingleObject(a->wake, INFINITE);
            continue;
        }

        if (!g.alive) {
            if (gfx_init(&g, a->cfg.monitor, err) != 0) {
                /* ошибку при первом запуске показываем, потеря захвата на UAC или смене режима тихо лечится повтором */
                if (!started_ok) {
                    report_error(a, err);
                    InterlockedExchange(&a->enabled, 0);
                } else {
                    WaitForSingleObject(a->wake, RETRY_MS);
                }
                continue;
            }
            started_ok = 1;
            InterlockedExchange(&a->dirty, 1);
        }

        if (InterlockedExchange(&a->dirty, 0)) {
            ff_config c = a->cfg;

            gfx_upload(&g, &c);
            force = 1;
        }
        if (!gfx_pump(&g, a->fast ? FRAME_WAIT_UI_MS : FRAME_WAIT_MS, force)) {
            gfx_shutdown(&g);
        }
        force = 0;
    }
    gfx_shutdown(&g);
    return 0;
}

int capture_start(ff_app *a)
{
    a->thread = CreateThread(NULL, 0, capture_main, a, 0, NULL);
    return a->thread ? 0 : -1;
}

void capture_stop(ff_app *a)
{
    if (!a->thread) {
        return;
    }
    InterlockedExchange(&a->quit, 1);
    SetEvent(a->wake);
    WaitForSingleObject(a->thread, STOP_WAIT_MS);
    CloseHandle(a->thread);
    a->thread = NULL;
}
