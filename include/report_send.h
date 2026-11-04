#ifndef FRINGEFIX_REPORT_SEND_H
#define FRINGEFIX_REPORT_SEND_H

#include <windows.h>

/* результат отправки отчёта; приходит в WM_APP_REPORT_DONE как lParam, освобождает получатель */
typedef struct {
    int ok;
    char id[64];
    char msg[160];
} ff_report_result;

/* отправляет тело отчёта POST-запросом в фоновом потоке; тело копируется, вызывающий может сразу его менять.
 * urls это один или несколько https-адресов приёма через пробел: при сбое сети или ответе 5xx и 429
 * берётся следующий, ответ 4xx прерывает перебор, потому что другой сервер ответил бы так же.
 * reverse меняет порядок адресов на обратный: для английского интерфейса первым идёт европейский сервер */
void report_send_async(HWND notify, const char *urls, const char *body, size_t len, int reverse);

#endif
