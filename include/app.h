#ifndef FRINGEFIX_APP_H
#define FRINGEFIX_APP_H

#include <windows.h>

#include "config.h"
#include "live.h"

/* сообщения скрытому окну: трей и результаты фоновых потоков */
#define WM_APP_TRAY (WM_APP + 1)
#define WM_APP_UPDATE_INFO (WM_APP + 2)
#define WM_APP_INSTALL_DONE (WM_APP + 3)
#define WM_APP_GFX_ERROR (WM_APP + 4)
#define WM_APP_STATE (WM_APP + 5)
#define WM_APP_RELEASE_LIST (WM_APP + 6)
#define WM_APP_REPORT_DONE (WM_APP + 7)

#define PROFILE_NAME_LEN 64

/* общее состояние. интерфейс живёт в главном потоке, захват и оверлей в потоках по одному на выход монитора,
 * чтобы меню и диалоги не замораживали картинку, а мониторы с разными настройками не ждали друг друга.
 * флаги с Interlocked, сдвиги и резкость пишутся по одному float: запись выровнена и атомарна, а рваный набор значений
 * на один кадр не виден. списки pause_apps и ignore_apps правятся на месте строкой, поток захвата копирует cfg
 * целиком без блокировки: последний байт списка всегда ноль, поэтому худший случай это одна проверка автопаузы
 * с наполовину записанным списком. работает монитор, когда включены общий выключатель enabled и его собственный
 * cfg.view[i].enabled */
typedef struct {
    ff_config cfg;
    HINSTANCE inst;
    HWND hwnd;
    /* поток и событие пробуждения каждого монитора; потоков nmon штук, по числу выходов на момент запуска */
    HANDLE thread[FF_MAX_MONITORS];
    HANDLE wake[FF_MAX_MONITORS];
    int nmon;
    /* имена выходов для аддона ReShade: он находит монитор игры по имени */
    wchar_t dev[FF_MAX_MONITORS][FF_LIVE_DEV_LEN];
    int ndev;
    wchar_t ini[MAX_PATH];
    /* общий выключатель: хоткей, трей, ползунок аддона; поверх выключателей мониторов */
    volatile LONG enabled;
    /* по биту на монитор: значения изменились, надо залить в шейдер; устройство надо пересоздать */
    volatile LONG dirty_mask;
    volatile LONG reinit_mask;
    /* по биту на монитор: оверлей не поднялся при первом запуске, монитор молчит, пока пользователь что-нибудь не изменит;
     * сам выключатель монитора в настройках при этом не трогается, чтобы разовый сбой не стал постоянным */
    volatile LONG fail_mask;
    volatile LONG quit;
    volatile LONG fast;
    /* оверлей приостановлен автопаузой хотя бы на одном мониторе: идёт игра или полноэкранное приложение;
     * susp_mask хранит по биту на монитор */
    volatile LONG suspended;
    volatile LONG susp_mask;
    /* пауза из-за ReShade-аддона в игре, а не из-за автопаузы: нужна только подсказке в трее */
    volatile LONG pause_addon;
    /* поворот рабочего стола каждого монитора, который видел его поток захвата: 0, 1, 2, 3 это 0, 90, 180, 270 градусов */
    volatile LONG rot[FF_MAX_MONITORS];
} ff_app;

/* все мониторы разом: маска для dirty_mask и reinit_mask */
#define APP_ALL_MONITORS ((1L << FF_MAX_MONITORS) - 1)

/* пути, конфиг и событие пробуждения; 0 или -1 */
int app_init(ff_app *a, HINSTANCE inst);

/* останавливает поток захвата и освобождает ресурсы */
void app_shutdown(ff_app *a);

/* пишет конфиг в %APPDATA%\fringefix\fringefix.ini; 0 или -1 */
int app_save(ff_app *a);

/* сдвиги или резкость изменились: залить в шейдер и перерисовать */
void app_apply(ff_app *a);

void app_set_enabled(ff_app *a, int on);

/* захват надо пересоздать на всех мониторах: сменился способ вывода или движок */
void app_reinit(ff_app *a);

/* работает ли фильтр на мониторе index: включены общий выключатель и выключатель монитора, запуск не провалился */
int app_monitor_active(const ff_app *a, int index);

/* упавшим при запуске мониторам ещё одна попытка: вызывается, когда пользователь включает фильтр, а не при каждой правке
 * ползунка, иначе каждый шаг показывал бы ту же ошибку заново */
void app_retry(ff_app *a);

/* оверлей монитора не поднялся при последнем запуске */
int app_monitor_failed(const ff_app *a, int index);

/* набор значений, который правят окно, профили и кнопка сброса: вид выбранного монитора, а в режиме Б для повёрнутого
 * монитора набор его ориентации */
ff_view *app_edit_view(ff_app *a);

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

/* кладёт комплектные профили, вшитые в exe, в папку профилей; существующие файлы не трогает,
 * поэтому правки пользователя сохраняются, а удалённый профиль вернётся при следующем запуске */
void app_profile_install_bundled(const ff_app *a);

/* путь файла локальной статистики рядом с настройками */
void app_stats_path(const ff_app *a, wchar_t out[MAX_PATH]);

/* имя файла без папки и расширения */
void app_profile_basename(const wchar_t *path, wchar_t out[PROFILE_NAME_LEN]);

#endif
