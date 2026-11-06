#ifndef FRINGEFIX_POWER_H
#define FRINGEFIX_POWER_H

#include "benchstat.h"

/* потребление и загрузка видеокарты через NVML, который ставит драйвер NVIDIA (nvml.dll). на других видеокартах
 * и без драйвера эти поля остаются -1, процессорная загрузка программы считается всегда */

/* подгружает nvml.dll и выбирает устройство по имени адаптера DXGI; 0 если NVML работает, -1 если нет */
int power_open(const char *adapter_name);
void power_close(void);

/* один опрос; поля, которых нет, равны -1, t_ms ставит вызывающий */
void power_read(ff_power_sample *s);

/* имя устройства NVML для подписи в окне; пусто, если NVML недоступен */
const char *power_device_name(void);

#endif
