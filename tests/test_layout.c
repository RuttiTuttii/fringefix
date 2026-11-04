#include <math.h>

#include "layout.h"
#include "test.h"

static int near(float a, float b)
{
    return fabsf(a - b) < 1e-6f;
}

int main(void)
{
    float s[3][2];

    /* QD-OLED: зелёный над парой, поэтому его картинка едет вниз, а красного и синего вверх;
     * знаки совпадают с найденными вручную: зелёный по вертикали плюс, красный и синий минус */
    ff_layout_shifts(FF_LAYOUT_QD_TRIANGLE, 1.0, 1.0, s);
    CHECK(near(s[0][0], 0.25f) && near(s[0][1], -0.25f));
    CHECK(near(s[1][0], 0.0f) && near(s[1][1], 0.25f));
    CHECK(near(s[2][0], -0.25f) && near(s[2][1], -0.25f));

    /* оси независимы */
    ff_layout_shifts(FF_LAYOUT_QD_TRIANGLE, 2.0, 0.0, s);
    CHECK(near(s[0][0], 0.5f) && near(s[0][1], 0.0f) && near(s[1][1], 0.0f) && near(s[2][0], -0.5f));

    /* минус зеркалит: зелёный вниз становится вверх, красный и синий меняются сторонами */
    ff_layout_shifts(FF_LAYOUT_QD_TRIANGLE, -1.0, -1.0, s);
    CHECK(near(s[0][0], -0.25f) && near(s[1][1], -0.25f) && near(s[2][0], 0.25f));

    /* нулевая сила, пустая раскладка, мусор на входе: нули без отрицательного нуля */
    ff_layout_shifts(FF_LAYOUT_QD_TRIANGLE, 0.0, 0.0, s);
    CHECK(s[0][0] == 0.0f && !signbit(s[0][0]) && !signbit(s[1][1]));
    ff_layout_shifts(FF_LAYOUT_NONE, 3.0, 3.0, s);
    CHECK(s[0][0] == 0.0f && s[1][1] == 0.0f && s[2][1] == 0.0f);
    ff_layout_shifts((ff_layout)99, 1.0, 1.0, s);
    CHECK(s[0][0] == 0.0f && s[2][1] == 0.0f);
    ff_layout_shifts(FF_LAYOUT_QD_TRIANGLE, NAN, 1.0, s);
    CHECK(s[0][0] == 0.0f && s[1][1] == 0.0f);

    /* предел сдвига соблюдается и при огромной силе */
    ff_layout_shifts(FF_LAYOUT_QD_TRIANGLE, 100.0, -100.0, s);
    CHECK(near(s[0][0], 2.0f) && near(s[0][1], 2.0f) && near(s[1][1], -2.0f));
    TEST_DONE("layout");
}
