#include "rspaths.h"

#include <ctype.h>
#include <string.h>

static int ieq(const char *a, size_t an, const char *b)
{
    size_t i;

    if (an != strlen(b)) {
        return 0;
    }
    for (i = 0; i < an; i++) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) {
            return 0;
        }
    }
    return 1;
}

/* обрезает пробелы и переводы строки по краям отрезка [s, e) */
static void trim(const char **s, const char **e)
{
    while (*s < *e && isspace((unsigned char)**s)) {
        (*s)++;
    }
    while (*e > *s && isspace((unsigned char)(*e)[-1])) {
        (*e)--;
    }
}

int ff_rs_ini_get(const char *text, const char *section, const char *key, char *out, size_t n)
{
    int in_section = 0;
    const char *p = text;

    if (n == 0) {
        return 0;
    }
    out[0] = '\0';
    /* BOM UTF-8 */
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
        p += 3;
    }
    while (*p) {
        const char *ls = p;
        const char *le = strchr(p, '\n');
        const char *eq;

        p = le ? le + 1 : p + strlen(p);
        if (!le) {
            le = p;
        }
        trim(&ls, &le);
        if (ls == le || *ls == ';' || *ls == '#') {
            continue;
        }
        if (*ls == '[') {
            const char *close = memchr(ls, ']', (size_t)(le - ls));

            in_section = close && ieq(ls + 1, (size_t)(close - ls - 1), section);
            continue;
        }
        eq = memchr(ls, '=', (size_t)(le - ls));
        if (in_section && eq) {
            const char *ke = eq;
            const char *vs = eq + 1;
            const char *ve = le;
            size_t len;

            trim(&ls, &ke);
            if (ieq(ls, (size_t)(ke - ls), key)) {
                trim(&vs, &ve);
                len = (size_t)(ve - vs);
                if (len >= n) {
                    len = n - 1;
                }
                memcpy(out, vs, len);
                out[len] = '\0';
                return 1;
            }
        }
    }
    return 0;
}

size_t ff_rs_first_path(const char *value, char *out, size_t n)
{
    const char *s = value;
    const char *e = strchr(value, ',');
    size_t len;

    if (n == 0) {
        return 0;
    }
    out[0] = '\0';
    if (!e) {
        e = value + strlen(value);
    }
    trim(&s, &e);
    if (s < e && *s == '"') {
        s++;
    }
    if (e > s && e[-1] == '"') {
        e--;
    }
    /* хвост "\**" значит "вся папка с вложенными" */
    while (e > s && (e[-1] == '*' || e[-1] == '\\' || e[-1] == '/')) {
        e--;
    }
    len = (size_t)(e - s);
    if (len >= n) {
        len = n - 1;
    }
    memcpy(out, s, len);
    out[len] = '\0';
    return len;
}

int ff_rs_is_absolute(const char *path)
{
    return (isalpha((unsigned char)path[0]) && path[1] == ':') || (path[0] == '\\' && path[1] == '\\');
}
