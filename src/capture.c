#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "gamedetect.h"
#include "gfx.h"
#include "output.h"

#define FRAME_WAIT_MS 100
#define FRAME_WAIT_UI_MS 16
#define RETRY_MS 500
#define STOP_WAIT_MS 5000
#define REASSERT_MS 2000
#define DETECT_MS 500
/* пауза снимается после стольких подряд проверок без игры: alt-tab туда и обратно не должен пересоздавать устройство */
#define RESUME_CHECKS 2

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

/* смена паузы сообщается интерфейсу: значок в трее серый, пока идёт игра */
static void set_paused(ff_app *a, int *paused, int now)
{
    if (now == *paused) {
        return;
    }
    *paused = now;
    InterlockedExchange(&a->suspended, now);
    PostMessageW(a->hwnd, WM_APP_STATE, 0, 0);
}

/* раз в DETECT_MS спрашивает, не игра ли на переднем плане; true, пока оверлей должен молчать */
static int auto_pause(ff_app *a, const RECT *mon, int *paused, int *clear, ULONGLONG *next)
{
    ULONGLONG now = GetTickCount64();

    if (a->cfg.auto_pause == 0) {
        set_paused(a, paused, 0);
        *clear = 0;
    } else if (now >= *next) {
        *next = now + DETECT_MS;
        if (game_detect(a->cfg.auto_pause, mon) != GAME_NONE) {
            *clear = 0;
            set_paused(a, paused, 1);
        } else if (*paused && ++*clear >= RESUME_CHECKS) {
            set_paused(a, paused, 0);
        }
    }
    return *paused;
}

static DWORD WINAPI capture_main(LPVOID arg)
{
    ff_app *a = arg;
    ff_gfx g;
    char err[GFX_ERR_LEN];
    int started_ok = 0;
    int force = 0;
    ULONGLONG last_assert = 0;
    ULONGLONG next_detect = 0;
    int paused = 0;
    int clear = 0;
    RECT mon = { 0, 0, 0, 0 };

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

        /* на паузе устройство и окно не держим: полноэкранная игра снова получает прямой вывод без композиции */
        if (auto_pause(a, &mon, &paused, &clear, &next_detect)) {
            gfx_shutdown(&g);
            WaitForSingleObject(a->wake, DETECT_MS);
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
            ff_output_rect(a->cfg.monitor, &mon);
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
        /* полноэкранное приложение или блокировка экрана могут увести окно из верха z-порядка */
        if (g.alive && GetTickCount64() - last_assert >= REASSERT_MS) {
            last_assert = GetTickCount64();
            gfx_reassert(&g);
        }
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
