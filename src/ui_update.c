#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "ui.h"
#include "version.h"

#define TEXT_MAX 320

static void box(const wchar_t *text, UINT icon)
{
    MessageBoxW(NULL, text, L"fringefix", icon | MB_SETFOREGROUND | MB_TOPMOST);
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
        _snwprintf(text, TEXT_MAX, L"Обновления недоступны: %ls", err);
        text[TEXT_MAX - 1] = L'\0';
        ui_settings_set_status(text);
        /* тихая проверка при запуске не должна выскакивать окном, если сети нет */
        if (info->manual) {
            box(text, MB_ICONWARNING);
        }
    } else if (!info->newer) {
        ui_settings_set_status(L"Установлена последняя версия");
        if (info->manual) {
            box(L"Установлена последняя версия.", MB_ICONINFORMATION);
        }
    } else {
        _snwprintf(text, TEXT_MAX, L"Доступна версия %ls, сейчас %ls. Скачать и установить?", tag, cur);
        text[TEXT_MAX - 1] = L'\0';
        ui_settings_set_status(text);
        if (MessageBoxW(NULL, text, L"fringefix",
                        MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND | MB_TOPMOST) == IDYES) {
            ui_settings_set_status(L"Скачиваю обновление...");
            update_install_async(a->hwnd, info);
        }
    }
    free(info);
}

void ui_update_on_done(ff_app *a, ff_install_result *res)
{
    if (res->ok) {
        wchar_t cmd[MAX_PATH + 32];
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;

        memset(&si, 0, sizeof si);
        si.cb = sizeof si;
        /* новый процесс ждёт, пока этот отпустит мьютекс единственного экземпляра */
        _snwprintf(cmd, MAX_PATH + 32, L"\"%ls\" --relaunch", res->exe);
        cmd[MAX_PATH + 31] = L'\0';
        if (CreateProcessW(res->exe, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            DestroyWindow(a->hwnd);
        } else {
            box(L"Обновление установлено, запусти fringefix заново.", MB_ICONINFORMATION);
        }
    } else {
        wchar_t err[200];
        wchar_t text[TEXT_MAX];

        ui_utf8(res->msg, err, 200);
        _snwprintf(text, TEXT_MAX, L"Не удалось обновиться: %ls", err);
        text[TEXT_MAX - 1] = L'\0';
        ui_settings_set_status(text);
        box(text, MB_ICONERROR);
    }
    free(res);
}
