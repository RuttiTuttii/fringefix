#ifndef FRINGEFIX_HTTP_H
#define FRINGEFIX_HTTP_H

#include <stddef.h>

/* GET по https через WinHTTP, редиректы следуют сами, http и ссылки с ? или # отклоняются.
 * тело кладётся в *out (malloc, на один байт длиннее с нулём в конце), его освобождает вызывающий.
 * больше max байт не читается и считается ошибкой; текст ошибки в err; 0 или -1 */
int ff_http_get(const char *url, size_t max, unsigned char **out, size_t *out_len, char *err,
                size_t err_len);

#endif
