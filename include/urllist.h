#ifndef FRINGEFIX_URLLIST_H
#define FRINGEFIX_URLLIST_H

#include <stddef.h>

#define FF_URLS_MAX 4
#define FF_URL_ITEM 256

/* делит строку адресов, разделённых пробелами, запятыми или точками с запятой, на отдельные адреса.
 * слишком длинные адреса и всё, что не начинается с https://, пропускаются.
 * возвращает число адресов; порядок как в строке, а если reverse не ноль, то обратный */
int ff_urllist_split(const char *text, char out[][FF_URL_ITEM], int max, int reverse);

#endif
