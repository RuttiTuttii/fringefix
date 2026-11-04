#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "gamedetect.h"
#include "gfx.h"
#include "output.h"
#include "stats_store.h"

#define FRAME_WAIT_MS 100
#define FRAME_WAIT_UI_MS 16
#define RETRY_MS 500
#define STOP_WAIT_MS 5000
#define REASSERT_MS 2000
#define DETECT_MS 500
/* пауза снимается после стольких подряд проверок без игры: alt-tab туда и обратно не должен пересоздавать устройство */
#define RESUME_CHECKS 2
#define FLUSH_MS 1000
#define SAVE_MS (5 * 60 * 1000)

/* состояние автопаузы: либо оверлей молчит, либо работает в облегчённом режиме */
typedef struct {
    int paused;
    int light;
    int clear;
    ULONGLONG next;
    RECT mon;
    /* открытый сеанс игры для статистики */
    char exe[64];
    int in_game;
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
        if (paused) {
            stats_store_pause();
        }
        InterlockedExchange(&a->suspended, paused);
        PostMessageW(a->hwnd, WM_APP_STATE, 0, 0);
    }
    s->paused = paused;
    s->light = light;
}

static void end_session(game_state *s)
{
    if (s->in_game) {
        stats_store_game_end();
        s->in_game = 0;
        s->exe[0] = '\0';
    }
}

/* счётчики кадров уходят в общую статистику и обнуляются; перед каждым выключением устройства обязательно */
static void flush_counters(ff_gfx *g)
{
    stats_store_add_frames(g->drawn, g->echo_skipped);
    g->drawn = 0;
    g->echo_skipped = 0;
}

static void shutdown_gfx(ff_gfx *g)
{
    flush_counters(g);
    gfx_shutdown(g);
}

/* раз в DETECT_MS спрашивает, не игра ли на переднем плане, и выбирает действие по game_action */
static void update_game_state(ff_app *a, game_state *s)
{
    static const char *const HOW[] = { "none", "fullscreen", "3d", "listed" };
    ULONGLONG now = GetTickCount64();
    ff_config c = a->cfg;
    char exe[64];
    int how;

    if (c.auto_pause == 0) {
        set_state(a, s, 0, 0);
        end_session(s);
        s->clear = 0;
    } else if (now >= s->next) {
        s->next = now + DETECT_MS;
        how = game_detect(&c, &s->mon, exe);
        if (how != GAME_NONE) {
            s->clear = 0;
            set_state(a, s, c.game_action == 0, c.game_action == 1);
            /* новый сеанс, если сменилась программа; имя без пути, пустое заменяется на unknown */
            if (!s->in_game || strcmp(exe, s->exe) != 0) {
                snprintf(s->exe, sizeof s->exe, "%s", exe[0] ? exe : "unknown");
                stats_store_game_begin(s->exe, HOW[how], c.game_action == 0 ? "pause" : "light");
                s->in_game = 1;
            }
        } else if ((s->paused || s->light) && ++s->clear >= RESUME_CHECKS) {
            set_state(a, s, 0, 0);
            end_session(s);
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
    int lost = 0;
    int force = 0;
    ULONGLONG last_assert = 0;
    ULONGLONG next_flush = 0;
    ULONGLONG next_save = GetTickCount64() + SAVE_MS;

    memset(&g, 0, sizeof g);
    memset(&gs, 0, sizeof gs);
    while (!a->quit) {
        /* окно оверлея принадлежит этому потоку, его сообщения разбираем здесь */
        pump_messages();

        if (!a->enabled || InterlockedExchange(&a->reinit, 0)) {
            shutdown_gfx(&g);
            started_ok = 0;
        }
        if (!a->enabled) {
            WaitForSingleObject(a->wake, INFINITE);
            continue;
        }

        /* на паузе устройство и окно не держим: полноэкранная игра снова получает прямой вывод без композиции */
        update_game_state(a, &gs);
        if (gs.paused) {
            shutdown_gfx(&g);
            WaitForSingleObject(a->wake, DETECT_MS);
            continue;
        }

        if (!g.alive) {
            ff_config snap = a->cfg;

            if (gfx_init(&g, &snap, err) != 0) {
                stats_store_error(err);
                /* ошибку при первом запуске показываем, потеря захвата на UAC или смене режима тихо лечится повтором */
                if (!started_ok) {
                    report_error(a, err);
                    InterlockedExchange(&a->enabled, 0);
                } else {
                    WaitForSingleObject(a->wake, RETRY_MS);
                }
                continue;
            }
            if (lost) {
                stats_store_device_reset();
                lost = 0;
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
            lost = 1;
            shutdown_gfx(&g);
        }
        force = 0;
        if (GetTickCount64() >= next_flush) {
            next_flush = GetTickCount64() + FLUSH_MS;
            flush_counters(&g);
        }
        if (GetTickCount64() >= next_save) {
            next_save = GetTickCount64() + SAVE_MS;
            stats_store_save();
        }
        /* полноэкранное приложение или блокировка экрана могут увести окно из верха z-порядка */
        if (g.alive && GetTickCount64() - last_assert >= REASSERT_MS) {
            last_assert = GetTickCount64();
            gfx_reassert(&g);
        }
    }
    shutdown_gfx(&g);
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
