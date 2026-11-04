#ifndef FRINGEFIX_UI_H
#define FRINGEFIX_UI_H

#include <windows.h>

#include "app.h"
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

/* для главного цикла: Tab и Enter в окне настроек; ненулевой ответ значит сообщение съедено */
int ui_settings_translate(MSG *m);

/* диалоги выбора файла профиля. 0 готово, 1 пользователь отказался,
 * -1 файл не открылся или не записался, -2 это не профиль fringefix; name получает имя без папки и .json */
int ui_profile_load_dialog(HWND owner, ff_app *a, wchar_t name[PROFILE_NAME_LEN]);
int ui_profile_save_dialog(HWND owner, ff_app *a, wchar_t name[PROFILE_NAME_LEN]);

/* ответы фоновых потоков обновления */
void ui_update_on_info(ff_app *a, ff_update_info *info);
void ui_update_on_done(ff_app *a, ff_install_result *res);

#endif
