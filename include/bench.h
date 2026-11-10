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

/* панель замеров в оверлее: рисуется окном поверх оверлея, пока открыто окно диагностики и включена галка */
void bench_set_hud(int on);
int bench_hud(void);

/* секунд до отката пробной настройки для панели; -1, если проба не идёт */
void bench_set_trial_left(int seconds);
int bench_trial_left(void);

/* сбои поднятия устройства и оверлея: окно диагностики по их росту во время пробы откатывает настройку сразу,
 * не дожидаясь таймера */
void bench_note_gfx_failure(void);
unsigned bench_gfx_failures(void);

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

/* способ вывода и приоритет GPU: что заказано и что действует на самом деле (tearing и приоритет выше нуля
 * система может не дать), с пояснением при расхождении */
void bench_set_variant_info(int present_req, int present_eff, int prio_req, int prio_eff, const char *note);
void bench_get_variant_info(int *present_req, int *present_eff, int *prio_req, int *prio_eff, char *note, size_t n);

/* видеокарта, на которой работает оверлей, для выбора устройства в NVML */
void bench_set_gpu_name(const char *name);
void bench_get_gpu_name(char *out, size_t n);

#endif
