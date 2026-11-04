#ifndef FRINGEFIX_UI_PAINT_H
#define FRINGEFIX_UI_PAINT_H

#include <windows.h>

/* шрифты и кисти окна настроек; вызывается при создании, смене dpi и смене темы */
void paint_init(int dpi);
void paint_free(void);

HFONT paint_font(void);
HFONT paint_bold(void);
HBRUSH paint_bg(void);
HBRUSH paint_field(void);

/* цвета для WM_CTLCOLOR*: поля и списки по цвету поля, остальное по фону; field_ctl принудительно поле */
LRESULT paint_ctlcolor(UINT msg, HDC dc, HWND ctl, HWND field_ctl);

/* рисует фон, карточки, заголовки, подписи строк и строку состояния */
void paint_window(HWND h, int dpi, const wchar_t *status);

#endif
