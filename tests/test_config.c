#include <stdio.h>
#include <string.h>

#include "config.h"
#include "test.h"

static void test_defaults(void)
{
    ff_config c;
    ff_config_defaults(&c);
    CHECK(c.view[0].shift[0][0] == 0.0f && c.view[0].shift[2][1] == 0.0f);
    /* работает только нулевой монитор, автопауза в полноэкранных и 3D-приложениях, движок 2 */
    CHECK(c.view[0].enabled == 1 && c.view[1].enabled == 0 && c.view[FF_MAX_MONITORS - 1].enabled == 0);
    CHECK(c.view[0].sharpen == 0.0f && c.monitor == 0 && c.check_updates == 1 && c.auto_pause == 2 && c.engine == 2);
    CHECK(c.gpu_priority == 0 && c.language == 0 && c.theme == 0 && c.stats == 0 && c.pause_apps[0] == '\0' && c.ignore_apps[0] == '\0');
}

static void test_engine(void)
{
    ff_config c;
    ff_config b;
    char buf[4096];

    ff_config_defaults(&c);
    CHECK(c.engine == 2);
    CHECK(ff_config_parse("engine = 1\n", &c) == 1 && c.engine == 1);
    CHECK(ff_config_parse("engine = 2\n", &c) == 1 && c.engine == 2);
    /* вне диапазона зажимается, мусор отбрасывается */
    CHECK(ff_config_parse("engine = 9\n", &c) == 1 && c.engine == 2);
    CHECK(ff_config_parse("engine = -4\n", &c) == 1 && c.engine == 0);
    CHECK(ff_config_parse("engine = x\n", &c) == 0 && c.engine == 0);
    c.engine = 1;
    CHECK(ff_config_format(&c, buf, sizeof buf) > 0);
    ff_config_defaults(&b);
    ff_config_parse(buf, &b);
    CHECK(b.engine == 1);

    /* способ вывода: по умолчанию с синхронизацией, диапазон 0..1, круг запись-чтение */
    ff_config_defaults(&c);
    CHECK(c.present_mode == 0);
    CHECK(ff_config_parse("present_mode = 1\n", &c) == 1 && c.present_mode == 1);
    CHECK(ff_config_parse("present_mode = 7\n", &c) == 1 && c.present_mode == 1);
    CHECK(ff_config_parse("present_mode = -2\n", &c) == 1 && c.present_mode == 0);
    CHECK(ff_config_parse("present_mode = x\n", &c) == 0 && c.present_mode == 0);
    c.present_mode = 1;
    CHECK(ff_config_format(&c, buf, sizeof buf) > 0);
    ff_config_defaults(&b);
    ff_config_parse(buf, &b);
    CHECK(b.present_mode == 1);
}

static void test_hud_corner(void)
{
    ff_config c;

    ff_config_defaults(&c);
    CHECK(c.hud_corner == 0);
    CHECK(ff_config_parse("hud_corner = 2\n", &c) == 1 && c.hud_corner == 2);
    /* вне диапазона зажимается, мусор отбрасывается */
    CHECK(ff_config_parse("hud_corner = 9\n", &c) == 1 && c.hud_corner == 3);
    CHECK(ff_config_parse("hud_corner = -1\n", &c) == 1 && c.hud_corner == 0);
    CHECK(ff_config_parse("hud_corner = x\n", &c) == 0 && c.hud_corner == 0);
}

static void test_hotkeys_and_beta(void)
{
    ff_config c;
    ff_config b;
    char buf[4096];

    ff_config_defaults(&c);
    CHECK(c.beta == 0);
    CHECK(c.hotkey_toggle.vk == 0x79 && c.hotkey_toggle.mods == 0);
    CHECK(c.hotkey_quit.vk == 0x7A && c.hotkey_quit.mods == 0);

    CHECK(ff_config_parse("hotkey_toggle = Ctrl+Alt+F\nhotkey_quit = none\nbeta = 1\n", &c) == 3);
    CHECK(c.hotkey_toggle.mods == (FF_MOD_CTRL | FF_MOD_ALT) && c.hotkey_toggle.vk == 'F');
    CHECK(c.hotkey_quit.vk == 0 && c.beta == 1);

    /* плохое сочетание отбрасывается, прежнее остаётся */
    CHECK(ff_config_parse("hotkey_toggle = Q\nhotkey_quit = Ctrl+Nope\nbeta = 2\n", &c) == 1);
    CHECK(c.hotkey_toggle.vk == 'F' && c.hotkey_quit.vk == 0 && c.beta == 1);

    /* круг запись-чтение */
    CHECK(ff_config_format(&c, buf, sizeof buf) > 0);
    ff_config_defaults(&b);
    ff_config_parse(buf, &b);
    CHECK(b.beta == 1 && b.hotkey_toggle.vk == 'F' && b.hotkey_toggle.mods == (FF_MOD_CTRL | FF_MOD_ALT));
    CHECK(b.hotkey_quit.vk == 0);
}

static void test_long_lists(void)
{
    ff_config a;
    ff_config b;
    char buf[8192];
    size_t i;

    /* самый длинный список проходит запись и чтение целиком: строка ini длиннее старого предела в 320 байт */
    ff_config_defaults(&a);
    for (i = 0; i + 1 < sizeof a.pause_apps; i++) {
        a.pause_apps[i] = i % 9 == 8 ? ';' : 'a';
        a.ignore_apps[i] = i % 7 == 6 ? ';' : 'b';
    }
    CHECK(ff_config_format(&a, buf, sizeof buf) > 0);
    ff_config_defaults(&b);
    ff_config_parse(buf, &b);
    CHECK(strcmp(a.pause_apps, b.pause_apps) == 0 && strcmp(a.ignore_apps, b.ignore_apps) == 0);
}

static void test_values(void)
{
    ff_config c;
    ff_config_defaults(&c);
    CHECK(ff_config_parse("mon1_red_y = 0.25\nmon1_green_y=-0.5\nmon1_blue_x=1\nmon1_sharpen=0.3\nmon1_enabled=1\n"
                          "monitor=1\ncheck_updates=0\nauto_pause=2\n", &c) == 8);
    CHECK(c.view[1].shift[0][1] == 0.25f);
    CHECK(c.view[1].shift[1][1] == -0.5f);
    CHECK(c.view[1].shift[2][0] == 1.0f);
    CHECK(c.view[1].sharpen > 0.29f && c.view[1].sharpen < 0.31f);
    CHECK(c.view[1].enabled == 1 && c.view[0].shift[0][1] == 0.0f);
    CHECK(c.monitor == 1 && c.check_updates == 0 && c.auto_pause == 2);
}

static void test_clamp(void)
{
    ff_config c;
    ff_config_defaults(&c);
    CHECK(ff_config_parse("mon0_red_x=99\nmon0_green_x=-99\nmon0_sharpen=-5\nmonitor=-3\n", &c) == 4);
    CHECK(c.view[0].shift[0][0] == 2.0f && c.view[0].shift[1][0] == -2.0f);
    CHECK(c.view[0].sharpen == -1.0f && c.monitor == 0);
    /* минус у резкости разрешён, плюс зажимается единицей */
    CHECK(ff_config_parse("mon0_sharpen=-0.5\n", &c) == 1 && c.view[0].sharpen == -0.5f);
    CHECK(ff_config_parse("mon0_sharpen=7\n", &c) == 1 && c.view[0].sharpen == 1.0f);
    CHECK(ff_config_parse("auto_pause=3\n", &c) == 1 && c.auto_pause == 3);
    CHECK(ff_config_parse("auto_pause=9\n", &c) == 1 && c.auto_pause == 3);
    CHECK(ff_config_parse("auto_pause=-1\n", &c) == 1 && c.auto_pause == 0);
    CHECK(ff_config_parse("monitor=1000\n", &c) == 1 && c.monitor == FF_MAX_MONITORS - 1);
    /* номер монитора вне таблицы это мусор, соседние мониторы не страдают */
    CHECK(ff_config_parse("mon9_red_x=1\nmon8_enabled=1\n", &c) == 0);
    CHECK(c.view[FF_MAX_MONITORS - 1].shift[0][0] == 0.0f && c.view[FF_MAX_MONITORS - 1].enabled == 0);
}

static void test_game_keys(void)
{
    ff_config c;
    char text[FF_APPS_MAX + 200];

    ff_config_defaults(&c);
    /* облегчённого режима больше нет: game_action из старых файлов принимается и ни на что не влияет */
    CHECK(ff_config_parse("game_action=1\ngpu_priority=-3\npause_apps = a.exe;b.exe\n"
                          "ignore_apps=c.exe\n", &c) == 4);
    CHECK(c.gpu_priority == -3 && c.auto_pause == 2);
    CHECK(strcmp(c.pause_apps, "a.exe;b.exe") == 0 && strcmp(c.ignore_apps, "c.exe") == 0);
    /* зажим диапазонов */
    CHECK(ff_config_parse("language=2\ntheme=1\nstats=1\n", &c) == 3);
    CHECK(c.language == 2 && c.theme == 1 && c.stats == 1);
    CHECK(ff_config_parse("language=9\ntheme=-4\n", &c) == 2 && c.language == 2 && c.theme == 0);
    CHECK(ff_config_parse("game_action=9\ngpu_priority=-99\n", &c) == 2);
    CHECK(c.gpu_priority == -7);
    /* слишком длинный список отбрасывается целиком, а не режется, прежнее значение остаётся */
    strcpy(text, "pause_apps=");
    memset(text + 11, 'a', FF_APPS_MAX + 80);
    strcpy(text + 11 + FF_APPS_MAX + 80, "\n");
    CHECK(ff_config_parse(text, &c) == 0 && strcmp(c.pause_apps, "a.exe;b.exe") == 0);
}

static void test_legacy(void)
{
    ff_config c;

    /* файл прежнего вида: сдвиги без номера монитора, monitor ниже них; значения переезжают на этот монитор,
     * включаются там и больше нигде */
    ff_config_defaults(&c);
    CHECK(ff_config_parse("red_x = 0.5\ngreen_y = -0.25\nsharpen = 0.1\nmonitor = 2\ngame_action = 1\n", &c) == 5);
    CHECK(c.monitor == 2 && c.view[2].shift[0][0] == 0.5f && c.view[2].shift[1][1] == -0.25f);
    CHECK(c.view[2].sharpen > 0.09f && c.view[2].enabled == 1);
    CHECK(c.view[0].enabled == 0 && c.view[0].shift[0][0] == 0.0f && c.view[1].enabled == 0);

    /* без ключа monitor старые значения остаются на нулевом мониторе */
    ff_config_defaults(&c);
    CHECK(ff_config_parse("red_x = 1\n", &c) == 1);
    CHECK(c.view[0].shift[0][0] == 1.0f && c.view[0].enabled == 1);

    /* monitor выше сдвигов или ниже: результат один */
    ff_config_defaults(&c);
    CHECK(ff_config_parse("monitor = 1\nred_x = 1\n", &c) == 2);
    CHECK(c.view[1].shift[0][0] == 1.0f && c.view[1].enabled == 1 && c.view[0].shift[0][0] == 0.0f);

    /* новый вид не трогается переездом */
    ff_config_defaults(&c);
    CHECK(ff_config_parse("mon0_red_x = 1\nmon1_red_x = 2\nmonitor = 1\n", &c) == 3);
    CHECK(c.view[0].shift[0][0] == 1.0f && c.view[1].shift[0][0] == 2.0f);
}

static void test_views(void)
{
    ff_config c;
    ff_config b;
    char buf[8192];

    /* два монитора с разными настройками и разным состоянием фильтра переживают круг запись-чтение,
     * нетронутые мониторы в файл не попадают */
    ff_config_defaults(&c);
    c.view[0].shift[0][0] = 0.4f;
    c.view[0].enabled = 0;
    c.view[2].shift[2][1] = -1.25f;
    c.view[2].sharpen = 0.2f;
    c.view[2].enabled = 1;
    c.monitor = 2;
    CHECK(ff_config_format(&c, buf, sizeof buf) > 0);
    CHECK(strstr(buf, "mon2_red_x") && !strstr(buf, "mon1_") && !strstr(buf, "mon3_"));
    ff_config_defaults(&b);
    CHECK(ff_config_parse(buf, &b) > 0);
    CHECK(memcmp(c.view, b.view, sizeof c.view) == 0 && b.monitor == 2);

    CHECK(ff_config_view(&c, -5) == &c.view[0] && ff_config_view(&c, 99) == &c.view[FF_MAX_MONITORS - 1]);
    ff_view_reset(&c.view[2]);
    CHECK(c.view[2].shift[2][1] == 0.0f && c.view[2].sharpen == 0.0f && c.view[2].enabled == 1);
}

static void test_rotation_sets(void)
{
    ff_config c;
    ff_config b;
    char buf[8192];

    /* режим А: любой поворот правит основной вид; режим Б: набор своей ориентации */
    ff_config_defaults(&c);
    CHECK(ff_config_view_for(&c, 1, 2) == &c.view[1] && ff_config_view_for(&c, 1, 0) == &c.view[1]);
    c.rot_mode = 1;
    CHECK(ff_config_view_for(&c, 1, 0) == &c.view[1] && ff_config_view_for(&c, 1, 1) == &c.rview[1][0]);
    CHECK(ff_config_view_for(&c, 1, 3) == &c.rview[1][2] && ff_config_view_for(&c, 1, 4) == &c.view[1]);
    CHECK(ff_config_view_for_const(&c, 0, 2) == &c.rview[0][1]);

    /* круг запись-чтение: режим, наборы и их отсутствие у нетронутых ориентаций */
    c.rview[0][0].shift[1][0] = 0.75f;
    c.rview[0][0].sharpen = -0.2f;
    c.rview[3][2].shift[2][1] = -1.5f;
    CHECK(ff_config_format(&c, buf, sizeof buf) > 0);
    CHECK(strstr(buf, "rotation_mode = 1") && strstr(buf, "mon0_r1_green_x") && strstr(buf, "mon3_r3_blue_y"));
    CHECK(!strstr(buf, "mon0_r2_") && !strstr(buf, "mon1_r"));
    ff_config_defaults(&b);
    CHECK(ff_config_parse(buf, &b) > 0);
    CHECK(b.rot_mode == 1 && memcmp(c.rview, b.rview, sizeof c.rview) == 0);

    /* флаг включения у набора поворота не принимается, номер поворота вне 1..3 тоже; режим вне 0..1 зажимается */
    ff_config_defaults(&b);
    CHECK(ff_config_parse("mon0_r1_enabled=1\nmon0_r4_red_x=1\nmon0_r0_red_x=1\n", &b) == 0);
    CHECK(b.rview[0][0].shift[0][0] == 0.0f && b.view[0].shift[0][0] == 0.0f);
    CHECK(ff_config_parse("rotation_mode=2\n", &b) == 1 && b.rot_mode == 1);
}

static void test_junk(void)
{
    ff_config c;
    ff_config_defaults(&c);
    /* комментарии, секции, нечисловые и «хвостатые» значения, nan, неизвестные ключи */
    CHECK(ff_config_parse("; red_x=1\n# red_x=1\n[sect]\nnope\nred_x=abc\nred_x=1.5 junk\n"
                          "red_x=nan\nred_x=inf\nmon0_red_x=abc\nunknown=1\n=\n", &c) == 0);
    CHECK(c.view[0].shift[0][0] == 0.0f);
}

static void test_bom(void)
{
    ff_config c;
    ff_config_defaults(&c);
    /* метка UTF-8 перед первым ключом не должна его съедать */
    CHECK(ff_config_parse("\xEF\xBB\xBF" "mon0_red_x=1\nmon0_green_x=0.5\n", &c) == 2);
    CHECK(c.view[0].shift[0][0] == 1.0f && c.view[0].shift[1][0] == 0.5f);
    CHECK(ff_config_parse("\xEF\xBB", &c) == 0);
}

static void test_line_endings(void)
{
    ff_config c;
    ff_config_defaults(&c);
    /* CRLF и последняя строка без перевода строки */
    CHECK(ff_config_parse("mon0_red_x=1\r\nmon0_green_x=0.5", &c) == 2);
    CHECK(c.view[0].shift[0][0] == 1.0f && c.view[0].shift[1][0] == 0.5f);
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
    char buf[4096];
    char tiny[16];
    int n;

    ff_config_defaults(&a);
    a.view[0].shift[0][0] = 0.25f;
    a.view[0].shift[1][1] = -0.125f;
    a.view[0].shift[2][0] = -0.5f;
    a.view[0].sharpen = 0.5f;
    a.monitor = 2;
    a.check_updates = 0;
    a.auto_pause = 3;
    a.gpu_priority = -4;
    a.engine = 1;
    a.present_mode = 1;
    a.hud_corner = 3;
    a.language = 1;
    a.theme = 2;
    a.stats = 1;
    strcpy(a.pause_apps, "game.exe;x.exe");
    strcpy(a.ignore_apps, "y.exe");
    strcpy(a.reshade_dir, "D:\\Игры\\Elden Ring\\Game");

    n = ff_config_format(&a, buf, sizeof buf);
    CHECK(n > 0);
    ff_config_defaults(&b);
    CHECK(ff_config_parse(buf, &b) == 25);
    CHECK(memcmp(&a, &b, sizeof a) == 0);
    /* маленький буфер не должен молча обрезать файл */
    CHECK(ff_config_format(&a, tiny, sizeof tiny) == -1);
}

int main(void)
{
    test_defaults();
    test_values();
    test_long_lists();
    test_engine();
    test_hud_corner();
    test_hotkeys_and_beta();
    test_clamp();
    test_game_keys();
    test_legacy();
    test_views();
    test_rotation_sets();
    test_junk();
    test_bom();
    test_line_endings();
    test_long_line();
    test_roundtrip();
    TEST_DONE("config");
}
