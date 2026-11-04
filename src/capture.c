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

/* состояние автопаузы: либо оверлей молчит, либо работает в облегчённом режиме */
typedef struct {
    int paused;
    int light;
    int clear;
    ULONGLONG next;
    RECT mon;
} game_state;

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
static void set_state(ff_app *a, game_state *s, int paused, int light)
{
    if (paused != s->paused) {
        InterlockedExchange(&a->suspended, paused);
        PostMessageW(a->hwnd, WM_APP_STATE, 0, 0);
    }
    s->paused = paused;
    s->light = light;
}

/* раз в DETECT_MS спрашивает, не игра ли на переднем плане, и выбирает действие по game_action */
static void update_game_state(ff_app *a, game_state *s)
{
    ULONGLONG now = GetTickCount64();
    ff_config c = a->cfg;

    if (c.auto_pause == 0) {
        set_state(a, s, 0, 0);
        s->clear = 0;
    } else if (now >= s->next) {
        s->next = now + DETECT_MS;
        if (game_detect(&c, &s->mon) != GAME_NONE) {
            s->clear = 0;
            set_state(a, s, c.game_action == 0, c.game_action == 1);
        } else if ((s->paused || s->light) && ++s->clear >= RESUME_CHECKS) {
            set_state(a, s, 0, 0);
        }
    }
}

static DWORD WINAPI capture_main(LPVOID arg)
{
    ff_app *a = arg;
    ff_gfx g;
    game_state gs;
    char err[GFX_ERR_LEN];
    int started_ok = 0;
    int force = 0;
    ULONGLONG last_assert = 0;

    memset(&g, 0, sizeof g);
    memset(&gs, 0, sizeof gs);
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
        update_game_state(a, &gs);
        if (gs.paused) {
            gfx_shutdown(&g);
            WaitForSingleObject(a->wake, DETECT_MS);
            continue;
        }

        if (!g.alive) {
            ff_config snap = a->cfg;

            if (gfx_init(&g, &snap, err) != 0) {
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
            ff_output_rect(snap.monitor, &gs.mon);
            InterlockedExchange(&a->dirty, 1);
        }

        /* облегчённый режим включается и выключается без пересоздания устройства */
        if (g.light != gs.light) {
            g.light = gs.light;
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
