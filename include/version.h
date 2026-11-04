#ifndef FRINGEFIX_VERSION_H
#define FRINGEFIX_VERSION_H

/* оба значения приходят из Makefile: git describe и owner/name репозитория */
#ifndef FF_VERSION
#define FF_VERSION "dev"
#endif
#ifndef FF_REPO
#define FF_REPO ""
#endif
/* адрес приёма отчётов; пусто значит, что отправка из этой сборки отключена */
#ifndef FF_REPORT_URL
#define FF_REPORT_URL ""
#endif

#endif
