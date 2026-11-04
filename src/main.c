/* fringefix: оверлей против цветной каёмки на треугольных сабпикселях QD-OLED.
 * здесь только запуск: единственный экземпляр, окно сообщений, поток захвата, главный цикл.
 * захват и шейдер в capture.c и gfx*.c, интерфейс в ui_*.c, обновления в update.c */

#include <windows.h>
#include <commctrl.h>
#include <string.h>

#include "app.h"
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
    InitCommonControlsEx(&icc);

    if (app_init(&app, inst) != 0 || ui_tray_create(&app) != 0 || capture_start(&app) != 0) {
        MessageBoxW(NULL, L"Не удалось запустить fringefix.", L"fringefix", MB_ICONERROR);
        return 1;
    }
    if (app.cfg.check_updates && FF_REPO[0]) {
        update_check_async(app.hwnd, FF_REPO, 0);
    }

    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        if (!ui_settings_translate(&m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    app_shutdown(&app);
    ReleaseMutex(single);
    CloseHandle(single);
    return 0;
}
