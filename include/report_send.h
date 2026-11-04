#ifndef FRINGEFIX_REPORT_SEND_H
#define FRINGEFIX_REPORT_SEND_H

#include <windows.h>

/* результат отправки отчёта; приходит в WM_APP_REPORT_DONE как lParam, освобождает получатель */
typedef struct {
    int ok;
    char id[64];
    char msg[160];
} ff_report_result;

/* отправляет тело отчёта POST-запросом в фоновом потоке; тело копируется, вызывающий может сразу его менять */
void report_send_async(HWND notify, const char *url, const char *body, size_t len);

#endif
