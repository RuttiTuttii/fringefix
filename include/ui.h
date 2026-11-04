#ifndef FRINGEFIX_UI_H
#define FRINGEFIX_UI_H

#include <windows.h>

#include "app.h"
#include "report_send.h"
#include "update.h"

/* скрытое окно сообщений, значок в трее и глобальные хоткеи; 0 или -1 */
int ui_tray_create(ff_app *a);

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

/* окно настроек программы: язык, тема, игры, обновления и отчёт */
void ui_app_show(ff_app *a);

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

/* мастер настройки по типу матрицы: шаги с тестовым паттерном */
void ui_wizard_show(ff_app *a);

/* диалоги выбора файла профиля. 0 готово, 1 пользователь отказался,
 * -1 файл не открылся или не записался, -2 это не профиль fringefix; name получает имя без папки и .json */
int ui_profile_load_dialog(HWND owner, ff_app *a, wchar_t name[PROFILE_NAME_LEN]);
int ui_profile_save_dialog(HWND owner, ff_app *a, wchar_t name[PROFILE_NAME_LEN]);

/* ответы фоновых потоков обновления */
void ui_update_on_info(ff_app *a, ff_update_info *info);
void ui_update_on_done(ff_app *a, ff_install_result *res);

#endif
