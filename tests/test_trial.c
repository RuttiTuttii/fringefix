#include <math.h>

#include "test.h"
#include "trial.h"

static void test_expire(void)
{
    ff_trial t = { 0 };
    ff_variant before = { 0, 0, 0 };
    ff_variant back = { 9, 9, 9 };

    /* без пробы ничего не истекает и откатывать нечего */
    CHECK(!ff_trial_active(&t) && ff_trial_left(&t, 5.0) == 0.0);
    CHECK(!ff_trial_expired(&t, 1e9, &back) && back.engine == 9);
    CHECK(!ff_trial_cancel(&t, &back) && back.engine == 9);

    ff_trial_begin(&t, &before, 1000.0);
    CHECK(ff_trial_active(&t) && fabs(ff_trial_left(&t, 1000.0) - FF_TRIAL_MS) < 1e-9);
    CHECK(fabs(ff_trial_left(&t, 4000.0) - 7000.0) < 1e-9);
    /* за миг до срока ещё держится, ровно в срок откат */
    CHECK(!ff_trial_expired(&t, 1000.0 + FF_TRIAL_MS - 1.0, &back));
    CHECK(ff_trial_expired(&t, 1000.0 + FF_TRIAL_MS, &back));
    CHECK(back.engine == 0 && back.present == 0 && back.priority == 0);
    /* после отката проба закрыта и второй раз не срабатывает */
    CHECK(!ff_trial_active(&t) && !ff_trial_expired(&t, 1e9, &back) && ff_trial_left(&t, 0.0) == 0.0);
}

static void test_repeat_and_keep(void)
{
    ff_trial t = { 0 };
    ff_variant first = { 0, 0, 0 };
    ff_variant second = { 2, 1, 0 };
    ff_variant back;

    /* вторая смена внутри пробы не затирает исходные значения, а срок идёт заново */
    ff_trial_begin(&t, &first, 0.0);
    ff_trial_begin(&t, &second, 6000.0);
    CHECK(fabs(ff_trial_left(&t, 6000.0) - FF_TRIAL_MS) < 1e-9);
    CHECK(!ff_trial_expired(&t, 12000.0, &back));
    CHECK(ff_trial_expired(&t, 16000.0, &back) && ff_variant_equal(&back, &first));

    /* подтверждение закрывает пробу без отката */
    ff_trial_begin(&t, &first, 0.0);
    ff_trial_keep(&t);
    CHECK(!ff_trial_active(&t) && !ff_trial_expired(&t, 1e9, &back));

    /* откат по кнопке или по сбою отдаёт исходные значения один раз */
    ff_trial_begin(&t, &second, 0.0);
    CHECK(ff_trial_cancel(&t, &back) && ff_variant_equal(&back, &second));
    CHECK(!ff_trial_cancel(&t, &back));
    CHECK(!ff_variant_equal(&first, &second));
}

int main(void)
{
    test_expire();
    test_repeat_and_keep();
    TEST_DONE("trial");
}
