#include <string.h>

#include "hotkey.h"
#include "test.h"

static void test_parse(void)
{
    ff_hotkey h = { 99, 99 };
    char buf[32];

    /* F-клавиши принимаются без модификатора */
    CHECK(ff_hotkey_parse("F10", &h) == 0 && h.mods == 0 && h.vk == 0x79);
    CHECK(ff_hotkey_parse("f24", &h) == 0 && h.vk == 0x87);
    /* регистр, пробелы и порядок модификаторов не важны */
    CHECK(ff_hotkey_parse(" win + ALT + q ", &h) == 0 && h.mods == (FF_MOD_WIN | FF_MOD_ALT) && h.vk == 'Q');
    CHECK(ff_hotkey_parse("Control+Shift+7", &h) == 0 && h.mods == (FF_MOD_CTRL | FF_MOD_SHIFT) && h.vk == '7');
    /* выключенное сочетание */
    CHECK(ff_hotkey_parse("", &h) == 0 && h.vk == 0 && h.mods == 0);
    CHECK(ff_hotkey_parse("None", &h) == 0 && h.vk == 0);

    /* при ошибке результат не трогается */
    h.mods = 1;
    h.vk = 'Z';
    CHECK(ff_hotkey_parse("Q", &h) == -1);
    CHECK(ff_hotkey_parse("Shift+", &h) == -1);
    CHECK(ff_hotkey_parse("Ctrl+Alt", &h) == -1);
    CHECK(ff_hotkey_parse("F25", &h) == -1);
    CHECK(ff_hotkey_parse("F0", &h) == -1);
    CHECK(ff_hotkey_parse("Ctrl+F+G", &h) == -1);
    CHECK(ff_hotkey_parse("F10+Ctrl", &h) == -1);
    CHECK(ff_hotkey_parse("Hyper+F", &h) == -1);
    CHECK(ff_hotkey_parse("+F10", &h) == -1);
    CHECK(ff_hotkey_parse("Ctrl+Space", &h) == -1);
    CHECK(h.mods == 1 && h.vk == 'Z');

    /* круг запись-чтение */
    CHECK(ff_hotkey_parse("Win+Alt+Q", &h) == 0 && ff_hotkey_format(&h, buf, sizeof buf) > 0);
    CHECK(strcmp(buf, "Alt+Win+Q") == 0);
    CHECK(ff_hotkey_parse(buf, &h) == 0 && h.mods == (FF_MOD_WIN | FF_MOD_ALT) && h.vk == 'Q');
    CHECK(ff_hotkey_parse("F11", &h) == 0 && ff_hotkey_format(&h, buf, sizeof buf) == 3 && strcmp(buf, "F11") == 0);
    h.vk = 0;
    CHECK(ff_hotkey_format(&h, buf, sizeof buf) == 4 && strcmp(buf, "none") == 0);
}

static void test_format_small_buffer(void)
{
    ff_hotkey h = { FF_MOD_CTRL | FF_MOD_ALT, 0x79 };
    char buf[32];
    char tiny[6];

    CHECK(ff_hotkey_format(&h, buf, sizeof buf) > 0 && strcmp(buf, "Ctrl+Alt+F10") == 0);
    CHECK(ff_hotkey_format(&h, tiny, sizeof tiny) == -1);
    CHECK(ff_hotkey_format(&h, buf, 0) == -1);
}

int main(void)
{
    test_parse();
    test_format_small_buffer();
    TEST_DONE("hotkey");
}
