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

    /* по умолчанию английский, после выбора русский */
    CHECK(wcscmp(ff_tr(S_MONITOR), L"Monitor") == 0);
    ff_i18n_set(FF_LANG_RU);
    CHECK(wcscmp(ff_tr(S_MONITOR), L"Монитор") == 0);
    TEST_DONE("i18n");
}
