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

/* WM_CTLCOLORSTATIC и WM_CTLCOLORBTN для подписей и флажков, которые лежат на карточке: фон цвета карточки */
LRESULT paint_ctlcolor_card(HDC dc);

/* цвета для WM_CTLCOLOR*: поля и списки по цвету поля, остальное по фону; field_ctl принудительно поле */
LRESULT paint_ctlcolor(UINT msg, HDC dc, HWND ctl, HWND field_ctl);

/* карточка с заголовком; координаты и размеры при 96 dpi, масштаб применяется внутри */
void paint_card(HDC dc, int dpi, int x, int y, int w, int h, const wchar_t *title);

/* карточка отдельной возможности: фон с оттенком акцента, рамка и полоса слева цветом акцента, на заголовке плашка badge */
void paint_card_accent(HDC dc, int dpi, int x, int y, int w, int h, const wchar_t *title, const wchar_t *badge);

/* строка выпадающего списка с владельцем: значок слева, текст справа, выбранная строка цветом акцента */
void paint_draw_item(const DRAWITEMSTRUCT *d, HICON icon, const wchar_t *text, int dpi);

/* круглая кнопка «?» из WM_DRAWITEM: кружок цвета акцента, нажатая темнее */
void paint_about_button(const DRAWITEMSTRUCT *d);

/* рисует фон, карточки, заголовки, подписи строк и строку состояния */
void paint_window(HWND h, int dpi, const wchar_t *status);

#endif
