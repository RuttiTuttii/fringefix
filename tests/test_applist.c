#include "applist.h"
#include "test.h"

int main(void)
{
    /* имя, путь с любым регистром и слешами */
    CHECK(ff_app_is_known_nongame(L"chrome.exe"));
    CHECK(ff_app_is_known_nongame(L"C:\\Program Files\\Google\\Chrome\\Application\\CHROME.EXE"));
    CHECK(ff_app_is_known_nongame(L"/usr/bin/Code.exe"));
    CHECK(ff_app_is_known_nongame(L"notepad++.exe"));

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
    TEST_DONE("applist");
}
