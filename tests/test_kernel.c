#include <math.h>

#include "kernel.h"
#include "test.h"

#define W 13
#define H 11

/* картинка из случайных значений, три канала; координаты зажимаются к краю, как в шейдере */
static float img[H][W][3];

static float px(int x, int y, int ch)
{
    x = x < 0 ? 0 : (x >= W ? W - 1 : x);
    y = y < 0 ? 0 : (y >= H ? H - 1 : y);
    return img[y][x][ch];
}

/* свой генератор: rand() у разных библиотек даёт разные ряды, а тест должен проходить одинаково везде */
static unsigned rng_state = 12345u;

static float frand(float lo, float hi)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return lo + (hi - lo) * ((float)(rng_state & 0xFFFFFFu) / (float)0x1000000u);
}

/* прямой расчёт по формулам классического шейдера: билинейное чтение в точке pix - s и лапласиан вокруг floor(pix - s) */
static float reference(int x, int y, int ch, const float s[2], float sharpen)
{
    float p[2] = { (float)x + 0.5f - s[0], (float)y + 0.5f - s[1] };
    float u[2] = { p[0] - 0.5f, p[1] - 0.5f };
    int i0 = (int)floorf(u[0]);
    int j0 = (int)floorf(u[1]);
    float fx = u[0] - (float)i0;
    float fy = u[1] - (float)j0;
    int qx = (int)floorf(p[0]);
    int qy = (int)floorf(p[1]);
    float v = (1 - fx) * (1 - fy) * px(i0, j0, ch) + fx * (1 - fy) * px(i0 + 1, j0, ch)
              + (1 - fx) * fy * px(i0, j0 + 1, ch) + fx * fy * px(i0 + 1, j0 + 1, ch);
    float lap = px(qx - 1, qy, ch) + px(qx + 1, qy, ch) + px(qx, qy - 1, ch) + px(qx, qy + 1, ch)
                - 4.0f * px(qx, qy, ch);
    float f0 = s[0] - nearbyintf(s[0]);
    float f1 = s[1] - nearbyintf(s[1]);
    float k = 1.72f * (f0 * f0 + f1 * f1);

    k = (k < 0.35f ? k : 0.35f) + sharpen * 0.35f;
    return v - k * lap;
}

static float apply(const ff_kernel *k, int x, int y, int ch)
{
    float v = 0.0f;
    int j;
    int i;

    for (j = 0; j < 3; j++) {
        for (i = 0; i < 3; i++) {
            v += k->w[ch][j * 3 + i] * px(x + k->base[ch][0] + i - 1, y + k->base[ch][1] + j - 1, ch);
        }
    }
    return v;
}

/* окна дают то же число, что и прямой расчёт, на всех пикселях, включая края картинки */
static void check_against_reference(const float shift[6], float sharpen)
{
    ff_kernel k;
    float c[2];
    int a;
    int ch;
    int x;
    int y;

    ff_kernel_build(shift, sharpen, &k);
    for (a = 0; a < 2; a++) {
        float lo = fminf(shift[a], fminf(shift[2 + a], shift[4 + a]));
        float hi = fmaxf(shift[a], fmaxf(shift[2 + a], shift[4 + a]));

        c[a] = -(lo + hi) * 0.5f;
    }
    for (ch = 0; ch < 3; ch++) {
        float s[2] = { shift[ch * 2] + c[0], shift[ch * 2 + 1] + c[1] };

        for (y = 0; y < H; y++) {
            for (x = 0; x < W; x++) {
                CHECK(fabsf(apply(&k, x, y, ch) - reference(x, y, ch, s, sharpen)) < 2e-4f);
            }
        }
    }
}

static void test_random_shifts(void)
{
    int t;
    int x;
    int y;
    int ch;

    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            for (ch = 0; ch < 3; ch++) {
                img[y][x][ch] = frand(0.0f, 1.0f);
            }
        }
    }
    for (t = 0; t < 400; t++) {
        float shift[6];

        /* половина проб в обычных пределах, половина на всём диапазоне настроек */
        float range = t % 2 ? 2.0f : 0.5f;

        for (ch = 0; ch < 3; ch++) {
            shift[ch * 2] = frand(-range, range);
            shift[ch * 2 + 1] = frand(-range, range);
        }
        check_against_reference(shift, frand(-1.0f, 1.0f));
    }
}

static void test_bundled_profile(void)
{
    /* профиль QD-OLED gen 3 4: разброс меньше пикселя по обеим осям, окна совпадают */
    static const float shift[6] = { -0.16f, -0.41f, 0.10f, 0.10f, -0.05f, -0.22f };
    ff_kernel k;

    ff_kernel_build(shift, -0.08f, &k);
    CHECK(k.shared == 1);
    check_against_reference(shift, -0.08f);
}

static void test_shared_condition(void)
{
    ff_kernel k;
    float a[6] = { -0.49f, 0.0f, 0.0f, 0.0f, 0.49f, 0.0f };
    float b[6] = { -0.7f, 0.0f, 0.0f, 0.0f, 0.7f, 0.0f };
    float z[6] = { 0 };

    /* разброс 0.98 пикселя: клетка общая; разброс 1.4: клетки разные */
    ff_kernel_build(a, 0.0f, &k);
    CHECK(k.shared == 1);
    ff_kernel_build(b, 0.0f, &k);
    CHECK(k.shared == 0);

    /* нулевые сдвиги и нулевая резкость: окно это единица в центре */
    ff_kernel_build(z, 0.0f, &k);
    CHECK(k.shared == 1);
    CHECK(fabsf(k.w[1][4] - 1.0f) < 1e-6f && fabsf(k.w[1][0]) < 1e-6f && fabsf(k.w[1][8]) < 1e-6f);
}

/* сумма весов равна единице при нулевой силе лапласиана и тоже единице при любой: лапласиан сумму не меняет,
 * поэтому однотонная область остаётся прежней */
static void test_flat_area_unchanged(void)
{
    ff_kernel k;
    float shift[6] = { 0.3f, -0.2f, -0.1f, 0.4f, 0.0f, 0.1f };
    int ch;
    int i;

    ff_kernel_build(shift, 0.7f, &k);
    for (ch = 0; ch < 3; ch++) {
        float sum = 0.0f;

        for (i = 0; i < 9; i++) {
            sum += k.w[ch][i];
        }
        CHECK(fabsf(sum - 1.0f) < 1e-5f);
    }
}

int main(void)
{
    test_random_shifts();
    test_bundled_profile();
    test_shared_condition();
    test_flat_area_unchanged();
    TEST_DONE("kernel");
}
