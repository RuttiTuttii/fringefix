#ifndef FRINGEFIX_STATS_H
#define FRINGEFIX_STATS_H

#include <stddef.h>

#define FF_GAMES_MAX 32
#define FF_ERRORS_MAX 5
#define FF_EXE_LEN 64
#define FF_ERR_LEN 96

/* одна программа, из-за которой оверлей уходил на паузу или в облегчённый режим */
typedef struct {
    char exe[FF_EXE_LEN];
    /* как определена: fullscreen, 3d или listed */
    char how[12];
    /* что сделал оверлей: pause или light */
    char action[8];
    unsigned sessions;
    unsigned seconds;
} ff_game_stat;

/* накопленные счётчики; только числа, имена exe без путей и тексты собственных ошибок оверлея */
typedef struct {
    unsigned uptime_min;
    unsigned frames_drawn;
    unsigned frames_echo_skipped;
    unsigned device_resets;
    unsigned pauses;
    char errors[FF_ERRORS_MAX][FF_ERR_LEN];
    int nerrors;
    ff_game_stat games[FF_GAMES_MAX];
    int ngames;
} ff_stats;

void ff_stats_init(ff_stats *s);

/* добавляет сеанс игры к записи с тем же именем exe или заводит новую; 0, если таблица полна.
 * имя приводится к нижнему регистру, лишние символы заменяются подчёркиванием, счётчики не переполняются */
int ff_stats_game_add(ff_stats *s, const char *exe, const char *how, const char *action,
                      unsigned seconds);

/* запоминает текст ошибки без повторов, до FF_ERRORS_MAX штук; непечатные символы заменяются */
void ff_stats_error_add(ff_stats *s, const char *msg);

/* текстовый вид для файла на диске: строки ключ=значение; длина или -1, если буфер мал */
int ff_stats_format(const ff_stats *s, char *buf, size_t n);

/* читает его обратно, добавляя к s; возвращает число принятых строк, мусор пропускает */
int ff_stats_parse(const char *text, ff_stats *s);

#endif
