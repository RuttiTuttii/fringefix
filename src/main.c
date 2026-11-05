/* fringefix: оверлей против цветной каёмки на треугольных сабпикселях QD-OLED.
 * здесь только запуск: единственный экземпляр, окно сообщений, поток захвата, главный цикл.
 * захват и шейдер в capture.c и gfx*.c, интерфейс в ui_*.c, обновления в update.c */

#include <windows.h>
#include <commctrl.h>
#include <string.h>

#include "app.h"
#include "i18n.h"
#include "stats_store.h"
#include "theme.h"
#include "ui.h"
#include "update.h"
#include "version.h"

#define SINGLE_NAME L"Local\\fringefix_single_instance"
#define RELAUNCH_WAIT_MS 10000

/* второй экземпляр поверх первого удвоил бы сдвиг, поэтому он не запускается;
 * исключение: после обновления старый процесс ещё закрывается, и новый его ждёт */
static HANDLE acquire_single(int relaunch)
{
    HANDLE h = CreateMutexW(NULL, TRUE, SINGLE_NAME);

    if (!h) {
        return NULL;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        DWORD w = relaunch ? WaitForSingleObject(h, RELAUNCH_WAIT_MS) : WAIT_TIMEOUT;

        if (w != WAIT_OBJECT_0 && w != WAIT_ABANDONED) {
            CloseHandle(h);
            return NULL;
        }
    }
    return h;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    static ff_app app;
    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    HANDLE single;
    MSG m;

    (void)prev;
    (void)show;

    single = acquire_single(cmd && strstr(cmd, "--relaunch") != NULL);
    if (!single) {
        return 0;
    }
    update_cleanup();
    /* до загрузки настроек язык берётся у системы, чтобы сообщение об ошибке было понятным */
    ui_language_apply(0);
    InitCommonControlsEx(&icc);

    if (app_init(&app, inst) != 0) {
        MessageBoxW(NULL, ff_tr(S_START_FAILED), L"fringefix", MB_ICONERROR);
        return 1;
    }
    /* статистика, язык и тема готовы до трея и потока захвата: первая ошибка захвата попадёт в статистику,
     * а подсказка трея сразу будет на языке из настроек; 0 значит как в системе */
    {
        wchar_t stats_path[MAX_PATH];

        app_stats_path(&app, stats_path);
        stats_store_init(stats_path, app.cfg.stats);
    }
    ui_language_apply(app.cfg.language);
    theme_set_mode(app.cfg.theme);
    theme_refresh();
    if (ui_tray_create(&app) != 0 || capture_start(&app) != 0) {
        MessageBoxW(NULL, ff_tr(S_START_FAILED), L"fringefix", MB_ICONERROR);
        return 1;
    }
    /* после обновления или отката открываются те же окна на тех же местах */
    ui_windows_restore(&app, cmd);
    if (app.cfg.check_updates && FF_REPO[0]) {
        update_check_async(app.hwnd, FF_REPO, 0, app.cfg.beta);
    }

    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        if (!ui_settings_translate(&m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    app_shutdown(&app);
    stats_store_save();
    ReleaseMutex(single);
    CloseHandle(single);
    return 0;
}
