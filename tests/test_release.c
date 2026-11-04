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

int main(void)
{
    test_version();
    test_parse_ok();
    test_parse_rejects();
    test_parse_repo_validation();
    test_parse_long_url();
    TEST_DONE("release");
}
