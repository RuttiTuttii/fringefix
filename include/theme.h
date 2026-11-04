#ifndef FRINGEFIX_THEME_H
#define FRINGEFIX_THEME_H

#include <windows.h>

/* цвета окна настроек; светлая или тёмная палитра берётся из настроек Windows */
typedef struct {
    int dark;
    COLORREF bg;
    COLORREF card;
    COLORREF field;
    COLORREF fg;
    COLORREF muted;
    COLORREF border;
    COLORREF accent;
} ff_palette;

/* перечитывает тему приложений и акцентный цвет из системы */
void theme_refresh(void);

const ff_palette *theme_get(void);

/* тёмная рамка и заголовок окна */
void theme_apply_window(HWND h);

/* системная тема элемента: кнопки и флажки (0) или поля ввода и списки (1) */
void theme_apply_control(HWND c, int field);

#endif
