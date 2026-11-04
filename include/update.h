#ifndef FRINGEFIX_UPDATE_H
#define FRINGEFIX_UPDATE_H

#include <windows.h>

#include "release.h"

/* результат проверки; приходит в WM_APP_UPDATE_INFO как lParam, освобождает получатель */
typedef struct {
    int manual;
    int newer;
    ff_release rel;
    char err[160];
} ff_update_info;

/* результат установки; приходит в WM_APP_INSTALL_DONE как lParam, освобождает получатель */
typedef struct {
    int ok;
    char msg[160];
    wchar_t exe[MAX_PATH];
} ff_install_result;

/* спрашивает GitHub про последний релиз repo (owner/name) в фоновом потоке.
 * manual значит, что проверку запустил пользователь и тишина в ответ не нужна */
void update_check_async(HWND notify, const char *repo, int manual);

/* скачивает exe релиза, сверяет sha256 с файлом рядом и подменяет текущий exe; в фоновом потоке.
 * старый остаётся рядом как fringefix.exe.old, его удаляет update_cleanup при следующем запуске */
void update_install_async(HWND notify, const ff_update_info *info);

/* удаляет остатки прошлого обновления */
void update_cleanup(void);

#endif
