#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "stats_store.h"

#define FILE_MAX 16384

static CRITICAL_SECTION g_lock;
static int g_ready;
static int g_enabled;
static ff_stats g_stats;
static wchar_t g_path[MAX_PATH];
static ULONGLONG g_started;
static char g_game[FF_EXE_LEN];
static char g_how[12];
static char g_action[8];
static ULONGLONG g_game_t0;

static void load_file(void)
{
    char buf[FILE_MAX];
    DWORD got = 0;
    HANDLE f = CreateFileW(g_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);

    if (f == INVALID_HANDLE_VALUE) {
        return;
    }
    if (!ReadFile(f, buf, sizeof buf - 1, &got, NULL)) {
        got = 0;
    }
    CloseHandle(f);
    buf[got] = '\0';
    ff_stats_parse(buf, &g_stats);
}

void stats_store_init(const wchar_t *path, int enabled)
{
    if (!g_ready) {
        InitializeCriticalSection(&g_lock);
        g_ready = 1;
    }
    EnterCriticalSection(&g_lock);
    wcsncpy(g_path, path, MAX_PATH - 1);
    g_path[MAX_PATH - 1] = L'\0';
    ff_stats_init(&g_stats);
    g_started = GetTickCount64();
    g_enabled = enabled;
    if (enabled) {
        load_file();
    }
    LeaveCriticalSection(&g_lock);
}

int stats_store_enabled(void)
{
    return g_enabled;
}

/* закрывает открытый сеанс игры под замком; вызывающий уже держит g_lock */
static void end_game_locked(void)
{
    if (g_game[0]) {
        ULONGLONG sec = (GetTickCount64() - g_game_t0) / 1000;

        ff_stats_game_add(&g_stats, g_game, g_how, g_action, sec > 0xFFFFFFFFu ? 0xFFFFFFFFu : (unsigned)sec);
        g_game[0] = '\0';
    }
}

void stats_store_set_enabled(int on)
{
    EnterCriticalSection(&g_lock);
    if (on && !g_enabled) {
        g_started = GetTickCount64();
        /* разбор файла прибавляет к g_stats, а в памяти уже лежит прежнее: начинаем с нуля, иначе счётчики удвоятся */
        ff_stats_init(&g_stats);
        load_file();
    } else if (!on && g_enabled) {
        /* итог уходит в файл до выключения: набранное с последнего сохранения иначе потерялось бы */
        end_game_locked();
        stats_store_save();
    }
    g_enabled = on;
    LeaveCriticalSection(&g_lock);
}

#define LOCKED(stmt)                         \
    do {                                     \
        if (!g_ready || !g_enabled) {        \
            return;                          \
        }                                    \
        EnterCriticalSection(&g_lock);       \
        stmt;                                \
        LeaveCriticalSection(&g_lock);       \
    } while (0)

void stats_store_add_frames(unsigned drawn, unsigned echo_skipped)
{
    LOCKED((g_stats.frames_drawn += drawn, g_stats.frames_echo_skipped += echo_skipped));
}

void stats_store_device_reset(void)
{
    LOCKED(g_stats.device_resets++);
}

void stats_store_pause(void)
{
    LOCKED(g_stats.pauses++);
}

void stats_store_error(const char *msg)
{
    LOCKED(ff_stats_error_add(&g_stats, msg));
}

void stats_store_game_begin(const char *exe, const char *how, const char *action)
{
    LOCKED((end_game_locked(), snprintf(g_game, sizeof g_game, "%s", exe),
            snprintf(g_how, sizeof g_how, "%s", how), snprintf(g_action, sizeof g_action, "%s", action),
            g_game_t0 = GetTickCount64()));
}

void stats_store_game_end(void)
{
    LOCKED(end_game_locked());
}

void stats_store_snapshot(ff_stats *out)
{
    ff_stats_init(out);
    if (!g_ready) {
        return;
    }
    EnterCriticalSection(&g_lock);
    *out = g_stats;
    if (g_enabled) {
        out->uptime_min += (unsigned)((GetTickCount64() - g_started) / 60000);
    }
    LeaveCriticalSection(&g_lock);
}

void stats_store_clear(void)
{
    if (!g_ready) {
        return;
    }
    EnterCriticalSection(&g_lock);
    ff_stats_init(&g_stats);
    g_started = GetTickCount64();
    g_game[0] = '\0';
    DeleteFileW(g_path);
    LeaveCriticalSection(&g_lock);
}

void stats_store_save(void)
{
    char buf[FILE_MAX];
    ff_stats snap;
    DWORD wrote = 0;
    HANDLE f;
    int n;

    if (!g_ready || !g_enabled) {
        return;
    }
    stats_store_snapshot(&snap);
    n = ff_stats_format(&snap, buf, sizeof buf);
    if (n < 0) {
        return;
    }
    f = CreateFileW(g_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        return;
    }
    WriteFile(f, buf, (DWORD)n, &wrote, NULL);
    CloseHandle(f);
}
