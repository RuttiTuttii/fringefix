#include <windows.h>
#include <string.h>

#include "ui.h"
#include "ui_paint.h"
#include "ui_state.h"
#include "winstate.h"

#define ARG_PREFIX "--windows="
#define STATE_MAX 96

void ui_windows_save(char *out, size_t n)
{
    ff_winstate w;
    POINT p;

    memset(&w, 0, sizeof w);
    w.open[FF_WIN_SETTINGS] = ui_settings_get_pos(&p);
    w.x[FF_WIN_SETTINGS] = (int)p.x;
    w.y[FF_WIN_SETTINGS] = (int)p.y;
    w.open[FF_WIN_PROGRAM] = ui_app_get_pos(&p);
    w.x[FF_WIN_PROGRAM] = (int)p.x;
    w.y[FF_WIN_PROGRAM] = (int)p.y;
    w.open[FF_WIN_APPS] = ui_apps_get_pos(&p);
    w.x[FF_WIN_APPS] = (int)p.x;
    w.y[FF_WIN_APPS] = (int)p.y;
    if (ff_winstate_format(&w, out, n) < 0) {
        out[0] = '\0';
    }
}

void ui_windows_restore(ff_app *a, const char *cmdline)
{
    ff_winstate w;
    char state[STATE_MAX];
    const char *arg = cmdline ? strstr(cmdline, ARG_PREFIX) : NULL;
    size_t n = 0;

    if (!arg) {
        return;
    }
    arg += sizeof ARG_PREFIX - 1;
    while (arg[n] && arg[n] != ' ' && n + 1 < sizeof state) {
        state[n] = arg[n];
        n++;
    }
    state[n] = '\0';
    memset(&w, 0, sizeof w);
    ff_winstate_parse(state, &w);
    /* dpi и шрифты обычно готовит окно настроек, а здесь первым может открыться окно программы */
    if (ui.dpi == 0) {
        HDC dc = GetDC(NULL);

        ui.dpi = GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(NULL, dc);
        paint_init(ui.dpi);
    }
    if (w.open[FF_WIN_SETTINGS]) {
        ui_settings_show_at(a, w.x[FF_WIN_SETTINGS], w.y[FF_WIN_SETTINGS]);
    }
    if (w.open[FF_WIN_PROGRAM]) {
        ui_app_show_at(a, w.x[FF_WIN_PROGRAM], w.y[FF_WIN_PROGRAM]);
    }
    if (w.open[FF_WIN_APPS]) {
        ui_apps_show_at(a, w.x[FF_WIN_APPS], w.y[FF_WIN_APPS]);
    }
}
