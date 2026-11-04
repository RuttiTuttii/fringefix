#ifndef FRINGEFIX_SLIDER_H
#define FRINGEFIX_SLIDER_H

#include <windows.h>

/* собственный ползунок: рисуется по палитре темы, заливка идёт от нуля, отметка нуля видна.
 * значения целые, окно настроек хранит сотые доли */
#define FF_SLIDER_CLASS L"fringefix_slider"

enum {
    FFS_SETRANGE = WM_USER + 1, /* wParam минимум, lParam максимум */
    FFS_SETPOS,                 /* wParam позиция, без уведомления */
    FFS_GETPOS
};

/* родитель получает WM_COMMAND с этим кодом, когда позицию изменил пользователь */
#define FFN_CHANGED 1

int slider_register(HINSTANCE inst);

#endif
