#ifndef FRINGEFIX_UI_PAINT_H
#define FRINGEFIX_UI_PAINT_H

#include <windows.h>

/* шрифты и кисти окна настроек; вызывается при создании, смене dpi и смене темы */
void paint_init(int dpi);
void paint_free(void);

HFONT paint_font(void);
HBRUSH paint_bg(void);
HBRUSH paint_field(void);

/* рисует фон, карточки, заголовки, подписи строк и строку состояния */
void paint_window(HWND h, int dpi, const wchar_t *status);

#endif
