#include <string.h>
#include <wchar.h>

#include "applist.h"
#include "test.h"

int main(void)
{
    /* имя, путь с любым регистром и слешами */
    CHECK(ff_app_is_known_nongame(L"chrome.exe"));
    CHECK(ff_app_is_known_nongame(L"C:\\Program Files\\Google\\Chrome\\Application\\CHROME.EXE"));
    CHECK(ff_app_is_known_nongame(L"/usr/bin/Code.exe"));
    CHECK(ff_app_is_known_nongame(L"notepad++.exe"));
    CHECK(ff_app_is_known_nongame(L"C:\\Program Files\\NVIDIA Corporation\\NVIDIA app\\CEF\\NVIDIA app.exe"));

    /* игры и незнакомые программы не в списке, как и похожие имена */
    CHECK(!ff_app_is_known_nongame(L"eldenring.exe"));
    CHECK(!ff_app_is_known_nongame(L"chrome.exe.bak"));
    CHECK(!ff_app_is_known_nongame(L"mychrome.exe"));
    CHECK(!ff_app_is_known_nongame(L"chrome"));

    /* пустое, NULL и очень длинное имя */
    CHECK(!ff_app_is_known_nongame(NULL));
    CHECK(!ff_app_is_known_nongame(L""));
    CHECK(!ff_app_is_known_nongame(L"C:\\dir\\"));
    CHECK(!ff_app_is_known_nongame(
        L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.exe"));
    /* список имён: регистр, пробелы, пути, имя без .exe */
    CHECK(ff_app_list_has("Game.exe; other ;x.exe", L"C:\\Games\\game.exe"));
    CHECK(ff_app_list_has("Game.exe; other ;x.exe", L"OTHER.EXE"));
    CHECK(ff_app_list_has("Game.exe; other ;x.exe", L"x.exe"));
    CHECK(!ff_app_list_has("Game.exe; other ;x.exe", L"gam.exe"));
    CHECK(!ff_app_list_has("Game.exe;;", L"other.exe"));
    CHECK(!ff_app_list_has("", L"game.exe"));
    CHECK(!ff_app_list_has(NULL, L"game.exe"));
    CHECK(!ff_app_list_has("game.exe", NULL));
    /* точка у элемента значит имя целиком: "game.exe" не совпадает с "game" */
    CHECK(!ff_app_list_has("game.exe", L"game"));
    /* длинное имя: то, что добавилось, должно и находиться, и убираться */
    {
        char list[128] = "";
        wchar_t longname[64];
        int i;

        for (i = 0; i < 59; i++) {
            longname[i] = L'a';
        }
        longname[59] = L'\0';
        wcscat(longname, L".exe");
        CHECK(wcslen(longname) == 63);
        CHECK(ff_app_list_add(list, sizeof list, longname) == 1);
        CHECK(ff_app_list_has(list, longname));
        CHECK(ff_app_list_add(list, sizeof list, longname) == 0);
        CHECK(ff_app_list_remove(list, sizeof list, longname) == 1 && list[0] == '\0');
    }
    CHECK(ff_app_is_known_nongame(L"idea64.exe") && ff_app_is_known_nongame(L"C:\\Windows\\System32\\Taskmgr.exe"));
    /* добавление: имя в нижнем регистре, повтор и путь не дублируют, не-ASCII и точка с запятой отвергаются */
    {
        char list[20] = "";

        CHECK(ff_app_list_add(list, sizeof list, L"C:\\Apps\\Viewer.EXE") == 1 && strcmp(list, "viewer.exe") == 0);
        CHECK(ff_app_list_add(list, sizeof list, L"viewer.exe") == 0 && strcmp(list, "viewer.exe") == 0);
        CHECK(ff_app_list_add(list, sizeof list, L"a.exe") == 1 && strcmp(list, "viewer.exe;a.exe") == 0);
        /* не помещается вместе с разделителем: список не меняется */
        CHECK(ff_app_list_add(list, sizeof list, L"b.exe") == -1 && strcmp(list, "viewer.exe;a.exe") == 0);
        CHECK(ff_app_list_add(list, sizeof list, L"\x0438\x0433\x0440\x0430.exe") == -1);
        CHECK(ff_app_list_add(list, sizeof list, L"x;y.exe") == -1);
        CHECK(ff_app_list_add(list, sizeof list, NULL) == -1);
    }
    /* удаление: по имени или пути, остальное сохраняется, повтор ничего не находит */
    {
        char list[64] = "a.exe; b.exe ;c";

        CHECK(ff_app_list_remove(list, sizeof list, L"D:\\B.exe") == 1 && strcmp(list, "a.exe;c") == 0);
        CHECK(ff_app_list_remove(list, sizeof list, L"b.exe") == 0 && strcmp(list, "a.exe;c") == 0);
        CHECK(ff_app_list_remove(list, sizeof list, L"c.exe") == 1 && strcmp(list, "a.exe") == 0);
        CHECK(ff_app_list_remove(list, sizeof list, L"a.exe") == 1 && list[0] == '\0');
    }
    TEST_DONE("applist");
}
