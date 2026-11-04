#include <string.h>

#include "report.h"
#include "test.h"

static void fill(ff_report *r)
{
    memset(r, 0, sizeof *r);
    ff_config_defaults(&r->cfg);
    ff_stats_init(&r->stats);
    strcpy(r->app_version, "v0.3.0");
    strcpy(r->language, "ru");
    r->windows_build = 26100;
    strcpy(r->gpu, "NVIDIA GeForce RTX 4070");
    r->width = 3840;
    r->height = 2160;
    r->cfg.shift[1][1] = 0.25f;
    ff_stats_game_add(&r->stats, "game.exe", "fullscreen", "pause", 3600);
    r->include_system = r->include_settings = r->include_stats = 1;
}

static void test_sections(void)
{
    ff_report r;
    char buf[FF_REPORT_MAX];

    fill(&r);
    CHECK(ff_report_format(&r, buf, sizeof buf) > 0);
    CHECK(strstr(buf, "\"schema\": \"fringefix-report\"") && strstr(buf, "\"version\": \"v0.3.0\""));
    CHECK(strstr(buf, "\"windows_build\": 26100") && strstr(buf, "\"gpu\": \"NVIDIA GeForce RTX 4070\""));
    CHECK(strstr(buf, "\"green\": { \"x\": 0.000, \"y\": 0.250 }"));
    CHECK(strstr(buf, "\"exe\": \"game.exe\"") && strstr(buf, "\"minutes\": 60"));

    /* снятые галки убирают разделы целиком, остаётся только версия и комментарий */
    r.include_system = r.include_settings = r.include_stats = 0;
    CHECK(ff_report_format(&r, buf, sizeof buf) > 0);
    CHECK(!strstr(buf, "system") && !strstr(buf, "settings") && !strstr(buf, "game.exe"));
    CHECK(strstr(buf, "\"comment\": \"\""));
}

static void test_escape(void)
{
    ff_report r;
    char buf[FF_REPORT_MAX];

    fill(&r);
    strcpy(r.comment, "say \"hi\"\\ \n\ttab \x01 Привет");
    CHECK(ff_report_format(&r, buf, sizeof buf) > 0);
    CHECK(strstr(buf, "\"comment\": \"say \\\"hi\\\"\\\\ \\n\\ttab \\u0001 Привет\""));
    /* сырых управляющих символов в выводе нет, кроме переводов строки между полями */
    {
        const char *c;

        for (c = buf; *c; c++) {
            CHECK((unsigned char)*c >= 32 || *c == '\n');
        }
    }
}

static void test_private_data(void)
{
    ff_report r;
    char buf[FF_REPORT_MAX];

    fill(&r);
    /* путь в имени игры уже вычищен слоем статистики: в отчёте нет ни слешей, ни двоеточий в exe */
    ff_stats_game_add(&r.stats, "C:\\Users\\Ivan\\Desktop\\x.exe", "listed", "pause", 5);
    CHECK(ff_report_format(&r, buf, sizeof buf) > 0);
    CHECK(!strstr(buf, "Users") && !strstr(buf, "Ivan") && !strstr(buf, "C:"));
}

static void test_buffer(void)
{
    ff_report r;
    char buf[FF_REPORT_MAX];
    char tiny[64];
    int n;

    fill(&r);
    n = ff_report_format(&r, buf, sizeof buf);
    CHECK(n > 0 && (size_t)n == strlen(buf));
    /* любой урезанный буфер даёт -1, а не обрезанный JSON */
    CHECK(ff_report_format(&r, tiny, sizeof tiny) == -1);
    CHECK(ff_report_format(&r, buf, (size_t)n) == -1);
    CHECK(ff_report_format(&r, buf, (size_t)n + 1) == n);
    CHECK(ff_report_format(&r, buf, 0) == -1);
}

static void test_worst_case_fits(void)
{
    ff_report r;
    char buf[FF_REPORT_MAX];
    char name[16];
    int i;

    fill(&r);
    for (i = 0; i < FF_GAMES_MAX; i++) {
        snprintf(name, sizeof name, "game%d.exe", i);
        ff_stats_game_add(&r.stats, name, "fullscreen", "pause", 1000);
    }
    for (i = 0; i < FF_ERRORS_MAX; i++) {
        char e[FF_ERR_LEN];

        memset(e, 'x', sizeof e - 1);
        e[sizeof e - 1] = '\0';
        e[0] = (char)('a' + i);
        ff_stats_error_add(&r.stats, e);
    }
    memset(r.comment, 'q', FF_COMMENT_MAX);
    r.comment[FF_COMMENT_MAX] = '\0';
    CHECK(ff_report_format(&r, buf, sizeof buf) > 0);
}

int main(void)
{
    test_sections();
    test_escape();
    test_private_data();
    test_buffer();
    test_worst_case_fits();
    TEST_DONE("report");
}
