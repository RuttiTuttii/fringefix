#include "release.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define VERSION_PART_MAX 99999

static int read_num(const char **p, int *out)
{
    int n = 0;
    int digits = 0;

    while (**p >= '0' && **p <= '9') {
        n = n * 10 + (**p - '0');
        if (n > VERSION_PART_MAX) {
            return -1;
        }
        (*p)++;
        digits++;
    }
    if (!digits) {
        return -1;
    }
    *out = n;
    return 0;
}

/* три числа и указатель на то, что идёт за ними */
static int parse3(const char *s, int v[3], const char **end)
{
    int i;

    if (*s == 'v' || *s == 'V') {
        s++;
    }
    for (i = 0; i < 3; i++) {
        if (read_num(&s, &v[i]) != 0) {
            return -1;
        }
        if (i < 2) {
            if (*s != '.') {
                return -1;
            }
            s++;
        }
    }
    *end = s;
    return 0;
}

int ff_version_parse(const char *s, int v[3])
{
    const char *end;

    return parse3(s, v, &end);
}

int ff_version_cmp(const int a[3], const int b[3])
{
    int i;

    for (i = 0; i < 3; i++) {
        if (a[i] != b[i]) {
            return a[i] < b[i] ? -1 : 1;
        }
    }
    return 0;
}

int ff_version_beta(const char *s)
{
    int v[3];
    const char *end;
    int n = 0;

    if (parse3(s, v, &end) != 0 || strncmp(end, "-beta.", 6) != 0) {
        return 0;
    }
    end += 6;
    return read_num(&end, &n) == 0 ? n : 0;
}

int ff_tag_cmp(const char *a, const char *b)
{
    int va[3];
    int vb[3];
    int ba;
    int bb;
    int c;

    if (ff_version_parse(a, va) != 0 || ff_version_parse(b, vb) != 0) {
        return 0;
    }
    c = ff_version_cmp(va, vb);
    if (c != 0) {
        return c;
    }
    /* бета предшествует релизу с тем же номером, беты между собой идут по своему номеру */
    ba = ff_version_beta(a);
    bb = ff_version_beta(b);
    if (ba == bb) {
        return 0;
    }
    if (ba == 0 || bb == 0) {
        return ba == 0 ? 1 : -1;
    }
    return ba < bb ? -1 : 1;
}

static int starts_with_ci(const char *s, const char *prefix)
{
    for (; *prefix; s++, prefix++) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) {
            return 0;
        }
    }
    return 1;
}

static int ends_with(const char *s, const char *suffix)
{
    size_t a = strlen(s);
    size_t b = strlen(suffix);

    return a >= b && strcmp(s + a - b, suffix) == 0;
}

/* значение строкового поля key в [from, end); экранирование не поддерживается, такие строки отбрасываются.
 * возвращает указатель за закрывающей кавычкой или NULL */
static const char *json_string(const char *from, const char *end, const char *key, char *out,
                               size_t n)
{
    char pat[48];
    const char *p;
    size_t i = 0;

    if (snprintf(pat, sizeof pat, "\"%s\"", key) >= (int)sizeof pat) {
        return NULL;
    }
    p = strstr(from, pat);
    if (!p || p + strlen(pat) >= end) {
        return NULL;
    }
    p += strlen(pat);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    if (*p != ':') {
        return NULL;
    }
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    if (*p != '"') {
        return NULL;
    }
    p++;
    while (p < end && *p && *p != '"') {
        if (*p == '\\' || i + 1 >= n) {
            return NULL;
        }
        out[i++] = *p++;
    }
    if (p >= end || *p != '"') {
        return NULL;
    }
    out[i] = '\0';
    return p + 1;
}

/* repo это owner/name из букв, цифр, точки, дефиса и подчёркивания, ровно один слеш */
static int repo_ok(const char *repo)
{
    int slashes = 0;
    const char *p;

    for (p = repo; *p; p++) {
        if (*p == '/') {
            slashes++;
        } else if (!isalnum((unsigned char)*p) && *p != '.' && *p != '-' && *p != '_') {
            return 0;
        }
    }
    return slashes == 1 && repo[0] != '/' && p[-1] != '/';
}

static int tag_ok(const char *tag)
{
    const char *p;

    for (p = tag; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '.' && *p != '-' && *p != '_') {
            return 0;
        }
    }
    return p != tag;
}

/* префикс, с которого обязана начинаться каждая принятая ссылка на файл релиза */
static int make_prefix(const char *repo, char *prefix, size_t n)
{
    return repo_ok(repo)
           && snprintf(prefix, n, "https://github.com/%s/releases/download/", repo) < (int)n;
}

/* один релиз: тег и файлы в [from, end) */
static int parse_segment(const char *from, const char *end, const char *prefix, ff_release *out)
{
    char url[FF_URL_MAX];
    const char *p;

    memset(out, 0, sizeof *out);
    if (!json_string(from, end, "tag_name", out->tag, sizeof out->tag) || !tag_ok(out->tag)) {
        return -1;
    }
    for (p = from; (p = json_string(p, end, "browser_download_url", url, sizeof url)) != NULL;) {
        if (!starts_with_ci(url, prefix)) {
            continue;
        }
        if (ends_with(url, "/fringefix.exe.sha256")) {
            snprintf(out->sha_url, sizeof out->sha_url, "%s", url);
        } else if (ends_with(url, "/fringefix.exe")) {
            snprintf(out->exe_url, sizeof out->exe_url, "%s", url);
        }
    }
    return (out->exe_url[0] && out->sha_url[0]) ? 0 : -2;
}

int ff_release_parse(const char *json, const char *repo, ff_release *out)
{
    char prefix[160];

    memset(out, 0, sizeof *out);
    if (!make_prefix(repo, prefix, sizeof prefix)) {
        return -1;
    }
    return parse_segment(json, json + strlen(json), prefix, out);
}

int ff_release_list_parse(const char *json, const char *repo, ff_release out[], int max)
{
    char prefix[160];
    const char *p = json;
    int n = 0;

    if (!make_prefix(repo, prefix, sizeof prefix)) {
        return -1;
    }
    /* у каждого релиза tag_name идёт раньше его файлов, поэтому границей служит следующий tag_name */
    while (n < max && (p = strstr(p, "\"tag_name\"")) != NULL) {
        const char *next = strstr(p + 1, "\"tag_name\"");
        const char *end = next ? next : json + strlen(json);

        if (parse_segment(p, end, prefix, &out[n]) == 0) {
            n++;
        }
        p = end;
    }
    return n;
}
