#ifndef FRINGEFIX_WINSTATE_H
#define FRINGEFIX_WINSTATE_H

#include <stddef.h>

/* окна интерфейса, которые возвращаются после перезапуска: настройки, программа, списки программ */
enum { FF_WIN_SETTINGS, FF_WIN_PROGRAM, FF_WIN_APPS, FF_WIN_COUNT };

/* какие окна были открыты и где стоял их левый верхний угол в координатах экрана */
typedef struct {
    int open[FF_WIN_COUNT];
    int x[FF_WIN_COUNT];
    int y[FF_WIN_COUNT];
} ff_winstate;

/* строка вида "s=10,20;p=-5,7;a=0,0" для аргумента командной строки; пустая, если открытых окон нет.
 * без пробелов, чтобы не разрывать аргумент. длина строки или -1, если буфер мал */
int ff_winstate_format(const ff_winstate *w, char *buf, size_t n);

/* обратное преобразование; неизвестные ключи и мусор пропускаются, w не должен содержать старого состояния.
 * возвращает число принятых окон */
int ff_winstate_parse(const char *s, ff_winstate *w);

#endif
