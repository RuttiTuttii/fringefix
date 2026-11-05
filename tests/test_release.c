#include <string.h>

#include "release.h"
#include "test.h"

#define BASE "https://github.com/Own/Repo/releases/download/v1.2.3/"

static const char GOOD[] =
    "{\"url\":\"x\",\"tag_name\":\"v1.2.3\",\"name\":\"v1.2.3\",\"assets\":["
    "{\"name\":\"fringefix.exe\",\"browser_download_url\":\"" BASE "fringefix.exe\"},"
    "{\"name\":\"fringefix.exe.sha256\",\"browser_download_url\":\"" BASE "fringefix.exe.sha256\"}]}";

static void test_version(void)
{
    int a[3];
    int b[3];

    CHECK(ff_version_parse("v1.2.3", a) == 0 && a[0] == 1 && a[1] == 2 && a[2] == 3);
    /* вывод git describe: хвост после третьего числа игнорируется */
    CHECK(ff_version_parse("v0.10.4-3-gabc1234-dirty", b) == 0 && b[1] == 10 && b[2] == 4);
    CHECK(ff_version_cmp(a, b) > 0);
    CHECK(ff_version_cmp(b, a) < 0);
    CHECK(ff_version_cmp(a, a) == 0);
    /* десять больше девяти, а не меньше, как при сравнении строк */
    CHECK(ff_version_parse("v1.9.0", a) == 0 && ff_version_parse("v1.10.0", b) == 0);
    CHECK(ff_version_cmp(a, b) < 0);

    CHECK(ff_version_parse("dev", a) == -1);
    CHECK(ff_version_parse("v1.2", a) == -1);
    CHECK(ff_version_parse("v1..2", a) == -1);
    CHECK(ff_version_parse("", a) == -1);
    CHECK(ff_version_parse("v1.2.99999999999", a) == -1);
}

static void test_beta(void)
{
    CHECK(ff_version_beta("v1.2.3-beta.4") == 4);
    CHECK(ff_version_beta("v1.2.3-beta.12-3-gabc1234") == 12);
    CHECK(ff_version_beta("v1.2.3") == 0 && ff_version_beta("v1.2.3-3-gabc1234") == 0);
    CHECK(ff_version_beta("v1.2.3-beta.") == 0 && ff_version_beta("v1.2.3-beta") == 0);
    CHECK(ff_version_beta("dev") == 0);

    /* бета старее релиза с тем же номером и новее предыдущего релиза */
    CHECK(ff_tag_cmp("v1.2.3-beta.1", "v1.2.3") < 0);
    CHECK(ff_tag_cmp("v1.2.3", "v1.2.3-beta.9") > 0);
    CHECK(ff_tag_cmp("v1.2.3-beta.1", "v1.2.2") > 0);
    CHECK(ff_tag_cmp("v1.2.3-beta.2", "v1.2.3-beta.10") < 0);
    CHECK(ff_tag_cmp("v1.2.3-beta.2", "v1.2.3-beta.2-5-gabc") == 0);
    CHECK(ff_tag_cmp("v1.3.0-beta.1", "v1.2.9") > 0);
    /* неразбираемый тег не считается ни новее, ни старее */
    CHECK(ff_tag_cmp("dev", "v1.2.3") == 0 && ff_tag_cmp("v1.2.3", "v1.2") == 0);
}

static void test_parse_ok(void)
{
    ff_release r;

    /* repo сравнивается без учёта регистра: GitHub отдаёт канонический */
    CHECK(ff_release_parse(GOOD, "own/repo", &r) == 0);
    CHECK(strcmp(r.tag, "v1.2.3") == 0);
    CHECK(strcmp(r.exe_url, BASE "fringefix.exe") == 0);
    CHECK(strcmp(r.sha_url, BASE "fringefix.exe.sha256") == 0);
}

static void test_parse_rejects(void)
{
    ff_release r;
    static const char foreign[] =
        "{\"tag_name\":\"v1.0.0\",\"assets\":["
        "{\"browser_download_url\":\"https://evil.example/Own/Repo/releases/download/v1.0.0/fringefix.exe\"},"
        "{\"browser_download_url\":\"https://evil.example/Own/Repo/releases/download/v1.0.0/fringefix.exe.sha256\"}]}";
    static const char other_repo[] =
        "{\"tag_name\":\"v1.0.0\",\"assets\":["
        "{\"browser_download_url\":\"https://github.com/Other/Repo/releases/download/v1.0.0/fringefix.exe\"},"
        "{\"browser_download_url\":\"https://github.com/Other/Repo/releases/download/v1.0.0/fringefix.exe.sha256\"}]}";
    static const char no_sha[] =
        "{\"tag_name\":\"v1.0.0\",\"assets\":["
        "{\"browser_download_url\":\"https://github.com/Own/Repo/releases/download/v1.0.0/fringefix.exe\"}]}";
    static const char bad_tag[] = "{\"tag_name\":\"v1.0.0/../x\",\"assets\":[]}";

    /* чужой хост и чужой репозиторий не принимаются */
    CHECK(ff_release_parse(foreign, "own/repo", &r) == -2);
    CHECK(ff_release_parse(other_repo, "own/repo", &r) == -2);
    CHECK(ff_release_parse(no_sha, "own/repo", &r) == -2);
    CHECK(ff_release_parse(bad_tag, "own/repo", &r) == -1);
    CHECK(ff_release_parse("{}", "own/repo", &r) == -1);
    CHECK(ff_release_parse("", "own/repo", &r) == -1);
}

static void test_parse_repo_validation(void)
{
    ff_release r;

    CHECK(ff_release_parse(GOOD, "", &r) == -1);
    CHECK(ff_release_parse(GOOD, "own", &r) == -1);
    CHECK(ff_release_parse(GOOD, "own/repo/x", &r) == -1);
    CHECK(ff_release_parse(GOOD, "/repo", &r) == -1);
    CHECK(ff_release_parse(GOOD, "own/", &r) == -1);
    CHECK(ff_release_parse(GOOD, "own/re po", &r) == -1);
    CHECK(ff_release_parse(GOOD, "own/repo?x=1", &r) == -1);
}

static void test_parse_long_url(void)
{
    ff_release r;
    char json[1024];
    char pad[400];

    /* ссылка длиннее буфера отбрасывается, а не обрезается */
    memset(pad, 'a', sizeof pad - 1);
    pad[sizeof pad - 1] = '\0';
    snprintf(json, sizeof json,
             "{\"tag_name\":\"v1.0.0\",\"assets\":[{\"browser_download_url\":\"%s%s/fringefix.exe\"}]}",
             BASE, pad);
    CHECK(ff_release_parse(json, "own/repo", &r) == -2);
}

#define B2 "https://github.com/Own/Repo/releases/download/v1.1.0/"
#define B3 "https://github.com/Own/Repo/releases/download/v1.0.0/"

static const char LIST[] =
    "[{\"tag_name\":\"v1.2.3\",\"assets\":["
    "{\"browser_download_url\":\"" BASE "fringefix.exe\"},"
    "{\"browser_download_url\":\"" BASE "fringefix.exe.sha256\"}],\"body\":\"notes\"},"
    /* у этого релиза нет файла хеша, в список он не попадёт */
    "{\"tag_name\":\"v1.1.0\",\"assets\":["
    "{\"browser_download_url\":\"" B2 "fringefix.exe\"}]},"
    "{\"tag_name\":\"v1.0.0\",\"assets\":["
    "{\"browser_download_url\":\"" B3 "fringefix.exe\"},"
    "{\"browser_download_url\":\"" B3 "fringefix.exe.sha256\"}]}]";

static void test_list(void)
{
    ff_release r[FF_RELEASES_MAX];

    CHECK(ff_release_list_parse(LIST, "own/repo", r, FF_RELEASES_MAX) == 2);
    CHECK(strcmp(r[0].tag, "v1.2.3") == 0 && strcmp(r[0].exe_url, BASE "fringefix.exe") == 0);
    /* файлы привязаны к своему релизу, а не к соседнему */
    CHECK(strcmp(r[1].tag, "v1.0.0") == 0 && strcmp(r[1].sha_url, B3 "fringefix.exe.sha256") == 0);

    /* потолок по числу, пустой ответ, плохой repo */
    CHECK(ff_release_list_parse(LIST, "own/repo", r, 1) == 1 && strcmp(r[0].tag, "v1.2.3") == 0);
    CHECK(ff_release_list_parse("[]", "own/repo", r, FF_RELEASES_MAX) == 0);
    CHECK(ff_release_list_parse("", "own/repo", r, FF_RELEASES_MAX) == 0);
    CHECK(ff_release_list_parse(LIST, "own", r, FF_RELEASES_MAX) == -1);
    /* чужой репозиторий не даёт ни одной записи */
    CHECK(ff_release_list_parse(LIST, "other/repo", r, FF_RELEASES_MAX) == 0);
}

int main(void)
{
    test_version();
    test_beta();
    test_parse_ok();
    test_parse_rejects();
    test_parse_repo_validation();
    test_parse_long_url();
    test_list();
    TEST_DONE("release");
}
