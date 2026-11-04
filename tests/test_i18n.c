#include <stddef.h>
#include <wchar.h>

#include "i18n.h"
#include "test.h"

static int count_percent(const wchar_t *s)
{
    int n = 0;

    for (; *s; s++) {
        n += *s == L'%';
    }
    return n;
}

int main(void)
{
    int id;

    for (id = 0; id < S_COUNT; id++) {
        const wchar_t *ru = ff_tr_lang(FF_LANG_RU, (str_id)id);
        const wchar_t *en = ff_tr_lang(FF_LANG_EN, (str_id)id);

        /* пустая строка значит, что в таблице забыли перевод */
        CHECK(ru[0] != L'\0');
        CHECK(en[0] != L'\0');
        /* число подстановок должно совпадать, иначе swprintf прочтёт чужой аргумент */
        CHECK(count_percent(ru) == count_percent(en));
    }
    CHECK(ff_tr_lang(FF_LANG_EN, (str_id)S_COUNT)[0] == L'\0');
    CHECK(ff_tr_lang(FF_LANG_EN, (str_id)-1)[0] == L'\0');

    /* язык «как в системе»: русский только у русского Windows, остальные, в том числе европейские, получают английский.
     * основные языки: 0x09 английский, 0x07 немецкий, 0x0c французский, 0x0a испанский, 0x10 итальянский,
     * 0x15 польский, 0x13 нидерландский, 0x1d шведский, 0x22 украинский, 0x19 русский */
    {
        static const unsigned OTHER[] = { 0x09, 0x07, 0x0c, 0x0a, 0x10, 0x15, 0x13, 0x1d, 0x22, 0x00 };
        size_t i;

        for (i = 0; i < sizeof OTHER / sizeof OTHER[0]; i++) {
            CHECK(ff_i18n_resolve(0, OTHER[i]) == FF_LANG_EN);
        }
        CHECK(ff_i18n_resolve(0, 0x19) == FF_LANG_RU);
        /* ручной выбор сильнее системы в обе стороны */
        CHECK(ff_i18n_resolve(1, 0x07) == FF_LANG_RU);
        CHECK(ff_i18n_resolve(2, 0x19) == FF_LANG_EN);
    }

    /* по умолчанию английский, после выбора русский */
    CHECK(wcscmp(ff_tr(S_MONITOR), L"Monitor") == 0);
    ff_i18n_set(FF_LANG_RU);
    CHECK(wcscmp(ff_tr(S_MONITOR), L"Монитор") == 0);
    TEST_DONE("i18n");
}
