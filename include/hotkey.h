#ifndef FRINGEFIX_HOTKEY_H
#define FRINGEFIX_HOTKEY_H

#include <stddef.h>

/* биты модификаторов совпадают с MOD_ALT, MOD_CONTROL, MOD_SHIFT и MOD_WIN из windows.h,
 * поэтому в RegisterHotKey они уходят как есть, а модуль остаётся без платформенных заголовков */
#define FF_MOD_ALT 1u
#define FF_MOD_CTRL 2u
#define FF_MOD_SHIFT 4u
#define FF_MOD_WIN 8u

/* виртуальные коды клавиш по умолчанию: VK_F10 и VK_F11 */
#define FF_VK_F10 0x79u
#define FF_VK_F11 0x7Au

/* сочетание клавиш: vk это виртуальный код Windows, ноль значит, что сочетание выключено */
typedef struct {
    unsigned mods;
    unsigned vk;
} ff_hotkey;

/* "F10", "Ctrl+Alt+Q", "win + alt + f"; регистр и пробелы вокруг плюса не важны, "none" и пустая строка
 * выключают сочетание. буква или цифра без модификатора отвергается: она съедала бы ввод во всех программах.
 * 0 при успехе, -1 при неизвестной клавише или модификаторе; при ошибке out не меняется */
int ff_hotkey_parse(const char *s, ff_hotkey *out);

/* обратное преобразование в виде "Ctrl+Alt+F10" или "none"; длина строки или -1, если буфер мал */
int ff_hotkey_format(const ff_hotkey *h, char *buf, size_t n);

#endif
