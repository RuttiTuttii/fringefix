#include "trial.h"

#include <string.h>

int ff_variant_equal(const ff_variant *a, const ff_variant *b)
{
    return a->engine == b->engine && a->present == b->present && a->priority == b->priority;
}

void ff_trial_begin(ff_trial *t, const ff_variant *before, double now_ms)
{
    if (!t->active) {
        t->saved = *before;
        t->active = 1;
    }
    t->deadline_ms = now_ms + FF_TRIAL_MS;
}

int ff_trial_active(const ff_trial *t)
{
    return t->active;
}

double ff_trial_left(const ff_trial *t, double now_ms)
{
    double left = t->deadline_ms - now_ms;

    return t->active && left > 0.0 ? left : 0.0;
}

int ff_trial_expired(ff_trial *t, double now_ms, ff_variant *revert)
{
    if (!t->active || now_ms < t->deadline_ms) {
        return 0;
    }
    *revert = t->saved;
    memset(t, 0, sizeof *t);
    return 1;
}

void ff_trial_keep(ff_trial *t)
{
    memset(t, 0, sizeof *t);
}

int ff_trial_cancel(ff_trial *t, ff_variant *revert)
{
    if (!t->active) {
        return 0;
    }
    *revert = t->saved;
    memset(t, 0, sizeof *t);
    return 1;
}
