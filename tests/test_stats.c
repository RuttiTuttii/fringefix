#include <string.h>

#include "stats.h"
#include "test.h"

static void test_games(void)
{
    ff_stats s;

    ff_stats_init(&s);
    CHECK(ff_stats_game_add(&s, "EldenRing.exe", "fullscreen", "pause", 3600));
    CHECK(ff_stats_game_add(&s, "eldenring.EXE", "3d", "light", 600));
    /* то же имя в другом регистре сливается в одну запись, метки берутся последние */
    CHECK(s.ngames == 1 && s.games[0].sessions == 2 && s.games[0].seconds == 4200);
    CHECK(strcmp(s.games[0].exe, "eldenring.exe") == 0 && strcmp(s.games[0].how, "3d") == 0);
    CHECK(strcmp(s.games[0].action, "light") == 0);

    /* путь, юникод и разделители превращаются в подчёркивания, пустое имя отбрасывается */
    CHECK(ff_stats_game_add(&s, "C:\\Users\\Ivan\\game.exe", "listed", "pause", 1));
    CHECK(strchr(s.games[1].exe, '\\') == NULL && strchr(s.games[1].exe, ':') == NULL);
    CHECK(!ff_stats_game_add(&s, "", "listed", "pause", 1));
}

static void test_limits(void)
{
    ff_stats s;
    char name[16];
    int i;

    ff_stats_init(&s);
    for (i = 0; i < FF_GAMES_MAX; i++) {
        snprintf(name, sizeof name, "g%d.exe", i);
        CHECK(ff_stats_game_add(&s, name, "fullscreen", "pause", 1));
    }
    /* таблица полна: новая игра не добавляется, старая продолжает копиться */
    CHECK(!ff_stats_game_add(&s, "extra.exe", "fullscreen", "pause", 1));
    CHECK(ff_stats_game_add(&s, "g0.exe", "fullscreen", "pause", 1) && s.games[0].sessions == 2);

    /* секунды не переполняются */
    ff_stats_init(&s);
    ff_stats_game_add(&s, "a.exe", "x", "y", 4000000000u);
    ff_stats_game_add(&s, "a.exe", "x", "y", 4000000000u);
    CHECK(s.games[0].seconds == 4294967295u);
}

static void test_errors(void)
{
    ff_stats s;
    int i;

    ff_stats_init(&s);
    ff_stats_error_add(&s, "CreateSwapChainForHwnd failed (0x887a0001)");
    ff_stats_error_add(&s, "CreateSwapChainForHwnd failed (0x887a0001)");
    ff_stats_error_add(&s, "bad|line\nwith\tcontrol");
    ff_stats_error_add(&s, "");
    CHECK(s.nerrors == 2);
    CHECK(strchr(s.errors[1], '|') == NULL && strchr(s.errors[1], '\n') == NULL);
    /* не больше FF_ERRORS_MAX разных */
    for (i = 0; i < 20; i++) {
        char t[16];

        snprintf(t, sizeof t, "e%d", i);
        ff_stats_error_add(&s, t);
    }
    CHECK(s.nerrors == FF_ERRORS_MAX);
}

static void test_roundtrip(void)
{
    ff_stats a;
    ff_stats b;
    char buf[4096];

    ff_stats_init(&a);
    a.uptime_min = 120;
    a.frames_drawn = 5000;
    a.frames_echo_skipped = 4000;
    a.device_resets = 2;
    a.pauses = 7;
    ff_stats_error_add(&a, "DuplicateOutput1 failed (0x80070005)");
    ff_stats_game_add(&a, "game.exe", "fullscreen", "pause", 100);
    ff_stats_game_add(&a, "game.exe", "fullscreen", "pause", 50);

    CHECK(ff_stats_format(&a, buf, sizeof buf) > 0);
    ff_stats_init(&b);
    CHECK(ff_stats_parse(buf, &b) == 7);
    CHECK(memcmp(&a, &b, sizeof a) == 0);

    /* повторная загрузка складывает счётчики, а не затирает */
    CHECK(ff_stats_parse(buf, &b) == 7 && b.frames_drawn == 10000 && b.games[0].sessions == 4);

    /* буфер мал: -1, без записи за пределы */
    CHECK(ff_stats_format(&a, buf, 10) == -1);
    CHECK(ff_stats_format(&a, buf, 0) == -1);
}

static void test_parse_junk(void)
{
    ff_stats s;

    ff_stats_init(&s);
    CHECK(ff_stats_parse("pauses=abc\nunknown=1\ngame=a.exe|x\ngame=a.exe|x|y|0|5\n=\nnoeq\n"
                         "game=b.exe|fullscreen|pause|2|60\r\n", &s) == 1);
    CHECK(s.ngames == 1 && s.games[0].sessions == 2 && s.pauses == 0);
}

int main(void)
{
    test_games();
    test_limits();
    test_errors();
    test_roundtrip();
    test_parse_junk();
    TEST_DONE("stats");
}
