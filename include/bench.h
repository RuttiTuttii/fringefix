#ifndef FRINGEFIX_BENCH_H
#define FRINGEFIX_BENCH_H

#include <stddef.h>
#include <windows.h>

#include "benchstat.h"

/* общий сборщик замеров: кадры пишет поток захвата, окно диагностики читает.
 * замеры включаются только пока открыто окно диагностики или идёт прогон, иначе таймеры видеокарты не создают нагрузки */
void bench_init(void);

void bench_set_wanted(int on);
int bench_wanted(void);

/* нагрузочный режим: каждый полученный кадр рисуется, а не только изменившийся; цикл подпитывает сам себя
 * частотой монитора и показывает стоимость при постоянно меняющемся экране */
void bench_set_force(int on);
int bench_force(void);

/* миллисекунды от запуска счётчика, общая шкала для кадров и потребления */
double bench_now_ms(void);
double bench_ticks_to_ms(LONGLONG qpc_delta);

void bench_push(const ff_sample *s);
void bench_push_power(const ff_power_sample *p);
unsigned bench_copy(ff_sample *out, unsigned max);
unsigned bench_copy_power(ff_power_sample *out, unsigned max);
void bench_clear(void);

/* что запросили, что работает на самом деле, доступен ли compute-шейдер, и пояснение при отказе */
void bench_set_engine_info(int requested, int effective, int cs_ok, const char *note);
void bench_get_engine_info(int *requested, int *effective, int *cs_ok, char *note, size_t n);

/* видеокарта, на которой работает оверлей, для выбора устройства в NVML */
void bench_set_gpu_name(const char *name);
void bench_get_gpu_name(char *out, size_t n);

#endif
