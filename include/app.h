#ifndef FRINGEFIX_APP_H
#define FRINGEFIX_APP_H

#include <windows.h>

#include "config.h"

/* сообщения скрытому окну: трей и результаты фоновых потоков */
#define WM_APP_TRAY (WM_APP + 1)
#define WM_APP_UPDATE_INFO (WM_APP + 2)
#define WM_APP_INSTALL_DONE (WM_APP + 3)
#define WM_APP_GFX_ERROR (WM_APP + 4)
#define WM_APP_STATE (WM_APP + 5)

#define PROFILE_NAME_LEN 64

/* общее состояние. интерфейс живёт в главном потоке, захват и оверлей в своём,
 * чтобы меню и диалоги не замораживали картинку. флаги с Interlocked, cfg пишется по одному float:
 * запись выровнена и атомарна, а рваный набор значений на один кадр не виден */
typedef struct {
    ff_config cfg;
    HINSTANCE inst;
    HWND hwnd;
    HANDLE thread;
    HANDLE wake;
    wchar_t ini[MAX_PATH];
    volatile LONG enabled;
    volatile LONG dirty;
    volatile LONG reinit;
    volatile LONG quit;
    volatile LONG fast;
    /* оверлей приостановлен автопаузой: идёт игра или полноэкранное приложение */
    volatile LONG suspended;
} ff_app;

/* пути, конфиг и событие пробуждения; 0 или -1 */
int app_init(ff_app *a, HINSTANCE inst);

/* останавливает поток захвата и освобождает ресурсы */
void app_shutdown(ff_app *a);

/* пишет конфиг в %APPDATA%\fringefix\fringefix.ini; 0 или -1 */
int app_save(ff_app *a);

/* сдвиги или резкость изменились: залить в шейдер и перерисовать */
void app_apply(ff_app *a);

void app_set_enabled(ff_app *a, int on);

/* сменили монитор: захват надо пересоздать */
void app_reinit(ff_app *a);

/* параметры вида поменялись без пересоздания захвата, например режим автопаузы */
void app_wake(ff_app *a);

/* окно настроек открыто: поток захвата отвечает на правки чаще */
void app_set_fast(ff_app *a, int on);

/* поток захвата и оверлея, живёт до capture_stop */
int capture_start(ff_app *a);
void capture_stop(ff_app *a);

/* профили: JSON-файлы с видом картинки в %APPDATA%\fringefix\profiles */
void app_profile_dir(const ff_app *a, wchar_t out[MAX_PATH]);

/* имена файлов без .json по алфавиту; возвращает их число */
int app_profile_list(const ff_app *a, wchar_t names[][PROFILE_NAME_LEN], int max);

/* 0, -1 файл не открылся, -2 это не профиль fringefix; при успехе сразу применяется */
int app_profile_load(ff_app *a, const wchar_t *path);

/* 0 или -1 */
int app_profile_save(ff_app *a, const wchar_t *path);

/* имя файла без папки и расширения */
void app_profile_basename(const wchar_t *path, wchar_t out[PROFILE_NAME_LEN]);

#endif
