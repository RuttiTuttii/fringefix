#include "layout.h"

#include <math.h>

#include "config.h"

/* смещения сабпикселей от центра пикселя в долях пикселя, [r, g, b][x, y], y вниз */
static const double OFFSET[FF_LAYOUT_COUNT][3][2] = {
    [FF_LAYOUT_NONE] = { { 0, 0 }, { 0, 0 }, { 0, 0 } },
    [FF_LAYOUT_QD_TRIANGLE] = { { -0.25, 0.25 }, { 0.0, -0.25 }, { 0.25, 0.25 } },
};

static float limit(double v)
{
    return (float)(v < -FF_SHIFT_LIMIT ? -FF_SHIFT_LIMIT : (v > FF_SHIFT_LIMIT ? FF_SHIFT_LIMIT : v));
}

void ff_layout_shifts(ff_layout layout, double kx, double ky, float shift[3][2])
{
    int c;

    if (layout < 0 || layout >= FF_LAYOUT_COUNT || !isfinite(kx) || !isfinite(ky)) {
        layout = FF_LAYOUT_NONE;
        kx = ky = 0.0;
    }
    for (c = 0; c < 3; c++) {
        /* минус нуля даёт -0.0, а профиль и поля ввода должны показывать 0 */
        shift[c][0] = limit(-OFFSET[layout][c][0] * kx + 0.0);
        shift[c][1] = limit(-OFFSET[layout][c][1] * ky + 0.0);
    }
}
