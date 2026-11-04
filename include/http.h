#ifndef FRINGEFIX_HTTP_H
#define FRINGEFIX_HTTP_H

#include <stddef.h>

/* GET по https через WinHTTP, редиректы следуют сами, http и ссылки с ? или # отклоняются.
 * тело кладётся в *out (malloc, на один байт длиннее с нулём в конце), его освобождает вызывающий.
 * больше max байт не читается и считается ошибкой; текст ошибки в err; 0 или -1 */
int ff_http_get(const char *url, size_t max, unsigned char **out, size_t *out_len, char *err,
                size_t err_len);

/* POST JSON по https; тело уходит как есть, ответ читается так же, как у GET; 0 при коде 2xx, иначе -1 */
int ff_http_post_json(const char *url, const void *body, size_t body_len, size_t max,
                      unsigned char **out, size_t *out_len, char *err, size_t err_len);

#endif
