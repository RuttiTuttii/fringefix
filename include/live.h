#ifndef FRINGEFIX_LIVE_H
#define FRINGEFIX_LIVE_H

#include <windows.h>

#include "config.h"

/* связь приложения и ReShade-аддона внутри одной сессии Windows. приложение выкладывает текущие сдвиги и резкость
 * в именованную память (то, что стоит на слайдерах сейчас, а не то, что записано в ini), аддон в процессе игры
 * читает их каждые полсекунды. обратно идёт только маркер: аддон создаёт именованный mutex с pid игры,
 * приложение по нему узнаёт, что в окне на переднем плане фильтр уже рисует аддон, и ставит оверлей на паузу */

#define FF_LIVE_MAGIC 0x32584646u
#define FF_LIVE_NAME L"Local\\fringefix_live_v2"
#define FF_ADDON_MARK_FMT L"Local\\fringefix_addon_%lu"
/* имя выхода как в DXGI и GetMonitorInfo: "\\.\\DISPLAY1"; по нему аддон находит монитор, на котором окно игры */
#define FF_LIVE_DEV_LEN 32

typedef struct {
    float shift[6];
    float sharpen;
    /* фильтр на этом мониторе включён в окне приложения */
    LONG enabled;
    wchar_t device[FF_LIVE_DEV_LEN];
} ff_live_view;

typedef struct {
    unsigned magic;
    /* счётчик для чтения без блокировки: нечётный, пока идёт запись */
    volatile LONG seq;
    /* процесс приложения: по нему аддон отличает живое приложение от устаревшей памяти */
    unsigned pid;
    /* общий выключатель приложения (хоткей, трей): фильтр работает, если включены он и монитор */
    LONG master;
    /* монитор, показанный в окне приложения: запасной вариант, когда аддон не нашёл монитор своего окна */
    LONG selected;
    LONG count;
    ff_live_view view[FF_MAX_MONITORS];
} ff_live;

/* приложение: создаёт память и выкладывает значения; 0 или -1. повторный вызов безопасен */
int live_publish_open(void);

/* devices это имена выходов по порядку номеров (n штук), nullptr значит неизвестны: аддон тогда берёт выбранный монитор */
void live_publish(const ff_config *cfg, int master, const wchar_t (*devices)[FF_LIVE_DEV_LEN], int n);
void live_publish_close(void);

/* аддон: читает снимок; 1, если приложение запущено и значения получены, 0 иначе.
 * при 0 снимок не трогается. вызывается из одного потока */
int live_read(ff_live *out);
void live_read_close(void);

/* вид монитора, для которого работает аддон: по имени выхода окна игры, иначе выбранный в приложении.
 * имя может быть пустым или nullptr; результат всегда указывает внутрь снимка */
const ff_live_view *live_pick_view(const ff_live *snap, const wchar_t *device);

/* аддон просит приложение включить или выключить общий выключатель (ползунок в окне аддона): отдельная память
 * с записью для всех процессов пользователя, потому что основная читается аддоном только на чтение.
 * live_cmd_send: аддон, 1 если команда ушла; live_cmd_poll: приложение, 1 если с прошлого опроса пришла новая
 * команда, её значение в *enabled */
#define FF_CMD_NAME L"Local\\fringefix_cmd_v2"
int live_cmd_send(int enabled);
int live_cmd_poll(int *enabled);

/* аддон: ставит и снимает маркер своего процесса */
void live_addon_announce(void);
void live_addon_withdraw(void);

/* приложение: 1, если в процессе pid загружен аддон */
int live_addon_present(DWORD pid);

#endif
