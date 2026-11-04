#ifndef FRINGEFIX_REPORT_ACTIONS_H
#define FRINGEFIX_REPORT_ACTIONS_H

#include <windows.h>

#include "app.h"
#include "report.h"

/* собирает отчёт из текущих настроек, статистики и сведений о системе и пишет его JSON в payload;
 * comment уже в UTF-8, не длиннее FF_COMMENT_MAX байт. возвращает длину или -1, если не влезло */
int report_compose(const ff_app *a, const char *comment, int sys, int settings, int stats,
                   char *payload, size_t n);

/* комментарий из поля ввода в UTF-8; не помещающееся в лимит отбрасывается с конца по символам */
void report_read_comment(HWND edit, int max_chars, char out[FF_COMMENT_MAX + 1]);

/* те же байты для окна предпросмотра: UTF-16 с переводами строк Windows */
void report_preview_text(const char *payload, int len, wchar_t *out, size_t out_chars);

/* текст в буфер обмена */
void report_copy(HWND owner, const wchar_t *text);

/* диалог сохранения и запись байтов отчёта; 1 если файл записан */
int report_save_file(HWND owner, const char *payload, int len);

/* открывает в браузере новый issue репозитория с заголовком и короткой просьбой вставить отчёт */
void report_open_issue(HWND owner, const wchar_t *title, const wchar_t *body);

#endif
