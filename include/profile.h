#ifndef FRINGEFIX_PROFILE_H
#define FRINGEFIX_PROFILE_H

#include <stddef.h>

#include "config.h"

#define FF_PROFILE_FORMAT "fringefix-profile"

/* профиль это только вид картинки: сдвиги трёх каналов и резкость, без монитора и без обновлений.
 * пишет JSON в буфер; возвращает длину или -1, если буфер мал */
int ff_profile_format(const ff_config *cfg, char *buf, size_t n);

/* читает JSON в cfg, остальные поля cfg не трогает; значения вне диапазона зажимаются, битые пропускаются.
 * возвращает число принятых значений от 0 до 7 или -1, если это не профиль fringefix */
int ff_profile_parse(const char *json, ff_config *cfg);

#endif
