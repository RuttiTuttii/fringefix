#include <string.h>

#include "rspaths.h"
#include "test.h"

int main(void)
{
    char v[128];
    const char *ini = "\xEF\xBB\xBF; comment\r\n[APP]\r\nEffectSearchPaths=wrong\r\n\r\n[GENERAL]\r\n"
                      "PerformanceMode=0\r\n  effectsearchpaths = .\\reshade-shaders\\Shaders\\** , D:\\x\\Shaders  \r\n"
                      "[ADDON]\r\nAddonPath=addons\r\n";

    /* значение берётся из своей секции, ключ и секция без учёта регистра, BOM и перевод строки не мешают */
    CHECK(ff_rs_ini_get(ini, "GENERAL", "EffectSearchPaths", v, sizeof v) == 1);
    CHECK(strcmp(v, ".\\reshade-shaders\\Shaders\\** , D:\\x\\Shaders") == 0);
    CHECK(ff_rs_ini_get(ini, "general", "EFFECTSEARCHPATHS", v, sizeof v) == 1);
    CHECK(ff_rs_ini_get(ini, "ADDON", "AddonPath", v, sizeof v) == 1 && strcmp(v, "addons") == 0);
    CHECK(ff_rs_ini_get(ini, "APP", "EffectSearchPaths", v, sizeof v) == 1 && strcmp(v, "wrong") == 0);
    CHECK(ff_rs_ini_get(ini, "ADDON", "Missing", v, sizeof v) == 0 && v[0] == '\0');
    CHECK(ff_rs_ini_get(ini, "NOPE", "AddonPath", v, sizeof v) == 0);
    CHECK(ff_rs_ini_get("", "GENERAL", "x", v, sizeof v) == 0);
    /* без перевода строки в конце и с обрезкой по размеру буфера */
    CHECK(ff_rs_ini_get("[A]\nk=abcdef", "A", "k", v, 4) == 1 && strcmp(v, "abc") == 0);
    CHECK(ff_rs_ini_get("[A]\nk=1", "A", "k", v, 0) == 0);

    /* первый путь списка без звёздочек и разделителя */
    CHECK(ff_rs_first_path(".\\reshade-shaders\\Shaders\\** , D:\\x", v, sizeof v) == 25);
    CHECK(strcmp(v, ".\\reshade-shaders\\Shaders") == 0);
    CHECK(ff_rs_first_path("\"C:\\My Shaders\\*\"", v, sizeof v) > 0 && strcmp(v, "C:\\My Shaders") == 0);
    CHECK(ff_rs_first_path("Shaders/**", v, sizeof v) == 7 && strcmp(v, "Shaders") == 0);
    CHECK(ff_rs_first_path("", v, sizeof v) == 0);
    CHECK(ff_rs_first_path(" , x", v, sizeof v) == 0);
    CHECK(ff_rs_first_path("abcdef", v, 4) == 3 && strcmp(v, "abc") == 0);

    CHECK(ff_rs_is_absolute("C:\\a") && ff_rs_is_absolute("d:/a") && ff_rs_is_absolute("\\\\srv\\share"));
    CHECK(!ff_rs_is_absolute(".\\a") && !ff_rs_is_absolute("reshade-shaders") && !ff_rs_is_absolute(""));

    TEST_DONE("rspaths");
}
