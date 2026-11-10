#ifndef FRINGEFIX_TRIAL_H
#define FRINGEFIX_TRIAL_H

/* пробное применение настроек вывода с откатом по таймеру, как подтверждение разрешения экрана в Windows:
 * новое значение действует, но в ini не пишется, пока его не подтвердили; нет ответа за FF_TRIAL_MS, значит
 * картинки не видно и всё возвращается. модуль без платформенного кода, время приходит параметром */

#define FF_TRIAL_MS 10000.0

/* то, что меняется пробой: движок, способ вывода кадра и приоритет GPU */
typedef struct {
    int engine;
    int present;
    int priority;
} ff_variant;

typedef struct {
    int active;
    double deadline_ms;
    /* значения до первой смены: к ним возвращает откат */
    ff_variant saved;
} ff_trial;

int ff_variant_equal(const ff_variant *a, const ff_variant *b);

/* начинает пробу; before это значения до смены. если проба уже идёт, прежними остаются значения до первой смены,
 * а срок отсчитывается заново от последней */
void ff_trial_begin(ff_trial *t, const ff_variant *before, double now_ms);

int ff_trial_active(const ff_trial *t);

/* сколько миллисекунд осталось; 0, если проба не идёт или срок вышел */
double ff_trial_left(const ff_trial *t, double now_ms);

/* 1, если срок вышел: проба закрыта, значения для отката лежат в *revert */
int ff_trial_expired(ff_trial *t, double now_ms, ff_variant *revert);

/* подтверждение: проба закрыта без отката */
void ff_trial_keep(ff_trial *t);

/* откат по кнопке, закрытию окна или сбою вывода: 1, если проба шла и значения для отката лежат в *revert */
int ff_trial_cancel(ff_trial *t, ff_variant *revert);

#endif
