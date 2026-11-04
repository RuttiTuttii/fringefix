#ifndef FRINGEFIX_UI_STATE_H
#define FRINGEFIX_UI_STATE_H

#include <windows.h>

#include "app.h"
#include "ui_layout.h"

/* общее состояние окна настроек для ui_settings.c (окно, команды) и ui_controls.c (элементы) */
typedef struct {
    ff_app *app;
    HWND win;
    HWND slider[IDX_SHARPEN + 1];
    HWND edit[IDX_SHARPEN + 1];
    HWND monitor;
    int dpi;
    /* больше нуля, пока поля правятся из кода: EN_CHANGE тогда не считается вводом пользователя */
    int sync;
} ui_state;

extern ui_state ui;

#define S(v) MulDiv((v), ui.dpi, 96)

enum {
    ID_SLIDER = 100,
    ID_EDIT = 120,
    ID_MONITOR = 140,
    ID_WIZARD,
    ID_PROGRAM,
    ID_PROF_LOAD,
    ID_PROF_SAVE,
    ID_PATTERN,
    ID_RESET,
    ID_SAVE
};

/* создаёт все элементы по таблицам ui_layout.h */
void ctl_build(void);

/* пересоздаёт элементы после смены dpi */
void ctl_rebuild(void);

/* значения конфига в ползунки, поля, список мониторов и флажок */
void ctl_sync(void);

/* шрифт и системная тема элементам, перерисовка окна */
void ctl_restyle(void);

/* то же для любого окна приложения с элементами, например для мастера */
void ctl_restyle_window(HWND w);

/* пользователь двинул ползунок или правит поле idx */
void ctl_slider_changed(int idx);
void ctl_edit_changed(int idx);

/* привести текст поля к виду значения */
void ctl_edit_show(int idx);

/* все сдвиги и резкость в ноль */
void ctl_reset(void);

#endif
