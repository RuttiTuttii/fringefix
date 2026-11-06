#include "power.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

typedef int nvml_ret;
typedef struct nvml_dev_st *nvml_dev;
typedef struct {
    unsigned gpu;
    unsigned memory;
} nvml_util;

#define NVML_OK 0
#define NVML_CLOCK_GRAPHICS 0
#define NVML_TEMP_GPU 0

static HMODULE g_dll;
static nvml_dev g_dev;
static char g_name[96];
static nvml_ret (*p_init)(void);
static nvml_ret (*p_shutdown)(void);
static nvml_ret (*p_count)(unsigned *);
static nvml_ret (*p_handle)(unsigned, nvml_dev *);
static nvml_ret (*p_name)(nvml_dev, char *, unsigned);
static nvml_ret (*p_power)(nvml_dev, unsigned *);
static nvml_ret (*p_util)(nvml_dev, nvml_util *);
static nvml_ret (*p_clock)(nvml_dev, int, unsigned *);
static nvml_ret (*p_temp)(nvml_dev, int, unsigned *);

/* процессорная загрузка программы: приращение времени процесса за приращение стенных часов */
static ULONGLONG g_cpu_last;
static ULONGLONG g_wall_last;

static ULONGLONG ft_u64(FILETIME f)
{
    return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime;
}

static HMODULE load_nvml(void)
{
    wchar_t path[MAX_PATH];
    wchar_t pf[MAX_PATH];
    HMODULE h = LoadLibraryExW(L"nvml.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (h) {
        return h;
    }
    /* старые драйверы кладут nvml.dll не в System32, а в папку NVSMI */
    if (GetEnvironmentVariableW(L"ProgramFiles", pf, MAX_PATH) > 0
        && _snwprintf(path, MAX_PATH, L"%ls\\NVIDIA Corporation\\NVSMI\\nvml.dll", pf) > 0) {
        path[MAX_PATH - 1] = L'\0';
        h = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    }
    return h;
}

/* указатель на функцию из FARPROC копируется побайтно: прямое приведение типа нарушает правила псевдонимов */
#define LOAD(var, sym)                              \
    do {                                            \
        FARPROC f_ = GetProcAddress(g_dll, sym);    \
        memcpy(&(var), &f_, sizeof f_);             \
    } while (0)

static int lower_contains(const char *hay, const char *needle)
{
    char a[96];
    char b[96];
    size_t i;

    snprintf(a, sizeof a, "%s", hay);
    snprintf(b, sizeof b, "%s", needle);
    for (i = 0; a[i]; i++) {
        a[i] = (char)((a[i] >= 'A' && a[i] <= 'Z') ? a[i] + 32 : a[i]);
    }
    for (i = 0; b[i]; i++) {
        b[i] = (char)((b[i] >= 'A' && b[i] <= 'Z') ? b[i] + 32 : b[i]);
    }
    return b[0] && strstr(a, b) != NULL;
}

int power_open(const char *adapter_name)
{
    unsigned n = 0;
    unsigned i;

    power_close();
    g_dll = load_nvml();
    if (!g_dll) {
        return -1;
    }
    LOAD(p_init, "nvmlInit_v2");
    LOAD(p_shutdown, "nvmlShutdown");
    LOAD(p_count, "nvmlDeviceGetCount_v2");
    LOAD(p_handle, "nvmlDeviceGetHandleByIndex_v2");
    LOAD(p_name, "nvmlDeviceGetName");
    LOAD(p_power, "nvmlDeviceGetPowerUsage");
    LOAD(p_util, "nvmlDeviceGetUtilizationRates");
    LOAD(p_clock, "nvmlDeviceGetClockInfo");
    LOAD(p_temp, "nvmlDeviceGetTemperature");
    if (!p_init || !p_shutdown || !p_count || !p_handle || !p_name || p_init() != NVML_OK) {
        power_close();
        return -1;
    }
    if (p_count(&n) != NVML_OK || n == 0) {
        power_close();
        return -1;
    }
    /* устройство с тем же именем, что у адаптера оверлея; если совпадения нет, первое */
    for (i = 0; i < n; i++) {
        nvml_dev d;
        char name[96];

        if (p_handle(i, &d) != NVML_OK || p_name(d, name, sizeof name) != NVML_OK) {
            continue;
        }
        if (!g_dev || lower_contains(adapter_name, name) || lower_contains(name, adapter_name)) {
            g_dev = d;
            snprintf(g_name, sizeof g_name, "%s", name);
            if (lower_contains(adapter_name, name) || lower_contains(name, adapter_name)) {
                break;
            }
        }
    }
    if (!g_dev) {
        power_close();
        return -1;
    }
    return 0;
}

void power_close(void)
{
    if (g_dll) {
        if (g_dev && p_shutdown) {
            p_shutdown();
        }
        FreeLibrary(g_dll);
    }
    g_dll = NULL;
    g_dev = NULL;
    g_name[0] = '\0';
    p_init = p_shutdown = NULL;
    p_count = NULL;
    p_handle = NULL;
    p_name = NULL;
    p_power = NULL;
    p_util = NULL;
    p_clock = NULL;
    p_temp = NULL;
}

void power_read(ff_power_sample *s)
{
    FILETIME cr, ex, ke, us, now;
    ULONGLONG cpu;
    ULONGLONG wall;
    SYSTEM_INFO si;

    s->watts = s->util_gpu = s->util_mem = s->clock_mhz = s->temp_c = s->cpu_pct = -1.0f;
    if (g_dev) {
        unsigned mw = 0;
        unsigned mhz = 0;
        unsigned deg = 0;
        nvml_util u;

        if (p_power && p_power(g_dev, &mw) == NVML_OK) {
            s->watts = (float)mw / 1000.0f;
        }
        if (p_util && p_util(g_dev, &u) == NVML_OK) {
            s->util_gpu = (float)u.gpu;
            s->util_mem = (float)u.memory;
        }
        if (p_clock && p_clock(g_dev, NVML_CLOCK_GRAPHICS, &mhz) == NVML_OK) {
            s->clock_mhz = (float)mhz;
        }
        if (p_temp && p_temp(g_dev, NVML_TEMP_GPU, &deg) == NVML_OK) {
            s->temp_c = (float)deg;
        }
    }
    GetProcessTimes(GetCurrentProcess(), &cr, &ex, &ke, &us);
    GetSystemTimeAsFileTime(&now);
    cpu = ft_u64(ke) + ft_u64(us);
    wall = ft_u64(now);
    GetSystemInfo(&si);
    if (g_wall_last && wall > g_wall_last && si.dwNumberOfProcessors > 0) {
        s->cpu_pct = (float)(100.0 * (double)(cpu - g_cpu_last) / (double)(wall - g_wall_last)
                             / (double)si.dwNumberOfProcessors);
    }
    g_cpu_last = cpu;
    g_wall_last = wall;
}

const char *power_device_name(void)
{
    return g_name;
}
