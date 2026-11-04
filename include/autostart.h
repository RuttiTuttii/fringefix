#ifndef FRINGEFIX_AUTOSTART_H
#define FRINGEFIX_AUTOSTART_H

/* автозапуск через ключ HKCU\Software\Microsoft\Windows\CurrentVersion\Run: права администратора не нужны.
 * состояние читается из реестра, а не из ini, чтобы галочка не расходилась с тем, что видит Windows */

/* 1, если запись есть и указывает на этот exe */
int autostart_get(void);

/* включает или выключает запись; 0 или -1 */
int autostart_set(int on);

#endif
