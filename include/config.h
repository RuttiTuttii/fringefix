#ifndef FRINGEFIX_CONFIG_H
#define FRINGEFIX_CONFIG_H

#include <stddef.h>

/* настройки сдвига каналов; модуль не зависит от платформы, чтобы тестироваться без Windows */
typedef struct {
    /* на сколько пикселей экрана уезжает содержимое канала: [r, g, b][x, y]; плюс означает вправо и вниз */
    float shift[3][2];
    /* сила резкости от 0 до 1 */
    float sharpen;
    /* номер монитора, считая выходы всех видеокарт подряд */
    int monitor;
    /* проверять обновления при запуске, 0 или 1 */
    int check_updates;
} ff_config;

/* все сдвиги нулевые: оверлей ничего не меняет */
void ff_config_defaults(ff_config *cfg);

/* разбирает ini из строки, возвращает число принятых ключей; мусор и неизвестные ключи пропускает */
int ff_config_parse(const char *text, ff_config *cfg);

/* пишет ini в буфер вместе с комментариями; возвращает длину или -1, если буфер мал */
int ff_config_format(const ff_config *cfg, char *buf, size_t n);

#endif
