#ifndef FRINGEFIX_STATS_STORE_H
#define FRINGEFIX_STATS_STORE_H

#include <windows.h>

#include "stats.h"

/* общая статистика процесса за замком: счётчики пишет поток захвата, читает окно отчёта.
 * пока выключена, ничего не копится и файл на диске не трогается */
void stats_store_init(const wchar_t *path, int enabled);
void stats_store_set_enabled(int on);
int stats_store_enabled(void);

void stats_store_add_frames(unsigned drawn, unsigned echo_skipped);
void stats_store_device_reset(void);
void stats_store_pause(void);
void stats_store_error(const char *msg);

/* сеанс игры: begin запоминает программу и время, end дописывает секунды; повторный begin закрывает прежний */
void stats_store_game_begin(const char *exe, const char *how, const char *action);
void stats_store_game_end(void);

/* копия накопленного с учётом времени работы; открытый сеанс игры в неё не входит */
void stats_store_snapshot(ff_stats *out);

/* обнуляет память и удаляет файл */
void stats_store_clear(void);

/* пишет файл статистики на диск, если она включена */
void stats_store_save(void);

#endif
