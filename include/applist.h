#ifndef FRINGEFIX_APPLIST_H
#define FRINGEFIX_APPLIST_H

#include <stddef.h>
#include <wchar.h>

/* известные не игры: браузеры, редакторы, оболочка, мессенджеры, офис. нужны автопаузе по 3D, где признаком
 * игры служит загруженный графический API, а его грузят и эти программы.
 * принимает имя exe или путь к нему, регистр не важен; 1 если программа в списке */
int ff_app_is_known_nongame(const wchar_t *exe);

/* есть ли программа в списке имён через точку с запятой; регистр не важен, пробелы по краям срезаются,
 * имя без .exe в списке тоже подходит. exe может быть путём */
int ff_app_list_has(const char *list, const wchar_t *exe);

/* дописывает имя exe в конец списка (буфер на n байт); 1 добавлено, 0 уже было, -1 не поместилось
 * или в имени есть не ASCII символы: такой элемент в списке не совпал бы ни с чем */
int ff_app_list_add(char *list, size_t n, const wchar_t *exe);

/* убирает имя из списка; 1 если было, 0 если не было. список не длиннее FF_APPS_MAX из config.h */
int ff_app_list_remove(char *list, size_t n, const wchar_t *exe);

/* index-й непустой элемент списка без пробелов по краям, регистр как записан;
 * 1 если он есть и поместился в out (n байт), 0 если элементов меньше или имя не влезло */
int ff_app_list_item(const char *list, int index, char *out, size_t n);

/* подходит ли запись окна под строку поиска: часть имени процесса или часть заголовка окна, регистр не важен,
 * пробелы по краям запроса срезаются; пустой запрос подходит всему. title может быть NULL */
int ff_app_query_match(const wchar_t *query, const wchar_t *exe, const wchar_t *title);

#endif
