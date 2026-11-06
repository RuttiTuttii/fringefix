#include "benchstat.h"

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ff_ring_push(ff_ring *r, const ff_sample *s)
{
    r->s[r->head] = *s;
    r->head = (r->head + 1) % FF_RING_CAP;
    if (r->count < FF_RING_CAP) {
        r->count++;
    }
}

void ff_pring_push(ff_pring *r, const ff_power_sample *s)
{
    r->s[r->head] = *s;
    r->head = (r->head + 1) % FF_PRING_CAP;
    if (r->count < FF_PRING_CAP) {
        r->count++;
    }
}

unsigned ff_ring_copy(const ff_ring *r, ff_sample *out, unsigned max)
{
    unsigned n = r->count < max ? r->count : max;
    unsigned start = (r->head + FF_RING_CAP - n) % FF_RING_CAP;
    unsigned i;

    for (i = 0; i < n; i++) {
        out[i] = r->s[(start + i) % FF_RING_CAP];
    }
    return n;
}

unsigned ff_pring_copy(const ff_pring *r, ff_power_sample *out, unsigned max)
{
    unsigned n = r->count < max ? r->count : max;
    unsigned start = (r->head + FF_PRING_CAP - n) % FF_PRING_CAP;
    unsigned i;

    for (i = 0; i < n; i++) {
        out[i] = r->s[(start + i) % FF_PRING_CAP];
    }
    return n;
}

static int cmp_float(const void *a, const void *b)
{
    float x = *(const float *)a;
    float y = *(const float *)b;

    return (x > y) - (x < y);
}

/* значение по ближайшему рангу: q = 0.95 берёт элемент с номером ceil(0.95 * n) */
static float rank_pick(const float *sorted, unsigned n, double q)
{
    unsigned k = (unsigned)ceil(q * (double)n);

    return sorted[k == 0 ? 0 : (k > n ? n - 1 : k - 1)];
}

void ff_stat_compute(const float *v, unsigned n, ff_stat *out)
{
    float *tmp;
    double sum = 0.0;
    unsigned i;

    memset(out, 0, sizeof *out);
    if (n == 0) {
        return;
    }
    tmp = malloc(n * sizeof *tmp);
    if (!tmp) {
        return;
    }
    memcpy(tmp, v, n * sizeof *tmp);
    qsort(tmp, n, sizeof *tmp, cmp_float);
    for (i = 0; i < n; i++) {
        sum += (double)tmp[i];
    }
    out->n = n;
    out->mean = (float)(sum / (double)n);
    out->p50 = rank_pick(tmp, n, 0.50);
    out->p95 = rank_pick(tmp, n, 0.95);
    out->p99 = rank_pick(tmp, n, 0.99);
    out->max = tmp[n - 1];
    free(tmp);
}

/* среднее по положительным и нулевым значениям поля; -1, если таких нет */
static float mean_valid(const ff_power_sample *p, unsigned np, double t0, double t1, size_t off)
{
    double sum = 0.0;
    unsigned used = 0;
    unsigned i;

    for (i = 0; i < np; i++) {
        float v;

        if (p[i].t_ms < t0 || p[i].t_ms >= t1) {
            continue;
        }
        memcpy(&v, (const unsigned char *)&p[i] + off, sizeof v);
        if (v >= 0.0f) {
            sum += (double)v;
            used++;
        }
    }
    return used ? (float)(sum / (double)used) : -1.0f;
}

void ff_run_summarize(const ff_sample *s, unsigned ns, const ff_power_sample *p, unsigned np,
                      double t0, double t1, int engine, ff_run_row *row)
{
    float *copy = malloc((ns + 1) * sizeof *copy);
    float *diff = malloc((ns + 1) * sizeof *diff);
    float *draw = malloc((ns + 1) * sizeof *draw);
    float *gpu = malloc((ns + 1) * sizeof *gpu);
    float *cpu = malloc((ns + 1) * sizeof *cpu);
    float *pres = malloc((ns + 1) * sizeof *pres);
    float *age = malloc((ns + 1) * sizeof *age);
    float *gap = malloc((ns + 1) * sizeof *gap);
    unsigned ncopy = 0, ndiff = 0, ndraw = 0, ngpu = 0, ncpu = 0, npres = 0, nage = 0, ngap = 0;
    double last_draw = -1.0;
    unsigned i;

    memset(row, 0, sizeof *row);
    row->engine = engine;
    row->seconds = (t1 - t0) / 1000.0;
    if (copy && diff && draw && gpu && cpu && pres && age && gap && engine != FF_ENGINE_OFF) {
        for (i = 0; i < ns; i++) {
            const ff_sample *c = &s[i];
            float total = 0.0f;
            int any = 0;

            if (c->t_ms < t0 || c->t_ms >= t1 || c->engine != engine || c->light) {
                continue;
            }
            row->cycles++;
            if (c->copy_ms >= 0.0f) {
                copy[ncopy++] = c->copy_ms;
                total += c->copy_ms;
                any = 1;
            }
            if (c->diff_ms >= 0.0f) {
                diff[ndiff++] = c->diff_ms;
                total += c->diff_ms;
                any = 1;
            }
            if (c->draw_ms >= 0.0f) {
                draw[ndraw++] = c->draw_ms;
                total += c->draw_ms;
                any = 1;
            }
            if (any) {
                gpu[ngpu++] = total;
            }
            if (c->cpu_ms >= 0.0f) {
                cpu[ncpu++] = c->cpu_ms;
            }
            if (c->present_ms >= 0.0f) {
                pres[npres++] = c->present_ms;
            }
            if (c->age_ms >= 0.0f) {
                age[nage++] = c->age_ms;
            }
            if (c->drew) {
                row->draws++;
                if (last_draw >= 0.0) {
                    gap[ngap++] = (float)(c->t_ms - last_draw);
                }
                last_draw = c->t_ms;
            }
        }
        ff_stat_compute(copy, ncopy, &row->copy);
        ff_stat_compute(diff, ndiff, &row->diff);
        ff_stat_compute(draw, ndraw, &row->draw);
        ff_stat_compute(gpu, ngpu, &row->gpu);
        ff_stat_compute(cpu, ncpu, &row->cpu);
        ff_stat_compute(pres, npres, &row->present);
        ff_stat_compute(age, nage, &row->age);
        ff_stat_compute(gap, ngap, &row->interval);
    }
    free(copy);
    free(diff);
    free(draw);
    free(gpu);
    free(cpu);
    free(pres);
    free(age);
    free(gap);
    row->watts = mean_valid(p, np, t0, t1, offsetof(ff_power_sample, watts));
    row->util_gpu = mean_valid(p, np, t0, t1, offsetof(ff_power_sample, util_gpu));
    row->clock_mhz = mean_valid(p, np, t0, t1, offsetof(ff_power_sample, clock_mhz));
    row->temp_c = mean_valid(p, np, t0, t1, offsetof(ff_power_sample, temp_c));
    row->cpu_pct = mean_valid(p, np, t0, t1, offsetof(ff_power_sample, cpu_pct));
}

const char *ff_engine_name(int engine)
{
    static const char *const NAMES[] = { "classic", "compute", "kernel", "off" };

    return engine >= 0 && engine <= FF_ENGINE_OFF ? NAMES[engine] : "unknown";
}

/* дописывает в буфер с курсором; ненулевой ответ значит, что не поместилось */
static int put(char *buf, size_t cap, size_t *pos, const char *fmt, ...)
{
    va_list ap;
    int w;

    va_start(ap, fmt);
    w = vsnprintf(buf + *pos, cap - *pos, fmt, ap);
    va_end(ap);
    if (w < 0 || (size_t)w >= cap - *pos) {
        return -1;
    }
    *pos += (size_t)w;
    return 0;
}

int ff_run_csv(const ff_run_row *rows, int n, char *buf, size_t cap)
{
    size_t pos = 0;
    int i;

    if (cap == 0) {
        return -1;
    }
    buf[0] = '\0';
    if (put(buf, cap, &pos, "engine,seconds,cycles_per_s,draws_per_s,copy_avg,copy_p95,diff_avg,diff_p95,"
                            "draw_avg,draw_p95,gpu_avg,gpu_p95,gpu_p99,gpu_max,cpu_avg,cpu_p95,"
                            "present_avg,present_p95,age_avg,age_p95,interval_avg,interval_p95,interval_p99,"
                            "watts,util_gpu,clock_mhz,temp_c,cpu_pct\n")) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        const ff_run_row *r = &rows[i];
        double sec = r->seconds > 0.0 ? r->seconds : 1.0;

        if (put(buf, cap, &pos,
                "%s,%.2f,%.2f,%.2f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,"
                "%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.2f,%.1f,%.0f,%.1f,%.1f\n",
                ff_engine_name(r->engine), r->seconds, (double)r->cycles / sec, (double)r->draws / sec,
                (double)r->copy.mean, (double)r->copy.p95, (double)r->diff.mean, (double)r->diff.p95,
                (double)r->draw.mean, (double)r->draw.p95, (double)r->gpu.mean, (double)r->gpu.p95,
                (double)r->gpu.p99, (double)r->gpu.max, (double)r->cpu.mean, (double)r->cpu.p95,
                (double)r->present.mean, (double)r->present.p95, (double)r->age.mean, (double)r->age.p95,
                (double)r->interval.mean, (double)r->interval.p95, (double)r->interval.p99,
                (double)r->watts, (double)r->util_gpu, (double)r->clock_mhz, (double)r->temp_c,
                (double)r->cpu_pct)) {
            return -1;
        }
    }
    return (int)pos;
}

int ff_samples_csv(const ff_sample *s, unsigned n, char *buf, size_t cap)
{
    size_t pos = 0;
    unsigned i;

    if (cap == 0) {
        return -1;
    }
    buf[0] = '\0';
    if (put(buf, cap, &pos, "t_ms,engine,light,drew,echo,copy_ms,diff_ms,draw_ms,cpu_ms,present_ms,age_ms\n")) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (put(buf, cap, &pos, "%.3f,%s,%u,%u,%u,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n", s[i].t_ms,
                ff_engine_name(s[i].engine), s[i].light, s[i].drew, s[i].echo, (double)s[i].copy_ms,
                (double)s[i].diff_ms, (double)s[i].draw_ms, (double)s[i].cpu_ms, (double)s[i].present_ms,
                (double)s[i].age_ms)) {
            return -1;
        }
    }
    return (int)pos;
}
