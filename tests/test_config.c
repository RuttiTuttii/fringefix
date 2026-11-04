#include <string.h>

#include "config.h"
#include "test.h"

static void test_defaults(void)
{
    ff_config c;
    ff_config_defaults(&c);
    CHECK(c.shift[0][0] == 0.0f && c.shift[2][1] == 0.0f);
    CHECK(c.sharpen == 0.0f && c.monitor == 0 && c.check_updates == 1);
}

static void test_values(void)
{
    ff_config c;
    ff_config_defaults(&c);
    CHECK(ff_config_parse("red_y = 0.25\ngreen_y=-0.5\nblue_x=1\nsharpen=0.3\nmonitor=1\n"
                          "check_updates=0\n", &c) == 6);
    CHECK(c.shift[0][1] == 0.25f);
    CHECK(c.shift[1][1] == -0.5f);
    CHECK(c.shift[2][0] == 1.0f);
    CHECK(c.sharpen > 0.29f && c.sharpen < 0.31f);
    CHECK(c.monitor == 1 && c.check_updates == 0);
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
    CHECK(ff_config_parse("monitor=1000\n", &c) == 1 && c.monitor == 15);
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
    char buf[1024];
    char tiny[16];
    int n;

    ff_config_defaults(&a);
    a.shift[0][0] = 0.25f;
    a.shift[1][1] = -0.125f;
    a.shift[2][0] = -0.5f;
    a.sharpen = 0.5f;
    a.monitor = 2;
    a.check_updates = 0;

    n = ff_config_format(&a, buf, sizeof buf);
    CHECK(n > 0);
    ff_config_defaults(&b);
    CHECK(ff_config_parse(buf, &b) == 9);
    CHECK(memcmp(&a, &b, sizeof a) == 0);
    /* маленький буфер не должен молча обрезать файл */
    CHECK(ff_config_format(&a, tiny, sizeof tiny) == -1);
}

int main(void)
{
    test_defaults();
    test_values();
    test_clamp();
    test_junk();
    test_line_endings();
    test_long_line();
    test_roundtrip();
    TEST_DONE("config");
}
