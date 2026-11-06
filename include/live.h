#ifndef FRINGEFIX_LIVE_H
#define FRINGEFIX_LIVE_H

#include <windows.h>

#include "config.h"

/* связь приложения и ReShade-аддона внутри одной сессии Windows. приложение выкладывает текущие сдвиги и резкость
 * в именованную память (то, что стоит на слайдерах сейчас, а не то, что записано в ini), аддон в процессе игры
 * читает их каждые полсекунды. обратно идёт только маркер: аддон создаёт именованный mutex с pid игры,
 * приложение по нему узнаёт, что в окне на переднем плане фильтр уже рисует аддон, и ставит оверлей на паузу */

#define FF_LIVE_MAGIC 0x31584646u
#define FF_LIVE_NAME L"Local\\fringefix_live_v1"
#define FF_ADDON_MARK_FMT L"Local\\fringefix_addon_%lu"

typedef struct {
    unsigned magic;
    /* счётчик для чтения без блокировки: нечётный, пока идёт запись */
    volatile LONG seq;
    /* процесс приложения: по нему аддон отличает живое приложение от устаревшей памяти */
    unsigned pid;
    float shift[6];
    float sharpen;
    LONG enabled;
} ff_live;

/* приложение: создаёт память и выкладывает значения; 0 или -1. повторный вызов безопасен */
int live_publish_open(void);
void live_publish(const ff_config *cfg, int enabled);
void live_publish_close(void);

/* аддон: читает значения; 1, если приложение запущено и значения получены, 0 иначе.
 * при 0 выходные поля не трогаются. вызывается из одного потока */
int live_read(float shift[6], float *sharpen, int *enabled);
void live_read_close(void);

/* аддон: ставит и снимает маркер своего процесса */
void live_addon_announce(void);
void live_addon_withdraw(void);

/* приложение: 1, если в процессе pid загружен аддон */
int live_addon_present(DWORD pid);

#endif
