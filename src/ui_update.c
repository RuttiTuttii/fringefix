#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "i18n.h"
#include "ui.h"
#include "version.h"

#define TEXT_MAX 320

static void box(const wchar_t *text, UINT icon)
{
    MessageBoxW(NULL, text, L"fringefix", icon | MB_SETFOREGROUND | MB_TOPMOST);
}

/* подставляет один или два текста в шаблон из таблицы строк */
static void format(wchar_t *out, str_id id, const wchar_t *a, const wchar_t *b)
{
    _snwprintf(out, TEXT_MAX, ff_tr(id), a, b);
    out[TEXT_MAX - 1] = L'\0';
}

void ui_update_on_info(ff_app *a, ff_update_info *info)
{
    wchar_t text[TEXT_MAX];
    wchar_t tag[48];
    wchar_t cur[48];
    wchar_t err[200];

    ui_utf8(info->rel.tag, tag, 48);
    ui_utf8(FF_VERSION, cur, 48);

    if (info->err[0]) {
        ui_utf8(info->err, err, 200);
        format(text, S_UPDATE_UNAVAILABLE, err, NULL);
        ui_settings_set_status(text);
        /* тихая проверка при запуске не должна выскакивать окном, если сети нет */
        if (info->manual) {
            box(text, MB_ICONWARNING);
        }
    } else if (!info->newer) {
        ui_settings_set_status(ff_tr(S_UP_TO_DATE));
        if (info->manual) {
            box(ff_tr(S_UP_TO_DATE), MB_ICONINFORMATION);
        }
    } else {
        if (info->rollback) {
            format(text, S_UPDATE_BETA_OFF, cur, tag);
        } else {
            format(text, S_UPDATE_FOUND, tag, cur);
        }
        ui_settings_set_status(text);
        if (MessageBoxW(NULL, text, L"fringefix",
                        MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND | MB_TOPMOST) == IDYES) {
            ui_settings_set_status(ff_tr(S_DOWNLOADING));
            update_install_async(a->hwnd, info);
        }
    }
    free(info);
}

void ui_update_on_done(ff_app *a, ff_install_result *res)
{
    if (res->ok) {
        wchar_t cmd[MAX_PATH + 160];
        char windows[96];
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;

        memset(&si, 0, sizeof si);
        si.cb = sizeof si;
        /* новый процесс ждёт, пока этот отпустит мьютекс единственного экземпляра,
         * и открывает те окна, что были открыты сейчас, на тех же местах */
        ui_windows_save(windows, sizeof windows);
        _snwprintf(cmd, MAX_PATH + 160, L"\"%ls\" --relaunch%hs%hs", res->exe,
                   windows[0] ? " --windows=" : "", windows);
        cmd[MAX_PATH + 159] = L'\0';
        if (CreateProcessW(res->exe, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            DestroyWindow(a->hwnd);
        } else {
            box(ff_tr(S_RESTART_MANUALLY), MB_ICONINFORMATION);
        }
    } else {
        wchar_t err[200];
        wchar_t text[TEXT_MAX];

        ui_utf8(res->msg, err, 200);
        format(text, S_UPDATE_FAILED, err, NULL);
        ui_settings_set_status(text);
        box(text, MB_ICONERROR);
    }
    free(res);
}
