#include "report.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* дописывает в буфер с учётом места; при нехватке помечает переполнение и дальше ничего не пишет */
typedef struct {
    char *buf;
    size_t n;
    size_t used;
    int overflow;
} out_t;

static void put(out_t *o, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void put(out_t *o, const char *fmt, ...)
{
    va_list ap;
    int w;

    if (o->overflow) {
        return;
    }
    va_start(ap, fmt);
    w = vsnprintf(o->buf + o->used, o->n - o->used, fmt, ap);
    va_end(ap);
    if (w < 0 || (size_t)w >= o->n - o->used) {
        o->overflow = 1;
        return;
    }
    o->used += (size_t)w;
}

/* строка JSON в кавычках: экранирует кавычку, обратную косую и управляющие символы; UTF-8 проходит как есть */
static void put_string(out_t *o, const char *s)
{
    put(o, "\"");
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;

        if (c == '"' || c == '\\') {
            put(o, "\\%c", c);
        } else if (c == '\n') {
            put(o, "\\n");
        } else if (c == '\r') {
            put(o, "\\r");
        } else if (c == '\t') {
            put(o, "\\t");
        } else if (c < 32 || c == 127) {
            put(o, "\\u%04x", c);
        } else {
            put(o, "%c", c);
        }
    }
    put(o, "\"");
}

static void put_system(out_t *o, const ff_report *r)
{
    put(o, "  \"system\": {\n    \"windows_build\": %u,\n    \"gpu\": ", r->windows_build);
    put_string(o, r->gpu);
    put(o, ",\n    \"monitor\": { \"width\": %u, \"height\": %u, \"hdr\": %s }\n  },\n", r->width,
        r->height, r->hdr ? "true" : "false");
}

static void put_settings(out_t *o, const ff_report *r)
{
    const ff_config *c = &r->cfg;
    /* картинка выбранного в окне монитора; сервер принимает один набор сдвигов */
    const ff_view *v = ff_config_view_const(c, c->monitor);

    put(o,
        "  \"settings\": {\n"
        "    \"red\": { \"x\": %.3f, \"y\": %.3f },\n"
        "    \"green\": { \"x\": %.3f, \"y\": %.3f },\n"
        "    \"blue\": { \"x\": %.3f, \"y\": %.3f },\n"
        "    \"sharpen\": %.3f,\n"
        "    \"auto_pause\": %d,\n"
        "    \"game_action\": %d,\n"
        "    \"gpu_priority\": %d\n"
        "  },\n",
        (double)v->shift[0][0], (double)v->shift[0][1], (double)v->shift[1][0],
        (double)v->shift[1][1], (double)v->shift[2][0], (double)v->shift[2][1], (double)v->sharpen,
        /* облегчённого режима больше нет; поле остаётся нулём, потому что сервер его требует */
        c->auto_pause, 0, c->gpu_priority);
}

static void put_stats(out_t *o, const ff_report *r)
{
    const ff_stats *s = &r->stats;
    int i;

    put(o,
        "  \"counters\": {\n"
        "    \"uptime_min\": %u,\n"
        "    \"frames_drawn\": %u,\n"
        "    \"frames_echo_skipped\": %u,\n"
        "    \"device_resets\": %u,\n"
        "    \"pauses\": %u\n"
        "  },\n  \"errors\": [",
        s->uptime_min, s->frames_drawn, s->frames_echo_skipped, s->device_resets, s->pauses);
    for (i = 0; i < s->nerrors; i++) {
        put(o, "%s\n    ", i ? "," : "");
        put_string(o, s->errors[i]);
    }
    put(o, "%s],\n  \"games\": [", s->nerrors ? "\n  " : "");
    for (i = 0; i < s->ngames; i++) {
        const ff_game_stat *g = &s->games[i];

        put(o, "%s\n    { \"exe\": ", i ? "," : "");
        put_string(o, g->exe);
        put(o, ", \"how\": ");
        put_string(o, g->how);
        put(o, ", \"action\": ");
        put_string(o, g->action);
        put(o, ", \"sessions\": %u, \"minutes\": %u }", g->sessions, (g->seconds + 30) / 60);
    }
    put(o, "%s],\n", s->ngames ? "\n  " : "");
}

int ff_report_format(const ff_report *r, char *buf, size_t n)
{
    out_t o = { buf, n, 0, 0 };

    if (n == 0) {
        return -1;
    }
    buf[0] = '\0';
    put(&o, "{\n  \"schema\": \"fringefix-report\",\n  \"version\": 1,\n  \"app\": { \"version\": ");
    put_string(&o, r->app_version);
    put(&o, ", \"language\": ");
    put_string(&o, r->language);
    put(&o, " },\n");
    if (r->include_system) {
        put_system(&o, r);
    }
    if (r->include_settings) {
        put_settings(&o, r);
    }
    if (r->include_stats) {
        put_stats(&o, r);
    }
    put(&o, "  \"comment\": ");
    put_string(&o, r->comment);
    put(&o, "\n}\n");
    return o.overflow ? -1 : (int)o.used;
}
