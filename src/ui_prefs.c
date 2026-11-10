#include <windows.h>
#include <wchar.h>

#include "i18n.h"
#include "theme.h"
#include "ui.h"
#include "ui_paint.h"
#include "ui_state.h"

void ui_language_apply(int mode)
{
    ff_i18n_set(ff_i18n_resolve(mode, PRIMARYLANGID(GetUserDefaultUILanguage())));
}

/* окна fringefix этого потока, у которых есть что перекрасить: трей и паттерн свою тему не имеют */
static BOOL CALLBACK restyle_top(HWND w, LPARAM unused)
{
    wchar_t cls[32];

    (void)unused;
    if (GetClassNameW(w, cls, 32) > 0 && wcsncmp(cls, L"fringefix_", 10) == 0
        && wcscmp(cls, L"fringefix_main") != 0 && wcscmp(cls, L"fringefix_pattern") != 0) {
        ctl_restyle_window(w);
    }
    return TRUE;
}

void ui_theme_apply(void)
{
    /* шрифты и кисти общие, поэтому пересоздаются один раз, а затем каждое окно берёт их заново */
    theme_refresh();
    paint_init(ui.dpi);
    EnumThreadWindows(GetCurrentThreadId(), restyle_top, 0);
}

void ui_dpi_apply(int dpi)
{
    if (dpi > 0 && dpi != ui.dpi) {
        ui.dpi = dpi;
        ui_theme_apply();
    }
}
