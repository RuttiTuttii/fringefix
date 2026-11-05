#include <string.h>

#include "test.h"
#include "winstate.h"

static void test_format(void)
{
    ff_winstate w;
    char buf[64];

    memset(&w, 0, sizeof w);
    CHECK(ff_winstate_format(&w, buf, sizeof buf) == 0 && buf[0] == '\0');
    w.open[FF_WIN_SETTINGS] = 1;
    w.x[FF_WIN_SETTINGS] = 10;
    w.y[FF_WIN_SETTINGS] = 20;
    w.open[FF_WIN_APPS] = 1;
    w.x[FF_WIN_APPS] = -1920;
    w.y[FF_WIN_APPS] = 0;
    CHECK(ff_winstate_format(&w, buf, sizeof buf) > 0 && strcmp(buf, "s=10,20;a=-1920,0") == 0);
    /* маленький буфер не даёт обрезанной строки */
    CHECK(ff_winstate_format(&w, buf, 8) == -1 && buf[0] == '\0');
    CHECK(ff_winstate_format(&w, buf, 0) == -1);
}

static void test_parse(void)
{
    ff_winstate w;
    ff_winstate r;
    char buf[64];

    memset(&w, 0, sizeof w);
    memset(&r, 0, sizeof r);
    w.open[FF_WIN_PROGRAM] = w.open[FF_WIN_SETTINGS] = 1;
    w.x[FF_WIN_PROGRAM] = -5;
    w.y[FF_WIN_PROGRAM] = 700;
    w.x[FF_WIN_SETTINGS] = 3;
    w.y[FF_WIN_SETTINGS] = 4;
    ff_winstate_format(&w, buf, sizeof buf);
    CHECK(ff_winstate_parse(buf, &r) == 2);
    CHECK(r.open[FF_WIN_SETTINGS] && r.x[FF_WIN_SETTINGS] == 3 && r.y[FF_WIN_SETTINGS] == 4);
    CHECK(r.open[FF_WIN_PROGRAM] && r.x[FF_WIN_PROGRAM] == -5 && r.y[FF_WIN_PROGRAM] == 700);
    CHECK(!r.open[FF_WIN_APPS]);

    /* мусор, неизвестные ключи и огромные числа пропускаются, годные окна остаются */
    memset(&r, 0, sizeof r);
    CHECK(ff_winstate_parse("x=1,2;;s=;p=9,9,9;a=99999999999,1;s=7,8", &r) == 2);
    CHECK(r.open[FF_WIN_SETTINGS] && r.x[FF_WIN_SETTINGS] == 7 && r.y[FF_WIN_SETTINGS] == 8);
    CHECK(r.open[FF_WIN_PROGRAM] && r.x[FF_WIN_PROGRAM] == 9 && r.y[FF_WIN_PROGRAM] == 9);
    CHECK(!r.open[FF_WIN_APPS]);
    memset(&r, 0, sizeof r);
    CHECK(ff_winstate_parse("", &r) == 0 && ff_winstate_parse("garbage", &r) == 0);
}

int main(void)
{
    test_format();
    test_parse();
    TEST_DONE("winstate");
}
