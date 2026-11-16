#ifndef FRINGEFIX_LOG_H
#define FRINGEFIX_LOG_H

#include <windows.h>

/* журнал работы: %APPDATA%\fringefix\fringefix.log. строка на событие с временем до миллисекунды, номером потока и монитором;
 * пишется из любого потока без замков, одна строка это один WriteFile в режиме дозаписи. при старте журнал больше 1 МБ
 * уходит в fringefix.log.1. файл fringefix.run живёт, пока программа работает: если он остался с прошлого раза,
 * процесс убит без штатного выхода (сбой без исключения, принудительное завершение, отключение питания) */
void ff_log_init(void);

/* штатный выход: пишет последнюю строку и убирает fringefix.run */
void ff_log_shutdown(void);

/* mon это номер выхода монитора или -1, если событие общее */
void ff_log(int mon, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* путь файла name в папке настроек; папка создаётся. без APPDATA файл ложится в текущую папку */
void ff_log_path(const wchar_t *name, wchar_t out[MAX_PATH]);

#endif
