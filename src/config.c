#include "config.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MONITOR_MAX (FF_MAX_MONITORS - 1)
/* строка ini с самым длинным списком программ плюс имя ключа и пробелы */
#define LINE_MAX_LEN (FF_APPS_MAX + 64)

void ff_config_defaults(ff_config *cfg)
{
    memset(cfg, 0, sizeof *cfg);
    cfg->check_updates = 1;
    /* в полноэкранных и 3D-приложениях */
    cfg->auto_pause = 2;
    /* compute и ядро 3x3 */
    cfg->engine = 2;
    /* без настроек работает только первый монитор, остальные включаются в окне */
    cfg->view[0].enabled = 1;
    /* глобальное сочетание без модификатора забирает клавишу у всех программ: F11 это полноэкранный режим браузеров */
    cfg->hotkey_toggle.vk = FF_VK_F10;
    cfg->hotkey_quit.vk = FF_VK_F11;
}

static int clamp_index(int index)
{
    return index < 0 ? 0 : (index >= FF_MAX_MONITORS ? FF_MAX_MONITORS - 1 : index);
}

ff_view *ff_config_view(ff_config *cfg, int index)
{
    return &cfg->view[clamp_index(index)];
}

const ff_view *ff_config_view_const(const ff_config *cfg, int index)
{
    return &cfg->view[clamp_index(index)];
}

void ff_view_reset(ff_view *v)
{
    memset(v->shift, 0, sizeof v->shift);
    v->sharpen = 0.0f;
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

/* суффиксы ключей вида: красный, зелёный, синий по осям, резкость, флаг включения */
static const char *const VIEW_KEYS[] = {
    "red_x", "red_y", "green_x", "green_y", "blue_x", "blue_y", "sharpen", "enabled",
};
#define NVIEW_KEYS ((int)(sizeof VIEW_KEYS / sizeof VIEW_KEYS[0]))

static float *view_slot(ff_view *v, int k)
{
    return k < 6 ? &v->shift[k / 2][k % 2] : &v->sharpen;
}

/* значения старого файла лежали вне мониторов и относились к тому, на котором шёл оверлей: ключ monitor стоял
 * ниже сдвигов, поэтому они переезжают, когда он разобран */
static void move_legacy(ff_config *cfg, int m)
{
    int from = cfg->legacy_view - 1;

    if (from < 0 || from == m) {
        return;
    }
    cfg->view[m] = cfg->view[from];
    cfg->view[m].enabled = 1;
    memset(&cfg->view[from], 0, sizeof cfg->view[from]);
    cfg->legacy_view = m + 1;
}

/* ключ красок монитора: red_x старого вида или mon3_red_x нового. возвращает 1, если ключ этого вида; в *res
 * кладёт принятое значение или -1, если оно мусор */
static int parse_view_key(const char *key, ff_config *cfg, double *res, const char *val)
{
    int idx = -1;
    int legacy = 1;
    int k;
    double v;

    if (strncmp(key, "mon", 3) == 0 && key[3] >= '0' && key[3] <= '9' && key[4] == '_') {
        idx = key[3] - '0';
        key += 5;
        legacy = 0;
    }
    for (k = 0; k < NVIEW_KEYS; k++) {
        if (strcmp(key, VIEW_KEYS[k]) == 0) {
            break;
        }
    }
    if (k == NVIEW_KEYS || (legacy && k == 7)) {
        return 0;
    }
    if (idx >= FF_MAX_MONITORS) {
        *res = -1.0;
        return 1;
    }
    if (k == 7) {
        if (!parse_number(val, 0.0, 1.0, &v)) {
            *res = -1.0;
            return 1;
        }
        cfg->view[idx].enabled = v >= 0.5;
        *res = 1.0;
        return 1;
    }
    if (!parse_number(val, k == 6 ? -FF_SHARPEN_LIMIT : -FF_SHIFT_LIMIT, k == 6 ? FF_SHARPEN_LIMIT : FF_SHIFT_LIMIT, &v)) {
        *res = -1.0;
        return 1;
    }
    if (legacy) {
        /* пока ключа monitor не было, значения кладутся на выбранный монитор; legacy_view помнит, где они лежат */
        idx = cfg->monitor;
        cfg->legacy_view = idx + 1;
        cfg->view[idx].enabled = 1;
    }
    *view_slot(&cfg->view[idx], k) = (float)v;
    *res = 1.0;
    return 1;
}

static int parse_line(char *line, ff_config *cfg)
{
    char *eq = strchr(line, '=');
    char *key;
    char *val;
    double v;

    if (!eq) {
        return 0;
    }
    *eq = '\0';
    key = trim(line);
    val = trim(eq + 1);
    if (*key == ';' || *key == '#' || *key == '[') {
        return 0;
    }

    if (parse_view_key(key, cfg, &v, val)) {
        return v >= 0.0 ? 1 : 0;
    }
    if (strcmp(key, "monitor") == 0) {
        if (!parse_number(val, 0.0, MONITOR_MAX, &v)) {
            return 0;
        }
        cfg->monitor = (int)v;
        move_legacy(cfg, cfg->monitor);
        return 1;
    }
    if (strcmp(key, "auto_pause") == 0) {
        if (!parse_number(val, 0.0, 3.0, &v)) {
            return 0;
        }
        cfg->auto_pause = (int)v;
        return 1;
    }
    /* облегчённого режима больше нет: ключ из старых файлов принимается и ничего не меняет */
    if (strcmp(key, "game_action") == 0) {
        return parse_number(val, 0.0, 1.0, &v);
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
    if (strcmp(key, "engine") == 0) {
        if (!parse_number(val, 0.0, 2.0, &v)) {
            return 0;
        }
        cfg->engine = (int)v;
        return 1;
    }
    if (strcmp(key, "present_mode") == 0) {
        if (!parse_number(val, 0.0, 1.0, &v)) {
            return 0;
        }
        cfg->present_mode = (int)v;
        return 1;
    }
    if (strcmp(key, "pause_apps") == 0) {
        return copy_string(val, cfg->pause_apps, sizeof cfg->pause_apps);
    }
    if (strcmp(key, "ignore_apps") == 0) {
        return copy_string(val, cfg->ignore_apps, sizeof cfg->ignore_apps);
    }
    if (strcmp(key, "hud_corner") == 0) {
        if (!parse_number(val, 0.0, 3.0, &v)) {
            return 0;
        }
        cfg->hud_corner = (int)v;
        return 1;
    }
    if (strcmp(key, "reshade_dir") == 0) {
        return copy_string(val, cfg->reshade_dir, sizeof cfg->reshade_dir);
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

/* вид монитора по умолчанию: нулевые сдвиги, включён только нулевой монитор; такой блок в файл не пишется */
static int view_is_default(const ff_config *cfg, int m)
{
    const ff_view *v = &cfg->view[m];
    int ch;

    for (ch = 0; ch < 6; ch++) {
        if (v->shift[ch / 2][ch % 2] != 0.0f) {
            return 0;
        }
    }
    return v->sharpen == 0.0f && v->enabled == (m == 0);
}

int ff_config_format(const ff_config *cfg, char *buf, size_t n)
{
    char hk_toggle[32];
    char hk_quit[32];
    size_t pos = 0;
    int w;
    int m;

    ff_hotkey_format(&cfg->hotkey_toggle, hk_toggle, sizeof hk_toggle);
    ff_hotkey_format(&cfg->hotkey_quit, hk_quit, sizeof hk_quit);
    w = snprintf(buf, n,
        "; настройки картинки каждого монитора лежат в блоках с префиксом mon и номером выхода\n"
        "; на сколько пикселей экрана уезжает содержимое канала, плюс означает вправо и вниз, диапазон от -2 до 2\n"
        "; резкость от -1 до 1, минус смягчает картинку; enabled включает фильтр на этом мониторе, 0 или 1\n"
        "; комментарии только на отдельной строке\n"
        "\n; монитор, настройки которого показаны в окне, выходы всех видеокарт считаются подряд с нуля\nmonitor = %d\n",
        cfg->monitor);
    if (w < 0 || (size_t)w >= n) {
        return -1;
    }
    pos = (size_t)w;
    for (m = 0; m < FF_MAX_MONITORS; m++) {
        const ff_view *v = &cfg->view[m];

        /* нулевой монитор пишется всегда, чтобы файл не выглядел пустым, остальные только с настройками */
        if (m != 0 && view_is_default(cfg, m)) {
            continue;
        }
        w = snprintf(buf + pos, n - pos,
            "\nmon%d_enabled = %d\n"
            "mon%d_red_x = %.3f\nmon%d_red_y = %.3f\n"
            "mon%d_green_x = %.3f\nmon%d_green_y = %.3f\n"
            "mon%d_blue_x = %.3f\nmon%d_blue_y = %.3f\n"
            "mon%d_sharpen = %.3f\n",
            m, v->enabled != 0, m, (double)v->shift[0][0], m, (double)v->shift[0][1],
            m, (double)v->shift[1][0], m, (double)v->shift[1][1],
            m, (double)v->shift[2][0], m, (double)v->shift[2][1], m, (double)v->sharpen);
        if (w < 0 || pos + (size_t)w >= n) {
            return -1;
        }
        pos += (size_t)w;
    }
    w = snprintf(buf + pos, n - pos,
        "\n; проверять обновления при запуске, 0 или 1\ncheck_updates = %d\n"
        "\n; получать бета-версии при проверке обновлений, 0 или 1\nbeta = %d\n"
        "\n; глобальные сочетания: включить или выключить оверлей и закрыть программу\n"
        "; F1-F24 или буква и цифра с Ctrl, Alt, Shift, Win через плюс, none выключает\n"
        "hotkey_toggle = %s\nhotkey_quit = %s\n"
        "\n; автопауза: 0 выключена, 1 в полноэкранных приложениях, 2 в полноэкранных и 3D-приложениях, 3 только в 3D-приложениях\nauto_pause = %d\n"
        "\n; приоритет GPU оверлея от -7 до 7, ниже ноля уступает видеокарту игре\ngpu_priority = %d\n"
        "\n; имена exe через точку с запятой: всегда на паузу (pause_apps) и никогда не на паузу, то есть фильтр применяется всегда (ignore_apps)\npause_apps = %s\nignore_apps = %s\n"
        "\n; язык интерфейса: 0 как в системе, 1 русский, 2 английский\nlanguage = %d\n"
        "\n; тема окон: 0 как в системе, 1 светлая, 2 тёмная\ntheme = %d\n"
        "\n; локальная статистика для отчёта, сама ничего не отправляет: 0 или 1\nstats = %d\n"
        "\n; движок обработки: 0 классический, 1 сравнение кадров на compute-шейдере, 2 то же и фильтр ядром 3x3 (по умолчанию)\nengine = %d\n"
        "\n; способ вывода кадра (бета): 0 с ожиданием вертикальной синхронизации, 1 без ожидания (tearing)\npresent_mode = %d\n"
        "\n; угол монитора для панели замеров: 0 слева сверху, 1 справа сверху, 2 слева снизу, 3 справа снизу\nhud_corner = %d\n"
        "\n; последняя папка кнопок ReShade-аддона\nreshade_dir = %s\n",
        cfg->check_updates, cfg->beta, hk_toggle, hk_quit, cfg->auto_pause, cfg->gpu_priority,
        cfg->pause_apps, cfg->ignore_apps, cfg->language, cfg->theme, cfg->stats, cfg->engine, cfg->present_mode, cfg->hud_corner, cfg->reshade_dir);

    return (w < 0 || pos + (size_t)w >= n) ? -1 : (int)(pos + (size_t)w);
}
