#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "bench.h"
#include "crash.h"
#include "gamedetect.h"
#include "gfx.h"
#include "hud.h"
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

/* состояние автопаузы монитора: оверлей либо работает, либо молчит */
typedef struct {
    int paused;
    int clear;
    ULONGLONG next;
    RECT mon;
    /* монитор оверлея: по дескриптору прямоугольник перечитывается на каждой проверке */
    HMONITOR hmon;
    int mon_index;
    /* открытый сеанс игры для статистики */
    char exe[64];
    int in_game;
} game_state;

/* поток одного монитора: свой захват, оверлей и автопауза; число потоков равно числу выходов на запуске */
typedef struct {
    ff_app *app;
    int index;
    /* панель замеров открыта этим потоком: закрыть её может только он, окно принадлежит его очереди сообщений */
    int hud_open;
} worker;

static worker g_workers[FF_MAX_MONITORS];

/* поднятие устройств идёт по одному потоку за раз: создание устройства, дублирование вывода и сборка шейдеров
 * разных мониторов не должны накладываться, а все остальное работает параллельно */
static SRWLOCK g_init_lock = SRWLOCK_INIT;

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

/* смена паузы сообщается интерфейсу: значок в трее серый, пока идёт игра хотя бы на одном мониторе */
static void set_state(ff_app *a, game_state *s, int index, int paused)
{
    if (paused != s->paused) {
        LONG mask;

        if (paused) {
            stats_store_pause();
            mask = InterlockedOr(&a->susp_mask, 1L << index) | (1L << index);
        } else {
            mask = InterlockedAnd(&a->susp_mask, ~(1L << index)) & ~(1L << index);
        }
        InterlockedExchange(&a->suspended, mask != 0);
        PostMessageW(a->hwnd, WM_APP_STATE, 0, 0);
    }
    s->paused = paused;
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

static void shutdown_gfx(worker *w, ff_gfx *g)
{
    crash_mark(w->index, "shutdown");
    /* панель замеров принадлежит оверлею: без него её нет */
    if (w->hud_open) {
        hud_sync(NULL, 0);
        w->hud_open = 0;
    }
    flush_counters(g);
    gfx_shutdown(g);
}

/* прямоугольник монитора оверлея по текущему режиму. монопольный полный экран меняет разрешение, а захват
 * в этот момент закрыт, поэтому прямоугольник из gfx_init устаревает и окно игры не совпало бы с ним */
static void refresh_monitor(game_state *s, int index)
{
    MONITORINFO mi;

    if (!s->hmon || s->mon_index != index) {
        s->hmon = ff_output_hmonitor(index);
        s->mon_index = index;
    }
    mi.cbSize = sizeof mi;
    if (s->hmon && GetMonitorInfoW(s->hmon, &mi)) {
        s->mon = mi.rcMonitor;
    } else {
        /* дескриптор устарел после смены конфигурации мониторов: следующая проверка найдёт его заново */
        s->hmon = NULL;
    }
}

/* раз в DETECT_MS спрашивает, не игра ли на переднем плане этого монитора, и ставит паузу.
 * процесс с ReShade-аддоном ставит паузу при любых настройках: фильтр в нём рисует аддон */
static void update_game_state(worker *w, game_state *s)
{
    static const char *const HOW[] = { "none", "fullscreen", "3d", "listed", "addon" };
    ff_app *a = w->app;
    ULONGLONG now = GetTickCount64();
    ff_config c = a->cfg;
    char exe[64];
    int how;

    if (now < s->next) {
        return;
    }
    s->next = now + DETECT_MS;
    refresh_monitor(s, w->index);
    how = game_detect(&c, &s->mon, exe);
    if (how != GAME_NONE) {
        s->clear = 0;
        InterlockedExchange(&a->pause_addon, how == GAME_ADDON);
        set_state(a, s, w->index, 1);
        /* новый сеанс, если сменилась программа; имя без пути, пустое заменяется на unknown */
        if (!s->in_game || strcmp(exe, s->exe) != 0) {
            snprintf(s->exe, sizeof s->exe, "%s", exe[0] ? exe : "unknown");
            stats_store_game_begin(s->exe, HOW[how], "pause");
            s->in_game = 1;
        }
    } else if (s->paused && ++s->clear >= RESUME_CHECKS) {
        set_state(a, s, w->index, 0);
        end_session(s);
    }
}

static DWORD WINAPI capture_main(LPVOID arg)
{
    worker *w = arg;
    ff_app *a = w->app;
    const int idx = w->index;
    const LONG bit = 1L << idx;
    ff_gfx g;
    game_state gs;
    char err[GFX_ERR_LEN];
    int started_ok = 0;
    int ret;
    int lost = 0;
    int force = 0;
    ULONGLONG last_assert = 0;
    ULONGLONG next_flush = 0;
    ULONGLONG next_save = GetTickCount64() + SAVE_MS;

    memset(&g, 0, sizeof g);
    memset(&gs, 0, sizeof gs);
    while (!a->quit) {
        const int active = app_monitor_active(a, idx);

        /* окно оверлея принадлежит этому потоку, его сообщения разбираем здесь */
        pump_messages();

        if (!active || (InterlockedAnd(&a->reinit_mask, ~bit) & bit)) {
            shutdown_gfx(w, &g);
            started_ok = 0;
        }
        if (!active) {
            /* выключенный монитор не держит ни устройство, ни паузу: сбрасываем и состояние игры */
            if (gs.paused) {
                set_state(a, &gs, idx, 0);
                end_session(&gs);
            }
            WaitForSingleObject(a->wake[idx], INFINITE);
            continue;
        }

        /* на паузе устройство и окно не держим: полноэкранная игра снова получает прямой вывод без композиции */
        crash_mark(idx, "detect");
        update_game_state(w, &gs);
        if (gs.paused) {
            shutdown_gfx(w, &g);
            WaitForSingleObject(a->wake[idx], DETECT_MS);
            continue;
        }

        if (!g.alive) {
            ff_config snap = a->cfg;

            /* свой монитор потока, а не выбранный в окне */
            snap.monitor = idx;
            crash_mark(idx, "init");
            AcquireSRWLockExclusive(&g_init_lock);
            ret = gfx_init(&g, &snap, err);
            ReleaseSRWLockExclusive(&g_init_lock);
            if (ret != 0) {
                stats_store_error(err);
                /* окно диагностики по росту этого счётчика откатывает пробную настройку, не дожидаясь таймера */
                bench_note_gfx_failure();
                /* ошибку при первом запуске показываем, потеря захвата на UAC или смене режима тихо лечится повтором */
                if (!started_ok) {
                    report_error(a, err);
                    /* молчит только этот монитор, остальные работают; выключатель в настройках остаётся как был */
                    InterlockedOr(&a->fail_mask, bit);
                    PostMessageW(a->hwnd, WM_APP_STATE, 0, 0);
                } else {
                    WaitForSingleObject(a->wake[idx], RETRY_MS);
                }
                continue;
            }
            if (lost) {
                stats_store_device_reset();
                lost = 0;
            }
            started_ok = 1;
            ff_output_rect(idx, &gs.mon);
            /* окну настроек важно знать поворот: в режиме Б от него зависит, какой набор значений на экране */
            if (InterlockedExchange(&a->rot[idx], g.rot) != g.rot) {
                PostMessageW(a->hwnd, WM_APP_STATE, 0, 0);
            }
            InterlockedOr(&a->dirty_mask, bit);
        }

        g.diag = idx == a->cfg.monitor;
        if (InterlockedAnd(&a->dirty_mask, ~bit) & bit) {
            ff_config c = a->cfg;

            crash_mark(idx, "upload");

            gfx_upload(&g, &c, ff_config_view_for_const(&c, idx, g.rot));
            force = 1;
        }
        /* панель замеров поверх оверлея выбранного монитора: есть, пока открыто окно диагностики и включена галка */
        if (g.diag && bench_wanted() && bench_hud()) {
            hud_sync(&gs.mon, 1);
            w->hud_open = 1;
        } else if (w->hud_open) {
            hud_sync(NULL, 0);
            w->hud_open = 0;
        }
        if (!gfx_pump(&g, a->fast ? FRAME_WAIT_UI_MS : FRAME_WAIT_MS, force || (g.diag && bench_force()))) {
            lost = 1;
            shutdown_gfx(w, &g);
        }
        force = 0;
        if (GetTickCount64() >= next_flush) {
            next_flush = GetTickCount64() + FLUSH_MS;
            flush_counters(&g);
        }
        /* файл статистики один на программу: пишет первый поток */
        if (idx == 0 && GetTickCount64() >= next_save) {
            next_save = GetTickCount64() + SAVE_MS;
            stats_store_save();
        }
        /* полноэкранное приложение или блокировка экрана могут увести окно из верха z-порядка */
        if (g.alive && GetTickCount64() - last_assert >= REASSERT_MS) {
            last_assert = GetTickCount64();
            gfx_reassert(&g);
            if (w->hud_open) {
                hud_reassert();
            }
        }
    }
    shutdown_gfx(w, &g);
    return 0;
}

int capture_start(ff_app *a)
{
    wchar_t names[FF_MAX_MONITORS][OUTPUT_NAME_LEN];
    int n = ff_output_list(names, FF_MAX_MONITORS);
    int i;

    /* без единого выхода (сеанс без видеокарты) поток нулевого монитора всё равно запускается и сообщит ошибку */
    a->nmon = n > 0 ? n : 1;
    for (i = 0; i < a->nmon; i++) {
        g_workers[i].app = a;
        g_workers[i].index = i;
        a->thread[i] = CreateThread(NULL, 0, capture_main, &g_workers[i], 0, NULL);
        if (!a->thread[i]) {
            capture_stop(a);
            return -1;
        }
    }
    return 0;
}

void capture_stop(ff_app *a)
{
    int i;

    InterlockedExchange(&a->quit, 1);
    for (i = 0; i < FF_MAX_MONITORS; i++) {
        SetEvent(a->wake[i]);
    }
    for (i = 0; i < FF_MAX_MONITORS; i++) {
        if (a->thread[i]) {
            WaitForSingleObject(a->thread[i], STOP_WAIT_MS);
            CloseHandle(a->thread[i]);
            a->thread[i] = NULL;
        }
    }
}
