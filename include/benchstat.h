#ifndef FRINGEFIX_BENCHSTAT_H
#define FRINGEFIX_BENCHSTAT_H

#include <stddef.h>

/* замеры цикла обработки кадра и их сводка; модуль без платформенного кода, поэтому считается и проверяется на любой системе.
 * отрицательное значение времени значит "этап не выполнялся или не измерен" */

#define FF_RING_CAP 4096
#define FF_PRING_CAP 1024
/* движок "фильтр выключен": замеров кадров нет, меряется только потребление */
#define FF_ENGINE_OFF 3

/* один обработанный кадр: полученный от захвата, скопированный и, по обстоятельствам, сравнённый и нарисованный */
typedef struct {
    /* момент получения кадра, мс от старта счётчика */
    double t_ms;
    /* время этапов на видеокарте: копия кадра, сравнение с прошлым, отрисовка шейдером */
    float copy_ms;
    float diff_ms;
    float draw_ms;
    /* процессорное время от получения кадра до вызова Present, вместе с ожиданием результата сравнения */
    float cpu_ms;
    /* длительность самого вызова Present; он ждёт вертикальной синхронизации, если очередь полна */
    float present_ms;
    /* сколько кадр пролежал у DWM до получения: от LastPresentTime до AcquireNextFrame */
    float age_ms;
    /* действующий движок 0..2, а не заказанный: без compute-шейдера сюда попадает 0 */
    unsigned char engine;
    unsigned char light;
    unsigned char drew;
    /* сравнение признало кадр эхом собственного окна */
    unsigned char echo;
} ff_sample;

/* потребление и загрузка видеокарты и процесса; опрашивается примерно 4 раза в секунду */
typedef struct {
    double t_ms;
    float watts;
    float util_gpu;
    float util_mem;
    float clock_mhz;
    float temp_c;
    /* процессорное время программы в процентах от всех логических процессоров */
    float cpu_pct;
} ff_power_sample;

typedef struct {
    ff_sample s[FF_RING_CAP];
    unsigned head;
    unsigned count;
} ff_ring;

typedef struct {
    ff_power_sample s[FF_PRING_CAP];
    unsigned head;
    unsigned count;
} ff_pring;

void ff_ring_push(ff_ring *r, const ff_sample *s);
void ff_pring_push(ff_pring *r, const ff_power_sample *s);

/* копирует последние до max записей в хронологическом порядке, возвращает их число */
unsigned ff_ring_copy(const ff_ring *r, ff_sample *out, unsigned max);
unsigned ff_pring_copy(const ff_pring *r, ff_power_sample *out, unsigned max);

typedef struct {
    unsigned n;
    float mean;
    float p50;
    float p95;
    float p99;
    float max;
} ff_stat;

/* среднее, перцентили по ближайшему рангу и максимум; v не меняется, пустой набор даёт нули */
void ff_stat_compute(const float *v, unsigned n, ff_stat *out);

/* итог по одному движку за окно времени [t0, t1) */
typedef struct {
    int engine;
    double seconds;
    unsigned cycles;
    unsigned draws;
    ff_stat copy;
    ff_stat diff;
    ff_stat draw;
    /* сумма трёх этапов в одном цикле */
    ff_stat gpu;
    ff_stat cpu;
    ff_stat present;
    ff_stat age;
    /* промежутки между соседними отрисовками */
    ff_stat interval;
    /* средние за окно по замерам потребления; отрицательное значит нет данных */
    float watts;
    float util_gpu;
    float clock_mhz;
    float temp_c;
    float cpu_pct;
} ff_run_row;

/* собирает строку итога; кадры берутся только с данным движком и без облегчённого режима,
 * для FF_ENGINE_OFF кадров нет и считается одно потребление */
void ff_run_summarize(const ff_sample *s, unsigned ns, const ff_power_sample *p, unsigned np,
                      double t0, double t1, int engine, ff_run_row *row);

/* таблица итогов и сырые кадры в CSV с точкой в числах; длина или -1, если буфер мал */
int ff_run_csv(const ff_run_row *rows, int n, char *buf, size_t cap);
int ff_samples_csv(const ff_sample *s, unsigned n, char *buf, size_t cap);

const char *ff_engine_name(int engine);

#endif
