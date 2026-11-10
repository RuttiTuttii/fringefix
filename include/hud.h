#ifndef FRINGEFIX_HUD_H
#define FRINGEFIX_HUD_H

#include <windows.h>

#include "benchstat.h"

/* панель замеров поверх оверлея: счётчики кадров, время этапов, задержки и потребление.
 * это отдельное окно, а не пиксели оверлея: оно исключено из захвата (WDA_EXCLUDEFROMCAPTURE), поэтому фильтр его не
 * трогает и оно не рождает кадры-эхо; пропускает мышь и не берёт фокус. окно принадлежит потоку захвата, живёт
 * только пока работает оверлей и открыто окно диагностики, при паузе, выключении и ошибке его нет */

/* всё, кроме замеров, что панель пишет в шапке */
typedef struct {
    int engine_req;
    int engine_eff;
    int present_req;
    int present_eff;
    int prio_req;
    int prio_eff;
    /* секунд до отката пробной настройки, -1 если пробы нет */
    int trial_left;
    int force;
} ff_hud_info;

/* размер панели в пикселях при данном dpi */
void hud_size(int dpi, int *w, int *h);

/* рисует панель в контекст размером w на h. вынесено из окна, чтобы рисунок проверялся без экрана и оверлея */
void hud_paint(HDC dc, int w, int h, int dpi, const ff_sample *s, unsigned ns, const ff_power_sample *p,
               unsigned np, double now_ms, const ff_hud_info *info);

/* отпускает шрифты панели; вызывается при закрытии окна */
void hud_release_fonts(void);

/* создаёт окно у левого верхнего угла монитора или закрывает его; зовётся только из потока захвата, дёшево при
 * повторных вызовах. сбой создания молча оставляет панель выключенной и повторяется не чаще раза в несколько секунд */
void hud_sync(const RECT *monitor, int want);

/* возвращает панель выше окна оверлея: оверлей сам поднимается наверх раз в две секунды */
void hud_reassert(void);

#endif
