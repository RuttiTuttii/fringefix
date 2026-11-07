#include "bench.h"

#include <string.h>

static CRITICAL_SECTION g_lock;
static int g_ready;
static LARGE_INTEGER g_freq;
static LARGE_INTEGER g_base;
static volatile LONG g_wanted;
static volatile LONG g_force;
static ff_ring g_ring;
static ff_pring g_pring;
static int g_req;
static int g_eff;
static int g_cs_ok;
static char g_note[160];
static char g_gpu[96];

void bench_init(void)
{
    if (g_ready) {
        return;
    }
    InitializeCriticalSection(&g_lock);
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_base);
    g_ready = 1;
}

void bench_set_wanted(int on)
{
    InterlockedExchange(&g_wanted, on ? 1 : 0);
}

int bench_wanted(void)
{
    return g_wanted != 0;
}

void bench_set_force(int on)
{
    InterlockedExchange(&g_force, on ? 1 : 0);
}

int bench_force(void)
{
    return g_force != 0;
}

double bench_ticks_to_ms(LONGLONG qpc_delta)
{
    return g_freq.QuadPart ? (double)qpc_delta * 1000.0 / (double)g_freq.QuadPart : 0.0;
}

double bench_now_ms(void)
{
    LARGE_INTEGER now;

    QueryPerformanceCounter(&now);
    return bench_ticks_to_ms(now.QuadPart - g_base.QuadPart);
}

void bench_push(const ff_sample *s)
{
    EnterCriticalSection(&g_lock);
    ff_ring_push(&g_ring, s);
    LeaveCriticalSection(&g_lock);
}

void bench_push_power(const ff_power_sample *p)
{
    EnterCriticalSection(&g_lock);
    ff_pring_push(&g_pring, p);
    LeaveCriticalSection(&g_lock);
}

unsigned bench_copy(ff_sample *out, unsigned max)
{
    unsigned n;

    EnterCriticalSection(&g_lock);
    n = ff_ring_copy(&g_ring, out, max);
    LeaveCriticalSection(&g_lock);
    return n;
}

unsigned bench_copy_power(ff_power_sample *out, unsigned max)
{
    unsigned n;

    EnterCriticalSection(&g_lock);
    n = ff_pring_copy(&g_pring, out, max);
    LeaveCriticalSection(&g_lock);
    return n;
}

void bench_clear(void)
{
    EnterCriticalSection(&g_lock);
    g_ring.head = g_ring.count = 0;
    g_pring.head = g_pring.count = 0;
    LeaveCriticalSection(&g_lock);
}

void bench_set_engine_info(int requested, int effective, int cs_ok, const char *note)
{
    EnterCriticalSection(&g_lock);
    g_req = requested;
    g_eff = effective;
    g_cs_ok = cs_ok;
    strncpy(g_note, note ? note : "", sizeof g_note - 1);
    g_note[sizeof g_note - 1] = '\0';
    LeaveCriticalSection(&g_lock);
}

void bench_get_engine_info(int *requested, int *effective, int *cs_ok, char *note, size_t n)
{
    EnterCriticalSection(&g_lock);
    *requested = g_req;
    *effective = g_eff;
    *cs_ok = g_cs_ok;
    if (n > 0) {
        strncpy(note, g_note, n - 1);
        note[n - 1] = '\0';
    }
    LeaveCriticalSection(&g_lock);
}

void bench_set_gpu_name(const char *name)
{
    EnterCriticalSection(&g_lock);
    strncpy(g_gpu, name, sizeof g_gpu - 1);
    g_gpu[sizeof g_gpu - 1] = '\0';
    LeaveCriticalSection(&g_lock);
}

void bench_get_gpu_name(char *out, size_t n)
{
    EnterCriticalSection(&g_lock);
    strncpy(out, g_gpu, n - 1);
    out[n - 1] = '\0';
    LeaveCriticalSection(&g_lock);
}
