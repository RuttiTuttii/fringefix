#include <math.h>
#include <string.h>

#include "benchstat.h"
#include "test.h"

static ff_ring ring;
static ff_pring pring;

static ff_sample make(double t, float copy, float diff, float draw, int engine, int drew)
{
    ff_sample s;

    memset(&s, 0, sizeof s);
    s.t_ms = t;
    s.copy_ms = copy;
    s.diff_ms = diff;
    s.draw_ms = draw;
    s.cpu_ms = 1.0f;
    s.present_ms = drew ? 0.5f : -1.0f;
    s.age_ms = -1.0f;
    s.engine = (unsigned char)engine;
    s.drew = (unsigned char)drew;
    return s;
}

static void test_stat(void)
{
    float v[100] = { 0 };
    ff_stat st;
    int i;

    ff_stat_compute(v, 0, &st);
    CHECK(st.n == 0 && st.mean == 0.0f && st.max == 0.0f);

    for (i = 0; i < 100; i++) {
        v[i] = (float)(100 - i);
    }
    ff_stat_compute(v, 100, &st);
    /* ближайший ранг: p95 это 95-й по возрастанию, p99 99-й, v не испорчен сортировкой */
    CHECK(st.n == 100 && fabsf(st.mean - 50.5f) < 1e-4f);
    CHECK(st.p50 == 50.0f && st.p95 == 95.0f && st.p99 == 99.0f && st.max == 100.0f);
    CHECK(v[0] == 100.0f && v[99] == 1.0f);

    ff_stat_compute(v, 1, &st);
    CHECK(st.p50 == 100.0f && st.p99 == 100.0f && st.max == 100.0f);
}

static void test_ring(void)
{
    ff_sample out[8];
    unsigned i;
    unsigned n;

    for (i = 0; i < FF_RING_CAP + 10; i++) {
        ff_sample s = make((double)i, 0.1f, 0.1f, 0.1f, 0, 1);

        ff_ring_push(&ring, &s);
    }
    CHECK(ring.count == FF_RING_CAP);
    n = ff_ring_copy(&ring, out, 8);
    /* последние восемь, по порядку, старые затёрты */
    CHECK(n == 8 && out[0].t_ms == (double)(FF_RING_CAP + 2) && out[7].t_ms == (double)(FF_RING_CAP + 9));
    n = ff_ring_copy(&ring, out, 3);
    CHECK(n == 3 && out[2].t_ms == (double)(FF_RING_CAP + 9));
}

static void test_summarize(void)
{
    ff_sample s[6];
    ff_power_sample p[3];
    ff_run_row row;
    char buf[2048];
    int len;

    s[0] = make(100.0, 0.2f, 0.3f, 0.5f, 1, 1);
    s[1] = make(110.0, 0.2f, 0.3f, -1.0f, 1, 0);
    s[2] = make(116.0, 0.4f, -1.0f, 0.7f, 1, 1);
    /* другой движок и кадр вне окна не считаются */
    s[3] = make(120.0, 9.0f, 9.0f, 9.0f, 2, 1);
    s[4] = make(5000.0, 9.0f, 9.0f, 9.0f, 1, 1);
    s[5] = make(130.0, 9.0f, 9.0f, 9.0f, 1, 1);
    s[5].light = 1;
    memset(p, 0, sizeof p);
    p[0].t_ms = 100.0; p[0].watts = 100.0f; p[0].util_gpu = 10.0f; p[0].clock_mhz = 2000.0f; p[0].temp_c = -1.0f; p[0].cpu_pct = 1.0f;
    p[1].t_ms = 120.0; p[1].watts = 120.0f; p[1].util_gpu = 30.0f; p[1].clock_mhz = 2200.0f; p[1].temp_c = -1.0f; p[1].cpu_pct = 3.0f;
    p[2].t_ms = 9000.0; p[2].watts = 999.0f; p[2].util_gpu = 99.0f; p[2].clock_mhz = 1.0f; p[2].temp_c = -1.0f; p[2].cpu_pct = 9.0f;

    ff_run_summarize(s, 6, p, 3, 0.0, 1000.0, 1, &row);
    CHECK(row.cycles == 3 && row.draws == 2);
    /* этапы считаются только там, где они были: сравнение в двух циклах, отрисовка в двух */
    CHECK(row.copy.n == 3 && row.diff.n == 2 && row.draw.n == 2);
    CHECK(fabsf(row.diff.mean - 0.3f) < 1e-5f && fabsf(row.draw.mean - 0.6f) < 1e-5f);
    /* сумма этапов по циклам: 1.0, 0.5, 1.1 */
    CHECK(row.gpu.n == 3 && fabsf(row.gpu.mean - (2.6f / 3.0f)) < 1e-5f && fabsf(row.gpu.max - 1.1f) < 1e-5f);
    /* промежуток между двумя отрисовками: 116 - 100 */
    CHECK(row.interval.n == 1 && fabsf(row.interval.mean - 16.0f) < 1e-4f);
    CHECK(row.present.n == 2 && row.age.n == 0);
    CHECK(fabsf(row.watts - 110.0f) < 1e-4f && fabsf(row.util_gpu - 20.0f) < 1e-4f);
    CHECK(row.temp_c < 0.0f && fabsf(row.cpu_pct - 2.0f) < 1e-4f);

    /* фильтр выключен: кадров нет, потребление есть */
    ff_run_summarize(s, 6, p, 3, 0.0, 1000.0, FF_ENGINE_OFF, &row);
    CHECK(row.cycles == 0 && row.copy.n == 0 && fabsf(row.watts - 110.0f) < 1e-4f);

    ff_run_summarize(s, 6, p, 3, 0.0, 1000.0, 1, &row);
    len = ff_run_csv(&row, 1, buf, sizeof buf);
    CHECK(len > 0 && strncmp(buf, "engine,seconds,", 15) == 0 && strstr(buf, "\ncompute,1.00,3.00,2.00,") != NULL);
    /* буфер мал: -1, а не переполнение */
    CHECK(ff_run_csv(&row, 1, buf, 40) == -1);
    CHECK(ff_run_csv(&row, 1, buf, 0) == -1);

    len = ff_samples_csv(s, 2, buf, sizeof buf);
    CHECK(len > 0 && strstr(buf, "\n100.000,compute,0,1,0,0.2000,0.3000,0.5000,") != NULL);
    CHECK(ff_samples_csv(s, 6, buf, 80) == -1);
}

static void test_win_stats(void)
{
    ff_sample s[5];
    ff_win_stats w;
    ff_run_row row;
    char buf[2048];

    s[0] = make(100.0, 0.2f, 0.3f, 0.5f, 1, 1);
    s[0].accum = 1;
    s[1] = make(200.0, 0.2f, 0.3f, 0.5f, 1, 1);
    s[1].accum = 4;
    s[2] = make(300.0, 0.2f, 0.3f, -1.0f, 1, 0);
    s[2].accum = 2;
    s[2].echo = 1;
    /* неизвестное число (0) считается как один кадр; облегчённый режим и чужой движок тоже входят */
    s[3] = make(400.0, 0.2f, 0.3f, 0.5f, 0, 1);
    s[3].light = 1;
    /* вне окна */
    s[4] = make(5000.0, 0.2f, 0.3f, 0.5f, 1, 1);
    s[4].accum = 9;

    ff_win_stats_compute(s, 5, 0.0, 1000.0, &w);
    CHECK(w.acquired == 4 && w.source == 8 && w.merged == 4);
    CHECK(w.draws == 3 && w.echo == 1);
    CHECK(fabsf(w.merged_pct - 50.0f) < 1e-4f && fabsf(w.source_per_s - 8.0f) < 1e-4f);
    CHECK(fabsf(w.acquired_per_s - 4.0f) < 1e-4f && fabsf(w.draws_per_s - 3.0f) < 1e-4f);

    /* пустое окно и нулевая длина без деления на ноль */
    ff_win_stats_compute(s, 0, 0.0, 1000.0, &w);
    CHECK(w.acquired == 0 && w.source == 0 && w.merged_pct == 0.0f && w.source_per_s == 0.0f);
    ff_win_stats_compute(s, 5, 10.0, 10.0, &w);
    CHECK(w.acquired == 0 && w.acquired_per_s == 0.0f);

    /* в итоге прогона источник считается только по кадрам нужного движка */
    ff_run_summarize(s, 5, NULL, 0, 0.0, 1000.0, 1, &row);
    CHECK(row.cycles == 3 && row.source == 7);
    CHECK(ff_run_csv(&row, 1, buf, sizeof buf) > 0 && strstr(buf, ",source_per_s,merged_pct\n") != NULL);
    /* 7 источников на 3 цикла за секунду: 7.00 и 57.14 процента склеено */
    CHECK(strstr(buf, ",7.00,57.14\n") != NULL);
    CHECK(ff_samples_csv(s, 2, buf, sizeof buf) > 0 && strstr(buf, ",age_ms,accum\n") != NULL
          && strstr(buf, "\n200.000,compute,0,1,0,0.2000,0.3000,0.5000,1.0000,0.5000,-1.0000,4\n") != NULL);
}

int main(void)
{
    test_stat();
    test_ring();
    test_summarize();
    test_win_stats();
    (void)pring;
    TEST_DONE("benchstat");
}
