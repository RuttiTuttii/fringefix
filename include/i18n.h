#ifndef FRINGEFIX_I18N_H
#define FRINGEFIX_I18N_H

#include <wchar.h>

/* все тексты интерфейса; таблицы русского и английского лежат в i18n.c и сверяются тестом */
typedef enum {
    S_RED, S_GREEN, S_BLUE, S_IMAGE,
    S_HORIZONTAL, S_VERTICAL, S_SHARPEN, S_MONITOR,
    S_HINT, S_CHECK_UPDATES,
    S_BTN_PROFILE_LOAD, S_BTN_PROFILE_SAVE, S_BTN_PATTERN, S_BTN_RESET, S_BTN_UPDATES, S_BTN_SAVE,
    S_WINDOW_TITLE, S_VERSION,
    S_SAVED, S_SAVE_FAILED,
    S_CHECKING, S_NO_REPO, S_UP_TO_DATE, S_UPDATE_UNAVAILABLE, S_UPDATE_FOUND, S_DOWNLOADING,
    S_UPDATE_FAILED, S_RESTART_MANUALLY,
    S_PROFILE_LOADED, S_PROFILE_SAVED, S_PROFILE_BAD, S_PROFILE_SAVE_FAILED,
    S_MENU_SETTINGS, S_MENU_ENABLE, S_MENU_DISABLE, S_MENU_PATTERN, S_MENU_PROFILES,
    S_MENU_NO_PROFILES, S_MENU_UPDATES, S_MENU_QUIT,
    S_TIP_ON, S_TIP_OFF, S_START_FAILED_TITLE, S_START_FAILED,
    S_PATTERN_TEXT,
    S_COUNT
} str_id;

typedef enum { FF_LANG_EN, FF_LANG_RU } ff_lang;

/* выбирает язык, пока не вызвано, работает английский */
void ff_i18n_set(ff_lang lang);

const wchar_t *ff_tr(str_id id);
const wchar_t *ff_tr_lang(ff_lang lang, str_id id);

#endif
