#ifndef FRINGEFIX_UI_H
#define FRINGEFIX_UI_H

#include <windows.h>

#include "app.h"
#include "report_send.h"
#include "update.h"

/* скрытое окно сообщений, значок в трее и глобальные хоткеи; 0 или -1 */
int ui_tray_create(ff_app *a);

/* регистрирует глобальные сочетания из a->cfg заново; бит 0 в ответе значит, что сочетание включения занято,
 * бит 1 то же про выход, ноль значит всё встало */
int ui_tray_bind_hotkeys(ff_app *a);

/* UTF-8 в UTF-16 для текстов из фоновых потоков */
void ui_utf8(const char *s, wchar_t *out, int n);

/* окно настроек: создаётся при первом показе, закрытие только прячет его */
void ui_settings_show(ff_app *a);
void ui_settings_set_status(const wchar_t *text);

/* значения конфига в элементы, если окно уже создано; нужно после загрузки профиля из меню трея */
void ui_settings_refresh(void);

/* окно настроек и справа от него тестовый паттерн */
void ui_pattern_show(ff_app *a);

/* выключатель фильтра в окне настроек по a->enabled, если окно уже создано; нужно, когда фильтр
 * переключили из трея, хоткеем или он отключился сам */
void ui_settings_refresh_power(void);

/* для главного цикла: Tab и Enter в окне настроек; ненулевой ответ значит сообщение съедено */
int ui_settings_translate(MSG *m);

/* списки программ «всегда применять» и «всегда на паузу» с выбором из запущенных окон */
void ui_apps_show(ff_app *a);
int ui_apps_get_pos(POINT *p);
void ui_apps_show_at(ff_app *a, int x, int y);

/* окно настроек программы: язык, тема, игры, обновления и отчёт */
void ui_app_show(ff_app *a);
int ui_app_get_pos(POINT *p);
void ui_app_show_at(ff_app *a, int x, int y);

/* диагностика (бета): выбор движка обработки, графики времени этапов и задержек, потребление, прогон по всем движкам */
void ui_bench_show(ff_app *a);

/* положение окна настроек, если оно открыто; 1 или 0 */
int ui_settings_get_pos(POINT *p);
void ui_settings_show_at(ff_app *a, int x, int y);

/* окна и их места на экране между запусками: перед перезапуском после обновления или отката строка с состоянием
 * кладётся в аргумент --windows=, новый процесс открывает те же окна там же. cmdline это строка запуска целиком */
void ui_windows_save(char *out, size_t n);
void ui_windows_restore(ff_app *a, const char *cmdline);

/* язык интерфейса: 0 как в системе, 1 русский, 2 английский; тему применяет ui_theme_apply после theme_set_mode */
void ui_language_apply(int mode);
void ui_theme_apply(void);

/* закрывает окно настроек: пересоздаётся на следующем показе уже с новым языком */
void ui_settings_destroy(void);

/* отчёт и локальная статистика: что именно уйдёт, комментарий и отправка */
void ui_report_show(ff_app *a);
void ui_report_on_result(ff_app *a, ff_report_result *res);

/* выбор версии из релизов репозитория с откатом на старую */
void ui_versions_show(ff_app *a);
void ui_versions_on_list(ff_app *a, ff_release_list *list);

/* диалоги выбора файла профиля. 0 готово, 1 пользователь отказался,
 * -1 файл не открылся или не записался, -2 это не профиль fringefix; name получает имя без папки и .json */
int ui_profile_load_dialog(HWND owner, ff_app *a, wchar_t name[PROFILE_NAME_LEN]);
int ui_profile_save_dialog(HWND owner, ff_app *a, wchar_t name[PROFILE_NAME_LEN]);

/* установка и удаление ReShade-аддона в папке игры, которую выбирает пользователь (reshade_inst.h) */
void ui_reshade_install(HWND owner);
void ui_reshade_remove(HWND owner);

/* ответы фоновых потоков обновления */
void ui_update_on_info(ff_app *a, ff_update_info *info);
void ui_update_on_done(ff_app *a, ff_install_result *res);

#endif
