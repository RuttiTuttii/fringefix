#include "config.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MONITOR_MAX 15
#define LINE_MAX_LEN 128

static const char *const SHIFT_KEYS[6] = {
    "red_x", "red_y", "green_x", "green_y", "blue_x", "blue_y",
};

void ff_config_defaults(ff_config *cfg)
{
    memset(cfg, 0, sizeof *cfg);
    cfg->check_updates = 1;
    cfg->auto_pause = 1;
}

/* срезает пробелы с обеих сторон прямо в буфере */
static char *trim(char *s)
{
    size_t n;

    while (*s == ' ' || *s == '\t') {
        s++;
    }
    n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) {
        s[--n] = '\0';
    }
    return s;
}

/* число должно занимать значение целиком, nan и inf отбрасываются, остальное зажимается в диапазон */
static int parse_number(const char *s, double lo, double hi, double *out)
{
    char *end;
    double v = strtod(s, &end);

    if (end == s || *end != '\0' || !isfinite(v)) {
        return 0;
    }
    *out = v < lo ? lo : (v > hi ? hi : v);
    return 1;
}

static int parse_line(char *line, ff_config *cfg)
{
    char *eq = strchr(line, '=');
    char *key;
    char *val;
    double v;
    int i;

    if (!eq) {
        return 0;
    }
    *eq = '\0';
    key = trim(line);
    val = trim(eq + 1);
    if (*key == ';' || *key == '#' || *key == '[') {
        return 0;
    }

    for (i = 0; i < 6; i++) {
        if (strcmp(key, SHIFT_KEYS[i]) == 0) {
            if (!parse_number(val, -FF_SHIFT_LIMIT, FF_SHIFT_LIMIT, &v)) {
                return 0;
            }
            cfg->shift[i / 2][i % 2] = (float)v;
            return 1;
        }
    }
    if (strcmp(key, "sharpen") == 0) {
        if (!parse_number(val, -FF_SHARPEN_LIMIT, FF_SHARPEN_LIMIT, &v)) {
            return 0;
        }
        cfg->sharpen = (float)v;
        return 1;
    }
    if (strcmp(key, "monitor") == 0) {
        if (!parse_number(val, 0.0, MONITOR_MAX, &v)) {
            return 0;
        }
        cfg->monitor = (int)v;
        return 1;
    }
    if (strcmp(key, "auto_pause") == 0) {
        if (!parse_number(val, 0.0, 2.0, &v)) {
            return 0;
        }
        cfg->auto_pause = (int)v;
        return 1;
    }
    if (strcmp(key, "check_updates") == 0) {
        if (!parse_number(val, 0.0, 1.0, &v)) {
            return 0;
        }
        cfg->check_updates = v >= 0.5;
        return 1;
    }
    return 0;
}

int ff_config_parse(const char *text, ff_config *cfg)
{
    int accepted = 0;
    const char *p = text;

    while (*p) {
        const char *eol = p;
        size_t len;
        char line[LINE_MAX_LEN];

        while (*eol && *eol != '\n') {
            eol++;
        }
        len = (size_t)(eol - p);
        /* слишком длинные строки не ключи, их пропускаем целиком */
        if (len < sizeof line) {
            memcpy(line, p, len);
            line[len] = '\0';
            accepted += parse_line(line, cfg);
        }
        p = *eol ? eol + 1 : eol;
    }
    return accepted;
}

int ff_config_format(const ff_config *cfg, char *buf, size_t n)
{
    int w = snprintf(buf, n,
        "; на сколько пикселей экрана уезжает содержимое канала, плюс означает вправо и вниз\n"
        "; диапазон от -2 до 2, комментарии только на отдельной строке\n"
        "red_x = %.3f\nred_y = %.3f\n"
        "green_x = %.3f\ngreen_y = %.3f\n"
        "blue_x = %.3f\nblue_y = %.3f\n"
        "\n; резкость от -1 до 1, минус смягчает картинку\nsharpen = %.3f\n"
        "\n; номер монитора, выходы всех видеокарт считаются подряд с нуля\nmonitor = %d\n"
        "\n; проверять обновления при запуске, 0 или 1\ncheck_updates = %d\n"
        "\n; автопауза: 0 выключена, 1 в полноэкранных приложениях, 2 ещё и в 3D-приложениях\nauto_pause = %d\n",
        (double)cfg->shift[0][0], (double)cfg->shift[0][1],
        (double)cfg->shift[1][0], (double)cfg->shift[1][1],
        (double)cfg->shift[2][0], (double)cfg->shift[2][1],
        (double)cfg->sharpen, cfg->monitor, cfg->check_updates, cfg->auto_pause);

    return (w < 0 || (size_t)w >= n) ? -1 : w;
}
