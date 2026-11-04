#include <string.h>

#include "test.h"
#include "urllist.h"

int main(void)
{
    char u[FF_URLS_MAX][FF_URL_ITEM];
    char longurl[400];

    /* разные разделители и лишние пробелы */
    CHECK(ff_urllist_split(" https://a.example/x,https://b.example/y;\thttps://c.example/z \n", u,
                           FF_URLS_MAX, 0) == 3);
    CHECK(strcmp(u[0], "https://a.example/x") == 0 && strcmp(u[2], "https://c.example/z") == 0);

    /* обратный порядок для английского интерфейса */
    CHECK(ff_urllist_split("https://a.example https://b.example", u, FF_URLS_MAX, 1) == 2);
    CHECK(strcmp(u[0], "https://b.example") == 0 && strcmp(u[1], "https://a.example") == 0);
    CHECK(ff_urllist_split("https://a https://b https://c", u, FF_URLS_MAX, 1) == 3);
    CHECK(strcmp(u[0], "https://c") == 0 && strcmp(u[1], "https://b") == 0 && strcmp(u[2], "https://a") == 0);

    /* только https, пустые и слишком длинные пропускаются */
    CHECK(ff_urllist_split("http://plain.example ftp://x https:// https://ok.example", u, FF_URLS_MAX, 0) == 1);
    CHECK(strcmp(u[0], "https://ok.example") == 0);
    CHECK(ff_urllist_split("", u, FF_URLS_MAX, 0) == 0);
    CHECK(ff_urllist_split("   ,;  ", u, FF_URLS_MAX, 0) == 0);
    memset(longurl, 'a', sizeof longurl);
    memcpy(longurl, "https://", 8);
    longurl[sizeof longurl - 1] = '\0';
    CHECK(ff_urllist_split(longurl, u, FF_URLS_MAX, 0) == 0);

    /* потолок по числу */
    CHECK(ff_urllist_split("https://1 https://2 https://3 https://4 https://5", u, FF_URLS_MAX, 0) == FF_URLS_MAX);
    TEST_DONE("urllist");
}
