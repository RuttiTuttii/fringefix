#include "urllist.h"

#include <string.h>

static int is_sep(char c)
{
    return c == ' ' || c == ',' || c == ';' || c == '\t' || c == '\n' || c == '\r';
}

int ff_urllist_split(const char *text, char out[][FF_URL_ITEM], int max, int reverse)
{
    int n = 0;
    int i;
    const char *p = text;

    while (*p && n < max) {
        size_t len = 0;
        const char *start;

        while (is_sep(*p)) {
            p++;
        }
        start = p;
        while (*p && !is_sep(*p)) {
            p++;
        }
        len = (size_t)(p - start);
        if (len > 0 && len < FF_URL_ITEM && strncmp(start, "https://", 8) == 0 && len > 8) {
            memcpy(out[n], start, len);
            out[n][len] = '\0';
            n++;
        }
    }
    if (reverse) {
        for (i = 0; i < n / 2; i++) {
            char tmp[FF_URL_ITEM];

            memcpy(tmp, out[i], FF_URL_ITEM);
            memcpy(out[i], out[n - 1 - i], FF_URL_ITEM);
            memcpy(out[n - 1 - i], tmp, FF_URL_ITEM);
        }
    }
    return n;
}
