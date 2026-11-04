#include "profile.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const CHANNELS[3] = { "red", "green", "blue" };

int ff_profile_format(const ff_config *cfg, char *buf, size_t n)
{
    int w = snprintf(buf, n,
        "{\n"
        "  \"format\": \"" FF_PROFILE_FORMAT "\",\n"
        "  \"version\": 1,\n"
        "  \"red\": { \"x\": %.3f, \"y\": %.3f },\n"
        "  \"green\": { \"x\": %.3f, \"y\": %.3f },\n"
        "  \"blue\": { \"x\": %.3f, \"y\": %.3f },\n"
        "  \"sharpen\": %.3f\n"
        "}\n",
        (double)cfg->shift[0][0], (double)cfg->shift[0][1],
        (double)cfg->shift[1][0], (double)cfg->shift[1][1],
        (double)cfg->shift[2][0], (double)cfg->shift[2][1],
        (double)cfg->sharpen);

    return (w < 0 || (size_t)w >= n) ? -1 : w;
}

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    return p;
}

/* значение ключа внутри [from, to): указатель на его первый символ или NULL */
static const char *find_value(const char *from, const char *to, const char *key)
{
    char pat[24];
    const char *p = from;
    size_t n;

    snprintf(pat, sizeof pat, "\"%s\"", key);
    n = strlen(pat);
    while ((p = strstr(p, pat)) != NULL && p + n <= to) {
        const char *v = skip_ws(p + n);

        if (*v == ':') {
            return skip_ws(v + 1);
        }
        p += n;
    }
    return NULL;
}

/* число должно читаться целиком с начала значения; nan и inf отбрасываются, остальное зажимается */
static int read_number(const char *v, double limit, float *out)
{
    char *end;
    double d;

    if (!v) {
        return 0;
    }
    d = strtod(v, &end);
    if (end == v || !isfinite(d)) {
        return 0;
    }
    *out = (float)(d < -limit ? -limit : (d > limit ? limit : d));
    return 1;
}

/* объект канала {"x": .., "y": ..}; граница ищется до первой закрывающей скобки */
static int read_channel(const char *json, const char *name, float xy[2])
{
    char pat[16];
    const char *obj;
    const char *open;
    const char *close;
    int n = 0;

    snprintf(pat, sizeof pat, "\"%s\"", name);
    obj = strstr(json, pat);
    open = obj ? strchr(obj, '{') : NULL;
    close = open ? strchr(open, '}') : NULL;
    if (!close) {
        return 0;
    }
    n += read_number(find_value(open, close, "x"), FF_SHIFT_LIMIT, &xy[0]);
    n += read_number(find_value(open, close, "y"), FF_SHIFT_LIMIT, &xy[1]);
    return n;
}

int ff_profile_parse(const char *json, ff_config *cfg)
{
    int n = 0;
    int i;

    if (!strstr(json, "\"" FF_PROFILE_FORMAT "\"")) {
        return -1;
    }
    for (i = 0; i < 3; i++) {
        n += read_channel(json, CHANNELS[i], cfg->shift[i]);
    }
    n += read_number(find_value(json, json + strlen(json), "sharpen"), FF_SHARPEN_LIMIT,
                     &cfg->sharpen);
    return n;
}
