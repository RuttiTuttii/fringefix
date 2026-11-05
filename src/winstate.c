#include "winstate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* по одной букве на окно, порядок как в enum */
static const char KEYS[FF_WIN_COUNT] = { 's', 'p', 'a' };

/* пределы координат: меньше любого реального расположения мониторов, но не дают переполнения при разборе */
#define COORD_LIMIT 100000

int ff_winstate_format(const ff_winstate *w, char *buf, size_t n)
{
    size_t used = 0;
    int i;

    if (n == 0) {
        return -1;
    }
    buf[0] = '\0';
    for (i = 0; i < FF_WIN_COUNT; i++) {
        int c;

        if (!w->open[i]) {
            continue;
        }
        c = snprintf(buf + used, n - used, "%s%c=%d,%d", used ? ";" : "", KEYS[i], w->x[i], w->y[i]);
        if (c < 0 || used + (size_t)c >= n) {
            buf[0] = '\0';
            return -1;
        }
        used += (size_t)c;
    }
    return (int)used;
}

/* "-12,34" в два числа в пределах; указатель за числами или NULL */
static const char *read_pair(const char *p, int *x, int *y)
{
    char *end;
    long a = strtol(p, &end, 10);
    long b;

    if (end == p || *end != ',') {
        return NULL;
    }
    p = end + 1;
    b = strtol(p, &end, 10);
    if (end == p || a < -COORD_LIMIT || a > COORD_LIMIT || b < -COORD_LIMIT || b > COORD_LIMIT) {
        return NULL;
    }
    *x = (int)a;
    *y = (int)b;
    return end;
}

int ff_winstate_parse(const char *s, ff_winstate *w)
{
    int accepted = 0;

    while (*s) {
        int i;

        for (i = 0; i < FF_WIN_COUNT && KEYS[i] != s[0]; i++) {
        }
        if (i < FF_WIN_COUNT && s[1] == '=') {
            const char *end = read_pair(s + 2, &w->x[i], &w->y[i]);

            if (end) {
                accepted += !w->open[i];
                w->open[i] = 1;
                s = end;
            }
        }
        /* до следующего окна: что бы ни стояло дальше, мусор пропускается */
        while (*s && *s != ';') {
            s++;
        }
        if (*s == ';') {
            s++;
        }
    }
    return accepted;
}
