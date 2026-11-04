#include "i18n.h"

static ff_lang g_lang = FF_LANG_EN;

static const wchar_t *const RU[S_COUNT] = {
    [S_RED] = L"Красный",
    [S_GREEN] = L"Зелёный",
    [S_BLUE] = L"Синий",
    [S_IMAGE] = L"Изображение",
    [S_HORIZONTAL] = L"По горизонтали",
    [S_VERTICAL] = L"По вертикали",
    [S_SHARPEN] = L"Резкость",
    [S_MONITOR] = L"Монитор",
    [S_HINT] = L"Двойной клик по ползунку сбрасывает его в ноль, значения можно вводить с клавиатуры",
    [S_CHECK_UPDATES] = L"Проверять обновления при запуске",
    [S_BTN_PROFILE_LOAD] = L"Загрузить профиль...",
    [S_BTN_PROFILE_SAVE] = L"Сохранить профиль...",
    [S_BTN_PATTERN] = L"Тестовый паттерн",
    [S_BTN_RESET] = L"Сбросить",
    [S_BTN_UPDATES] = L"Обновления",
    [S_BTN_SAVE] = L"Сохранить",
    [S_WINDOW_TITLE] = L"fringefix: настройки",
    [S_VERSION] = L"Версия %ls",
    [S_SAVED] = L"Настройки сохранены",
    [S_SAVE_FAILED] = L"Не удалось сохранить настройки",
    [S_CHECKING] = L"Проверяю обновления...",
    [S_NO_REPO] = L"В этой сборке не указан репозиторий для обновлений",
    [S_UP_TO_DATE] = L"Установлена последняя версия",
    [S_UPDATE_UNAVAILABLE] = L"Обновления недоступны: %ls",
    [S_UPDATE_FOUND] = L"Доступна версия %ls, сейчас %ls. Скачать и установить?",
    [S_DOWNLOADING] = L"Скачиваю обновление...",
    [S_UPDATE_FAILED] = L"Не удалось обновиться: %ls",
    [S_RESTART_MANUALLY] = L"Обновление установлено, запусти fringefix заново.",
    [S_PROFILE_LOADED] = L"Профиль загружен: %ls",
    [S_PROFILE_SAVED] = L"Профиль сохранён: %ls",
    [S_PROFILE_BAD] = L"Это не профиль fringefix",
    [S_PROFILE_SAVE_FAILED] = L"Не удалось сохранить профиль",
    [S_MENU_SETTINGS] = L"Настройки",
    [S_MENU_ENABLE] = L"Включить\tCtrl+Alt+F",
    [S_MENU_DISABLE] = L"Выключить\tCtrl+Alt+F",
    [S_MENU_PATTERN] = L"Тестовый паттерн",
    [S_MENU_PROFILES] = L"Профили",
    [S_MENU_NO_PROFILES] = L"(нет профилей)",
    [S_MENU_UPDATES] = L"Проверить обновления",
    [S_MENU_QUIT] = L"Выход\tCtrl+Alt+Q",
    [S_TIP_ON] = L"включён",
    [S_TIP_OFF] = L"выключен",
    [S_START_FAILED_TITLE] = L"fringefix не включился",
    [S_START_FAILED] = L"Не удалось запустить fringefix.",
    [S_PATTERN_TEXT] =
        L"fringefix: тестовый паттерн\n\n"
        L"Плоские верх и низ (e f z s t) показывают вертикальную каёмку,\n"
        L"прямые стойки (l i k d) показывают горизонтальную.\n\n"
        L"Esc или клик закрывают окно",
};

static const wchar_t *const EN[S_COUNT] = {
    [S_RED] = L"Red",
    [S_GREEN] = L"Green",
    [S_BLUE] = L"Blue",
    [S_IMAGE] = L"Image",
    [S_HORIZONTAL] = L"Horizontal",
    [S_VERTICAL] = L"Vertical",
    [S_SHARPEN] = L"Sharpness",
    [S_MONITOR] = L"Monitor",
    [S_HINT] = L"Double-click a slider to reset it to zero, values can be typed in",
    [S_CHECK_UPDATES] = L"Check for updates on start",
    [S_BTN_PROFILE_LOAD] = L"Load profile...",
    [S_BTN_PROFILE_SAVE] = L"Save profile...",
    [S_BTN_PATTERN] = L"Test pattern",
    [S_BTN_RESET] = L"Reset",
    [S_BTN_UPDATES] = L"Updates",
    [S_BTN_SAVE] = L"Save",
    [S_WINDOW_TITLE] = L"fringefix settings",
    [S_VERSION] = L"Version %ls",
    [S_SAVED] = L"Settings saved",
    [S_SAVE_FAILED] = L"Could not save settings",
    [S_CHECKING] = L"Checking for updates...",
    [S_NO_REPO] = L"This build has no update repository",
    [S_UP_TO_DATE] = L"You have the latest version",
    [S_UPDATE_UNAVAILABLE] = L"Updates unavailable: %ls",
    [S_UPDATE_FOUND] = L"Version %ls is available, you have %ls. Download and install?",
    [S_DOWNLOADING] = L"Downloading the update...",
    [S_UPDATE_FAILED] = L"Update failed: %ls",
    [S_RESTART_MANUALLY] = L"The update is installed, start fringefix again.",
    [S_PROFILE_LOADED] = L"Profile loaded: %ls",
    [S_PROFILE_SAVED] = L"Profile saved: %ls",
    [S_PROFILE_BAD] = L"This is not a fringefix profile",
    [S_PROFILE_SAVE_FAILED] = L"Could not save the profile",
    [S_MENU_SETTINGS] = L"Settings",
    [S_MENU_ENABLE] = L"Enable\tCtrl+Alt+F",
    [S_MENU_DISABLE] = L"Disable\tCtrl+Alt+F",
    [S_MENU_PATTERN] = L"Test pattern",
    [S_MENU_PROFILES] = L"Profiles",
    [S_MENU_NO_PROFILES] = L"(no profiles)",
    [S_MENU_UPDATES] = L"Check for updates",
    [S_MENU_QUIT] = L"Quit\tCtrl+Alt+Q",
    [S_TIP_ON] = L"on",
    [S_TIP_OFF] = L"off",
    [S_START_FAILED_TITLE] = L"fringefix did not start",
    [S_START_FAILED] = L"fringefix could not start.",
    [S_PATTERN_TEXT] =
        L"fringefix: test pattern\n\n"
        L"Flat tops and bottoms (e f z s t) show vertical fringing,\n"
        L"upright stems (l i k d) show horizontal fringing.\n\n"
        L"Esc or a click closes the window",
};

void ff_i18n_set(ff_lang lang)
{
    g_lang = lang;
}

const wchar_t *ff_tr_lang(ff_lang lang, str_id id)
{
    const wchar_t *s;

    if (id < 0 || id >= S_COUNT) {
        return L"";
    }
    s = lang == FF_LANG_RU ? RU[id] : EN[id];
    return s ? s : L"";
}

const wchar_t *ff_tr(str_id id)
{
    return ff_tr_lang(g_lang, id);
}
