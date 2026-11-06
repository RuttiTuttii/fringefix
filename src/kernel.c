#include "kernel.h"

#include <math.h>
#include <string.h>

/* сила лапласиана канала: общая формула классического шейдера */
static float lap_strength(const float s[2], float sharpen)
{
    float fx = s[0] - nearbyintf(s[0]);
    float fy = s[1] - nearbyintf(s[1]);
    float k = 1.72f * (fx * fx + fy * fy);

    return (k < 0.35f ? k : 0.35f) + sharpen * 0.35f;
}

/* веса одной оси: билинейная пара смещений off0 и off0 + 1 внутри окна из трёх клеток вокруг центра cb.
 * центр лапласиана floor(0.5 - s) всегда равен off0 или off0 + 1, поэтому пара помещается в окно */
static int axis_weights(float s, float wb[3])
{
    float u = -s;
    float fl = floorf(u);
    float fr = u - fl;
    int off0 = (int)fl;
    int cb = (int)floorf(0.5f - s);
    int i0 = off0 - cb + 1;

    memset(wb, 0, 3 * sizeof wb[0]);
    /* защита от погрешности float на самой границе клетки */
    if (i0 < 0) {
        i0 = 0;
    }
    if (i0 > 1) {
        i0 = 1;
    }
    wb[i0] += 1.0f - fr;
    wb[i0 + 1] += fr;
    return cb;
}

void ff_kernel_build(const float shift[6], float sharpen, ff_kernel *k)
{
    float c[2];
    int a;
    int ch;

    memset(k, 0, sizeof *k);
    for (a = 0; a < 2; a++) {
        float lo = shift[a];
        float hi = shift[a];

        for (ch = 1; ch < 3; ch++) {
            lo = shift[ch * 2 + a] < lo ? shift[ch * 2 + a] : lo;
            hi = shift[ch * 2 + a] > hi ? shift[ch * 2 + a] : hi;
        }
        c[a] = -(lo + hi) * 0.5f;
    }
    for (ch = 0; ch < 3; ch++) {
        float s[2];
        float wb[2][3];
        float kk;
        int j;
        int i;

        s[0] = shift[ch * 2] + c[0];
        s[1] = shift[ch * 2 + 1] + c[1];
        kk = lap_strength(s, sharpen);
        k->base[ch][0] = axis_weights(s[0], wb[0]);
        k->base[ch][1] = axis_weights(s[1], wb[1]);
        for (j = 0; j < 3; j++) {
            for (i = 0; i < 3; i++) {
                float lap = 0.0f;

                if (i == 1 && j == 1) {
                    lap = -4.0f;
                } else if (i == 1 || j == 1) {
                    lap = 1.0f;
                }
                k->w[ch][j * 3 + i] = wb[1][j] * wb[0][i] - kk * lap;
            }
        }
    }
    k->shared = k->base[0][0] == k->base[1][0] && k->base[0][0] == k->base[2][0]
                && k->base[0][1] == k->base[1][1] && k->base[0][1] == k->base[2][1];
}
