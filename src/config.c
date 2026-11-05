#include "config.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MONITOR_MAX 15
/* строка ini с самым длинным списком программ плюс имя ключа и пробелы */
#define LINE_MAX_LEN (FF_APPS_MAX + 64)

static const char *const SHIFT_KEYS[6] = {
    "red_x", "red_y", "green_x", "green_y", "blue_x", "blue_y",
};

void ff_config_defaults(ff_config *cfg)
{
    memset(cfg, 0, sizeof *cfg);
    cfg->check_updates = 1;
    cfg->auto_pause = 1;
    /* глобальное сочетание без модификатора забирает клавишу у всех программ: F11 это полноэкранный режим браузеров */
    cfg->hotkey_toggle.vk = FF_VK_F10;
    cfg->hotkey_quit.vk = FF_VK_F11;
}

void ff_config_resolve_game(ff_config *cfg)
{
    if (cfg->auto_pause != 0) {
        cfg->game_action = 0;
    } else if (cfg->game_action == 1) {
        cfg->auto_pause = 2;
    }
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

/* строка целиком до конца значения; не помещается в буфер значит ключ отброшен, а не обрезан */
static int copy_string(const char *val, char *dst, size_t n)
{
    if (strlen(val) >= n) {
        return 0;
    }
    strcpy(dst, val);
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
        if (!parse_number(val, 0.0, 3.0, &v)) {
            return 0;
        }
        cfg->auto_pause = (int)v;
        return 1;
    }
    if (strcmp(key, "game_action") == 0) {
        if (!parse_number(val, 0.0, 1.0, &v)) {
            return 0;
        }
        cfg->game_action = (int)v;
        return 1;
    }
    if (strcmp(key, "gpu_priority") == 0) {
        if (!parse_number(val, -7.0, 7.0, &v)) {
            return 0;
        }
        cfg->gpu_priority = (int)v;
        return 1;
    }
    if (strcmp(key, "language") == 0 || strcmp(key, "theme") == 0) {
        if (!parse_number(val, 0.0, 2.0, &v)) {
            return 0;
        }
        *(key[0] == 'l' ? &cfg->language : &cfg->theme) = (int)v;
        return 1;
    }
    if (strcmp(key, "stats") == 0) {
        if (!parse_number(val, 0.0, 1.0, &v)) {
            return 0;
        }
        cfg->stats = v >= 0.5;
        return 1;
    }
    if (strcmp(key, "pause_apps") == 0) {
        return copy_string(val, cfg->pause_apps, sizeof cfg->pause_apps);
    }
    if (strcmp(key, "ignore_apps") == 0) {
        return copy_string(val, cfg->ignore_apps, sizeof cfg->ignore_apps);
    }
    if (strcmp(key, "hotkey_toggle") == 0 || strcmp(key, "hotkey_quit") == 0) {
        ff_hotkey h;

        if (ff_hotkey_parse(val, &h) != 0) {
            return 0;
        }
        *(key[7] == 't' ? &cfg->hotkey_toggle : &cfg->hotkey_quit) = h;
        return 1;
    }
    if (strcmp(key, "beta") == 0) {
        if (!parse_number(val, 0.0, 1.0, &v)) {
            return 0;
        }
        cfg->beta = v >= 0.5;
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

    /* блокнот Windows сохраняет UTF-8 с меткой BOM: без пропуска первый ключ файла терялся бы */
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
        p += 3;
    }

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
    char hk_toggle[32];
    char hk_quit[32];
    int w;

    ff_hotkey_format(&cfg->hotkey_toggle, hk_toggle, sizeof hk_toggle);
    ff_hotkey_format(&cfg->hotkey_quit, hk_quit, sizeof hk_quit);
    w = snprintf(buf, n,
        "; на сколько пикселей экрана уезжает содержимое канала, плюс означает вправо и вниз\n"
        "; диапазон от -2 до 2, комментарии только на отдельной строке\n"
        "red_x = %.3f\nred_y = %.3f\n"
        "green_x = %.3f\ngreen_y = %.3f\n"
        "blue_x = %.3f\nblue_y = %.3f\n"
        "\n; резкость от -1 до 1, минус смягчает картинку\nsharpen = %.3f\n"
        "\n; номер монитора, выходы всех видеокарт считаются подряд с нуля\nmonitor = %d\n"
        "\n; проверять обновления при запуске, 0 или 1\ncheck_updates = %d\n"
        "\n; получать бета-версии при проверке обновлений, 0 или 1\nbeta = %d\n"
        "\n; глобальные сочетания: включить или выключить оверлей и закрыть программу\n"
        "; F1-F24 или буква и цифра с Ctrl, Alt, Shift, Win через плюс, none выключает\n"
        "hotkey_toggle = %s\nhotkey_quit = %s\n"
        "\n; автопауза: 0 выключена, 1 в полноэкранных приложениях, 2 в полноэкранных и 3D-приложениях, 3 только в 3D-приложениях\nauto_pause = %d\n"
        "\n; что делать в игре, выбирается при auto_pause = 0: 0 пауза, 1 облегчённый режим без резкости и лапласиана\ngame_action = %d\n"
        "\n; приоритет GPU оверлея от -7 до 7, ниже ноля уступает видеокарту игре\ngpu_priority = %d\n"
        "\n; имена exe через точку с запятой: всегда на паузу (pause_apps) и никогда не на паузу, то есть фильтр применяется всегда (ignore_apps)\npause_apps = %s\nignore_apps = %s\n"
        "\n; язык интерфейса: 0 как в системе, 1 русский, 2 английский\nlanguage = %d\n"
        "\n; тема окон: 0 как в системе, 1 светлая, 2 тёмная\ntheme = %d\n"
        "\n; локальная статистика для отчёта, сама ничего не отправляет: 0 или 1\nstats = %d\n",
        (double)cfg->shift[0][0], (double)cfg->shift[0][1],
        (double)cfg->shift[1][0], (double)cfg->shift[1][1],
        (double)cfg->shift[2][0], (double)cfg->shift[2][1],
        (double)cfg->sharpen, cfg->monitor, cfg->check_updates, cfg->beta, hk_toggle, hk_quit, cfg->auto_pause, cfg->game_action, cfg->gpu_priority,
        cfg->pause_apps, cfg->ignore_apps, cfg->language, cfg->theme, cfg->stats);

    return (w < 0 || (size_t)w >= n) ? -1 : w;
}
