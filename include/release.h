#ifndef FRINGEFIX_RELEASE_H
#define FRINGEFIX_RELEASE_H

#define FF_URL_MAX 256

/* то, что нужно знать об релизе на GitHub: тег и два файла рядом, exe и его sha256 */
typedef struct {
    char tag[32];
    char exe_url[FF_URL_MAX];
    char sha_url[FF_URL_MAX];
} ff_release;

/* "v1.2.3" в три числа; хвост после третьего ("-3-gabc1234") игнорируется; 0 или -1 */
int ff_version_parse(const char *s, int v[3]);

/* меньше нуля, ноль или больше нуля, как у strcmp */
int ff_version_cmp(const int a[3], const int b[3]);

/* достаёт тег и ссылки из JSON ответа releases/latest.
 * принимает только ссылки на github.com/<repo>/releases/download/, остальные отбрасывает.
 * 0 при успехе, -1 нет тега или плохой repo, -2 нет exe или sha256 среди файлов релиза */
int ff_release_parse(const char *json, const char *repo, ff_release *out);

#define FF_RELEASES_MAX 30

/* разбирает JSON-массив из releases (новые первыми): сохраняет релизы, у которых есть оба файла, до max штук.
 * порядок как в ответе; число сохранённых или -1 при плохом repo */
int ff_release_list_parse(const char *json, const char *repo, ff_release out[], int max);

#endif
