#include <stdio.h>
#include <string.h>

#include "config.h"
#include "test.h"

static void test_defaults(void)
{
    ff_config c;
    ff_config_defaults(&c);
    CHECK(c.shift[0][0] == 0.0f && c.shift[2][1] == 0.0f);
    CHECK(c.sharpen == 0.0f && c.monitor == 0 && c.check_updates == 1 && c.auto_pause == 1);
    CHECK(c.game_action == 0 && c.gpu_priority == 0 && c.pause_apps[0] == '\0' && c.ignore_apps[0] == '\0');
}

static void test_values(void)
{
    ff_config c;
    ff_config_defaults(&c);
    CHECK(ff_config_parse("red_y = 0.25\ngreen_y=-0.5\nblue_x=1\nsharpen=0.3\nmonitor=1\n"
                          "check_updates=0\nauto_pause=2\n", &c) == 7);
    CHECK(c.shift[0][1] == 0.25f);
    CHECK(c.shift[1][1] == -0.5f);
    CHECK(c.shift[2][0] == 1.0f);
    CHECK(c.sharpen > 0.29f && c.sharpen < 0.31f);
    CHECK(c.monitor == 1 && c.check_updates == 0 && c.auto_pause == 2);
}

static void test_clamp(void)
{
    ff_config c;
    ff_config_defaults(&c);
    CHECK(ff_config_parse("red_x=99\ngreen_x=-99\nsharpen=-5\nmonitor=-3\n", &c) == 4);
    CHECK(c.shift[0][0] == 2.0f && c.shift[1][0] == -2.0f);
    CHECK(c.sharpen == -1.0f && c.monitor == 0);
    /* минус у резкости разрешён, плюс зажимается единицей */
    CHECK(ff_config_parse("sharpen=-0.5\n", &c) == 1 && c.sharpen == -0.5f);
    CHECK(ff_config_parse("sharpen=7\n", &c) == 1 && c.sharpen == 1.0f);
    CHECK(ff_config_parse("auto_pause=9\n", &c) == 1 && c.auto_pause == 2);
    CHECK(ff_config_parse("auto_pause=-1\n", &c) == 1 && c.auto_pause == 0);
    CHECK(ff_config_parse("monitor=1000\n", &c) == 1 && c.monitor == 15);
}

static void test_game_keys(void)
{
    ff_config c;
    char text[600];

    ff_config_defaults(&c);
    CHECK(ff_config_parse("game_action=1\ngpu_priority=-3\npause_apps = a.exe;b.exe\n"
                          "ignore_apps=c.exe\n", &c) == 4);
    CHECK(c.game_action == 1 && c.gpu_priority == -3);
    CHECK(strcmp(c.pause_apps, "a.exe;b.exe") == 0 && strcmp(c.ignore_apps, "c.exe") == 0);
    /* зажим диапазонов */
    CHECK(ff_config_parse("game_action=9\ngpu_priority=-99\n", &c) == 2);
    CHECK(c.game_action == 1 && c.gpu_priority == -7);
    /* слишком длинный список отбрасывается целиком, а не режется, прежнее значение остаётся */
    strcpy(text, "pause_apps=");
    memset(text + 11, 'a', 280);
    strcpy(text + 291, "\n");
    CHECK(ff_config_parse(text, &c) == 0 && strcmp(c.pause_apps, "a.exe;b.exe") == 0);
}

static void test_junk(void)
{
    ff_config c;
    ff_config_defaults(&c);
    /* комментарии, секции, нечисловые и «хвостатые» значения, nan, неизвестные ключи */
    CHECK(ff_config_parse("; red_x=1\n# red_x=1\n[sect]\nnope\nred_x=abc\nred_x=1.5 junk\n"
                          "red_x=nan\nred_x=inf\nunknown=1\n=\n", &c) == 0);
    CHECK(c.shift[0][0] == 0.0f);
}

static void test_line_endings(void)
{
    ff_config c;
    ff_config_defaults(&c);
    /* CRLF и последняя строка без перевода строки */
    CHECK(ff_config_parse("red_x=1\r\ngreen_x=0.5", &c) == 2);
    CHECK(c.shift[0][0] == 1.0f && c.shift[1][0] == 0.5f);
}

static void test_long_line(void)
{
    ff_config c;
    char text[400];

    ff_config_defaults(&c);
    memset(text, 'a', 300);
    text[300] = '\n';
    text[301] = '\0';
    CHECK(ff_config_parse(text, &c) == 0);
}

static void test_roundtrip(void)
{
    ff_config a;
    ff_config b;
    char buf[2048];
    char tiny[16];
    int n;

    ff_config_defaults(&a);
    a.shift[0][0] = 0.25f;
    a.shift[1][1] = -0.125f;
    a.shift[2][0] = -0.5f;
    a.sharpen = 0.5f;
    a.monitor = 2;
    a.check_updates = 0;
    a.auto_pause = 2;
    a.game_action = 1;
    a.gpu_priority = -4;
    strcpy(a.pause_apps, "game.exe;x.exe");
    strcpy(a.ignore_apps, "y.exe");

    n = ff_config_format(&a, buf, sizeof buf);
    CHECK(n > 0);
    ff_config_defaults(&b);
    CHECK(ff_config_parse(buf, &b) == 14);
    CHECK(memcmp(&a, &b, sizeof a) == 0);
    /* маленький буфер не должен молча обрезать файл */
    CHECK(ff_config_format(&a, tiny, sizeof tiny) == -1);
}

int main(void)
{
    test_defaults();
    test_values();
    test_clamp();
    test_game_keys();
    test_junk();
    test_line_endings();
    test_long_line();
    test_roundtrip();
    TEST_DONE("config");
}
