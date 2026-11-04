#ifndef FRINGEFIX_APPLIST_H
#define FRINGEFIX_APPLIST_H

#include <wchar.h>

/* известные не игры: браузеры, редакторы, оболочка, мессенджеры, офис. нужны автопаузе по 3D, где признаком
 * игры служит загруженный графический API, а его грузят и эти программы.
 * принимает имя exe или путь к нему, регистр не важен; 1 если программа в списке */
int ff_app_is_known_nongame(const wchar_t *exe);

#endif
