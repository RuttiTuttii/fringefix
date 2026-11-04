#include "stats.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ff_stats_init(ff_stats *s)
{
    memset(s, 0, sizeof *s);
}

static unsigned add_sat(unsigned a, unsigned b)
{
    return b > UINT_MAX - a ? UINT_MAX : a + b;
}

/* в имя попадают только буквы, цифры и точка, плюс, минус, подчёркивание, пробел */
static void clean_name(const char *src, char *dst, size_t n)
{
    size_t i = 0;

    for (; *src && i + 1 < n; src++) {
        unsigned char c = (unsigned char)*src;

        dst[i++] = (isalnum(c) && c < 128) || c == '.' || c == '+' || c == '-' || c == ' '
                       ? (char)tolower(c) : '_';
    }
    dst[i] = '\0';
}

/* короткая метка из латиницы и цифр: разделитель | и переводы строк недопустимы */
static void clean_tag(const char *src, char *dst, size_t n)
{
    size_t i = 0;

    for (; *src && i + 1 < n; src++) {
        unsigned char c = (unsigned char)*src;

        dst[i++] = (isalnum(c) && c < 128) ? (char)tolower(c) : '_';
    }
    dst[i] = '\0';
}

static int merge_game(ff_stats *s, const char *exe, const char *how, const char *action,
                      unsigned sessions, unsigned seconds)
{
    char name[FF_EXE_LEN];
    ff_game_stat *g = NULL;
    int i;

    clean_name(exe, name, sizeof name);
    if (name[0] == '\0') {
        return 0;
    }
    for (i = 0; i < s->ngames; i++) {
        if (strcmp(s->games[i].exe, name) == 0) {
            g = &s->games[i];
            break;
        }
    }
    if (!g) {
        if (s->ngames >= FF_GAMES_MAX) {
            return 0;
        }
        g = &s->games[s->ngames++];
        memset(g, 0, sizeof *g);
        strcpy(g->exe, name);
    }
    clean_tag(how, g->how, sizeof g->how);
    clean_tag(action, g->action, sizeof g->action);
    g->sessions = add_sat(g->sessions, sessions);
    g->seconds = add_sat(g->seconds, seconds);
    return 1;
}

int ff_stats_game_add(ff_stats *s, const char *exe, const char *how, const char *action,
                      unsigned seconds)
{
    return merge_game(s, exe, how, action, 1, seconds);
}

void ff_stats_error_add(ff_stats *s, const char *msg)
{
    char clean[FF_ERR_LEN];
    size_t i = 0;
    int k;

    for (; *msg && i + 1 < sizeof clean; msg++) {
        unsigned char c = (unsigned char)*msg;

        clean[i++] = (c >= 32 && c < 127 && c != '|') ? (char)c : '_';
    }
    clean[i] = '\0';
    if (clean[0] == '\0') {
        return;
    }
    for (k = 0; k < s->nerrors; k++) {
        if (strcmp(s->errors[k], clean) == 0) {
            return;
        }
    }
    if (s->nerrors < FF_ERRORS_MAX) {
        strcpy(s->errors[s->nerrors++], clean);
    }
}

int ff_stats_format(const ff_stats *s, char *buf, size_t n)
{
    size_t used = 0;
    int w;
    int i;

#define APPEND(...)                                                  \
    do {                                                             \
        w = snprintf(buf + used, n - used, __VA_ARGS__);             \
        if (w < 0 || (size_t)w >= n - used) {                        \
            return -1;                                               \
        }                                                            \
        used += (size_t)w;                                           \
    } while (0)

    if (n == 0) {
        return -1;
    }
    buf[0] = '\0';
    APPEND("uptime_min=%u\nframes_drawn=%u\nframes_echo_skipped=%u\ndevice_resets=%u\npauses=%u\n",
           s->uptime_min, s->frames_drawn, s->frames_echo_skipped, s->device_resets, s->pauses);
    for (i = 0; i < s->nerrors; i++) {
        APPEND("error=%s\n", s->errors[i]);
    }
    for (i = 0; i < s->ngames; i++) {
        APPEND("game=%s|%s|%s|%u|%u\n", s->games[i].exe, s->games[i].how, s->games[i].action,
               s->games[i].sessions, s->games[i].seconds);
    }
#undef APPEND
    return (int)used;
}

static int parse_game(char *val, ff_stats *s)
{
    char *part[5];
    char *p = val;
    int n = 0;
    unsigned sessions;
    unsigned seconds;
    char *end;

    while (n < 5) {
        part[n++] = p;
        p = strchr(p, '|');
        if (!p) {
            break;
        }
        *p++ = '\0';
    }
    if (n != 5 || strchr(part[4], '|')) {
        return 0;
    }
    sessions = (unsigned)strtoul(part[3], &end, 10);
    if (*end || part[3][0] == '\0') {
        return 0;
    }
    seconds = (unsigned)strtoul(part[4], &end, 10);
    if (*end || part[4][0] == '\0' || sessions == 0) {
        return 0;
    }
    return merge_game(s, part[0], part[1], part[2], sessions, seconds);
}

static int parse_counter(const char *key, const char *val, ff_stats *s)
{
    char *end;
    unsigned v = (unsigned)strtoul(val, &end, 10);
    unsigned *dst = NULL;

    if (*end || val[0] == '\0') {
        return 0;
    }
    if (strcmp(key, "uptime_min") == 0) {
        dst = &s->uptime_min;
    } else if (strcmp(key, "frames_drawn") == 0) {
        dst = &s->frames_drawn;
    } else if (strcmp(key, "frames_echo_skipped") == 0) {
        dst = &s->frames_echo_skipped;
    } else if (strcmp(key, "device_resets") == 0) {
        dst = &s->device_resets;
    } else if (strcmp(key, "pauses") == 0) {
        dst = &s->pauses;
    }
    if (!dst) {
        return 0;
    }
    *dst = add_sat(*dst, v);
    return 1;
}

int ff_stats_parse(const char *text, ff_stats *s)
{
    int accepted = 0;
    const char *p = text;

    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        char line[256];
        char *eq;

        if (len < sizeof line) {
            memcpy(line, p, len);
            line[len] = '\0';
            if (len > 0 && line[len - 1] == '\r') {
                line[len - 1] = '\0';
            }
            eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                if (strcmp(line, "error") == 0) {
                    ff_stats_error_add(s, eq + 1);
                    accepted++;
                } else if (strcmp(line, "game") == 0) {
                    accepted += parse_game(eq + 1, s);
                } else {
                    accepted += parse_counter(line, eq + 1, s);
                }
            }
        }
        p = eol ? eol + 1 : p + len;
    }
    return accepted;
}
