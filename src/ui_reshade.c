#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <wchar.h>

#include "i18n.h"
#include "reshade_inst.h"
#include "ui.h"
#include "ui_state.h"

/* диалог выбора exe игры; путь в path, 0 при отмене */
static int pick_game(HWND owner, wchar_t path[MAX_PATH])
{
    OPENFILENAMEW o;

    memset(&o, 0, sizeof o);
    path[0] = L'\0';
    o.lStructSize = sizeof o;
    o.hwndOwner = owner;
    o.lpstrFilter = L"*.exe\0*.exe\0";
    o.lpstrFile = path;
    o.nMaxFile = MAX_PATH;
    o.lpstrTitle = ff_tr(S_RS_PICK);
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&o) ? 1 : 0;
}

static void say(HWND owner, UINT icon, const wchar_t *fmt, const wchar_t *a, const wchar_t *b)
{
    wchar_t text[2 * MAX_PATH + 256];

    _snwprintf(text, sizeof text / sizeof text[0], fmt, a, b);
    text[sizeof text / sizeof text[0] - 1] = L'\0';
    MessageBoxW(owner, text, L"fringefix", icon | MB_TOPMOST);
}

void ui_reshade_install(HWND owner)
{
    wchar_t exe[MAX_PATH];
    wchar_t addon[MAX_PATH];
    wchar_t effect[MAX_PATH];
    int rc;

    if (!pick_game(owner, exe)) {
        return;
    }
    /* ReShade не ставится отсюда: без него аддон никто не загрузит, поэтому спрашиваем, прежде чем писать файлы */
    if (!rs_find_reshade(exe)
        && MessageBoxW(owner, ff_tr(S_RS_NO_RESHADE), L"fringefix", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST) != IDYES) {
        return;
    }
    rc = rs_install(ui.app->inst, exe, addon, effect);
    if (rc == RS_OK) {
        say(owner, MB_ICONINFORMATION, ff_tr(S_RS_DONE), addon, effect);
    } else if (rc == RS_ERR_RESOURCE) {
        MessageBoxW(owner, ff_tr(S_RS_NO_RES), L"fringefix", MB_ICONERROR | MB_TOPMOST);
    } else if (rc == RS_ERR_EXE) {
        MessageBoxW(owner, ff_tr(S_RS_BAD_PATH), L"fringefix", MB_ICONERROR | MB_TOPMOST);
    } else {
        say(owner, MB_ICONERROR, ff_tr(S_RS_FAILED), addon, L"");
    }
}

void ui_reshade_remove(HWND owner)
{
    wchar_t exe[MAX_PATH];
    wchar_t count[16];
    int n;

    if (!pick_game(owner, exe)) {
        return;
    }
    n = rs_remove(exe);
    if (n <= 0) {
        MessageBoxW(owner, ff_tr(S_RS_NOTHING), L"fringefix", MB_ICONINFORMATION | MB_TOPMOST);
        return;
    }
    _snwprintf(count, 16, L"%d", n);
    say(owner, MB_ICONINFORMATION, ff_tr(S_RS_REMOVED), count, L"");
}
