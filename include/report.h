#ifndef FRINGEFIX_REPORT_H
#define FRINGEFIX_REPORT_H

#include <stddef.h>

#include "config.h"
#include "stats.h"

#define FF_COMMENT_MAX 1000
#define FF_REPORT_MAX 16384

/* всё, что может попасть в отчёт; отправляется только то, что отмечено include_* */
typedef struct {
    char app_version[32];
    char language[4];
    unsigned windows_build;
    char gpu[64];
    unsigned width;
    unsigned height;
    int hdr;
    ff_config cfg;
    ff_stats stats;
    char comment[FF_COMMENT_MAX + 1];
    int include_system;
    int include_settings;
    int include_stats;
} ff_report;

/* пишет отчёт в JSON ровно в том виде, в каком он уйдёт по сети, и возвращает длину или -1, если буфер мал.
 * в отчёте нет имени пользователя, имени компьютера, путей, заголовков окон, адресов и постоянного
 * идентификатора; время тоже не пишется */
int ff_report_format(const ff_report *r, char *buf, size_t n);

#endif
