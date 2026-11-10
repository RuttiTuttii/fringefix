#include <stdio.h>
#include <string.h>

#include "profile.h"
#include "test.h"

static void test_roundtrip(void)
{
    ff_view a;
    ff_view b;
    char buf[512];
    char tiny[32];

    memset(&a, 0, sizeof a);
    a.shift[0][0] = 0.25f;
    a.shift[0][1] = -0.5f;
    a.shift[1][1] = 0.125f;
    a.shift[2][0] = -1.5f;
    a.sharpen = -0.25f;
    a.enabled = 0;

    CHECK(ff_profile_format(&a, buf, sizeof buf) > 0);
    memset(&b, 0, sizeof b);
    b.enabled = 1;
    CHECK(ff_profile_parse(buf, &b) == 7);
    CHECK(memcmp(a.shift, b.shift, sizeof a.shift) == 0 && a.sharpen == b.sharpen);
    /* флаг включения в профиль не входит: загрузка профиля не выключает и не включает монитор */
    CHECK(b.enabled == 1);
    CHECK(ff_profile_format(&a, tiny, sizeof tiny) == -1);
}

static void test_clamp_and_junk(void)
{
    ff_view c;
    static const char json[] =
        "{\"format\":\"fringefix-profile\",\"red\":{\"x\":99,\"y\":\"abc\"},"
        "\"green\":{\"x\":-99},\"blue\":{\"x\":nan,\"y\":0.5},\"sharpen\":-9}";

    memset(&c, 0, sizeof c);
    /* x красного и x зелёного зажаты, y красного и x синего отброшены */
    CHECK(ff_profile_parse(json, &c) == 4);
    CHECK(c.shift[0][0] == 2.0f && c.shift[0][1] == 0.0f);
    CHECK(c.shift[1][0] == -2.0f);
    CHECK(c.shift[2][0] == 0.0f && c.shift[2][1] == 0.5f);
    CHECK(c.sharpen == -1.0f);
}

static void test_foreign_files(void)
{
    ff_view c;

    memset(&c, 0, sizeof c);
    CHECK(ff_profile_parse("", &c) == -1);
    CHECK(ff_profile_parse("{}", &c) == -1);
    CHECK(ff_profile_parse("{\"red\":{\"x\":1}}", &c) == -1);
    CHECK(ff_profile_parse("{\"format\":\"other\",\"red\":{\"x\":1}}", &c) == -1);
    CHECK(c.shift[0][0] == 0.0f);
    /* маркер есть, значений нет: это профиль, но пустой */
    CHECK(ff_profile_parse("{\"format\":\"fringefix-profile\"}", &c) == 0);
}

static void test_layout_variants(void)
{
    ff_view c;

    memset(&c, 0, sizeof c);
    /* пробелы, переводы строк и порядок ключей произвольные, объект канала без закрывающей скобки отбрасывается */
    CHECK(ff_profile_parse("{ \"sharpen\" :\n 0.5 , \"format\" : \"fringefix-profile\",\n"
                           "\"red\" : {\n \"y\" : 0.1 ,\n \"x\" : -0.2 } }", &c) == 3);
    CHECK(c.shift[0][0] == -0.2f && c.shift[0][1] == 0.1f && c.sharpen == 0.5f);
    CHECK(ff_profile_parse("{\"format\":\"fringefix-profile\",\"green\":{\"x\":1", &c) == 0);
}

/* комплектные профили из profiles/ вшиваются в exe, поэтому каждый должен читаться полностью */
static void test_bundled(void)
{
    static const char *const FILES[] = { "profiles/qd-oled-gen-3-4.json" };
    size_t i;

    for (i = 0; i < sizeof FILES / sizeof FILES[0]; i++) {
        ff_view c;
        char buf[4096];
        size_t n;
        FILE *f = fopen(FILES[i], "rb");

        CHECK(f != NULL);
        if (!f) {
            continue;
        }
        n = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        buf[n] = '\0';
        memset(&c, 0, sizeof c);
        CHECK(ff_profile_parse(buf, &c) == 7);
    }
}

int main(void)
{
    test_roundtrip();
    test_clamp_and_junk();
    test_foreign_files();
    test_layout_variants();
    test_bundled();
    TEST_DONE("profile");
}
