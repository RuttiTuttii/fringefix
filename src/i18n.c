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
    [S_AUTOPAUSE] = L"Автопауза",
    [S_AP_OFF] = L"Выключена",
    [S_AP_FULLSCREEN] = L"В полноэкранных приложениях",
    [S_AP_3D] = L"В полноэкранных и 3D-приложениях",
    [S_TIP_PAUSED] = L"пауза, идёт игра или полный экран",
    [S_BTN_WIZARD] = L"Мастер настройки...",
    [S_WIZ_TITLE] = L"fringefix: мастер настройки",
    [S_WIZ_STEP] = L"Шаг %d из 3",
    [S_WIZ_T0] = L"Тип матрицы",
    [S_WIZ_T1] = L"Вертикальная каёмка",
    [S_WIZ_T2] = L"Горизонтальная каёмка",
    [S_WIZ_B0] = L"Выбери тип матрицы монитора. Для QD-OLED с треугольными субпикселями мастер сам "
                 L"посчитает сдвиги по геометрии: красный и синий стоят рядом снизу, зелёный над ними. "
                 L"Дальше останется подогнать две силы, по вертикали и по горизонтали. "
                 L"Для обычных RGB-полосок коррекция не нужна.",
    [S_WIZ_B1] = L"Нажми «Тестовый паттерн» и смотри на горизонтальные линии в правой половине. "
                 L"Двигай силу по вертикали, пока верхний и нижний края линий не станут серыми. "
                 L"Если каёмка стала сильнее, двигай в другую сторону: минус зеркалит раскладку.",
    [S_WIZ_B2] = L"Теперь смотри на вертикальные линии в левой половине и двигай силу по горизонтали, "
                 L"пока их края не станут серыми. Потом нажми «Готово»: значения попадут в окно настроек, "
                 L"там их можно уточнить вручную.",
    [S_WIZ_TYPE_QD] = L"QD-OLED, треугольные субпиксели (поколения 1-4)",
    [S_WIZ_TYPE_NONE] = L"RGB-полоски (LCD, QD-OLED 5 поколения): коррекция не нужна",
    [S_WIZ_STRENGTH_V] = L"Сила по вертикали",
    [S_WIZ_STRENGTH_H] = L"Сила по горизонтали",
    [S_WIZ_BACK] = L"Назад",
    [S_WIZ_NEXT] = L"Далее",
    [S_WIZ_FINISH] = L"Готово",
    [S_WIZ_CANCEL] = L"Отмена",
    [S_GAMEACTION] = L"Действие в играх",
    [S_GA_PAUSE] = L"Пауза",
    [S_GA_LIGHT] = L"Облегчённый режим",
    [S_BTN_VERSIONS] = L"Версии...",
    [S_VER_TITLE] = L"fringefix: версии",
    [S_VER_HINT] = L"Выбери версию из списка: новее или старее текущей. Откат на старую ставится так же, настройки сохраняются.",
    [S_VER_LOADING] = L"Загружаю список версий...",
    [S_VER_FAILED] = L"Не удалось получить список: %ls",
    [S_VER_LATEST] = L"  (последняя)",
    [S_VER_CURRENT] = L"  (установлена)",
    [S_VER_INSTALL] = L"Установить",
    [S_VER_ROLLBACK] = L"Откатить",
    [S_VER_SAME] = L"Установлена",
    [S_VER_CLOSE] = L"Закрыть",
    [S_VER_CONFIRM_UP] = L"Установить версию %ls? Сейчас %ls. Программа перезапустится.",
    [S_VER_CONFIRM_DOWN] = L"Откатиться на версию %ls? Сейчас %ls. Программа перезапустится.",
    [S_BTN_PROGRAM] = L"Программа...",
    [S_APP_TITLE] = L"fringefix: программа",
    [S_CARD_INTERFACE] = L"Интерфейс",
    [S_CARD_GAMES] = L"Игры",
    [S_CARD_DATA] = L"Обновления и данные",
    [S_LANGUAGE] = L"Язык",
    [S_LANG_AUTO] = L"Авто (как в системе)",
    [S_LANG_RU] = L"Русский",
    [S_LANG_EN] = L"English",
    [S_THEME] = L"Тема",
    [S_THEME_AUTO] = L"Авто (как в системе)",
    [S_THEME_LIGHT] = L"Светлая",
    [S_THEME_DARK] = L"Тёмная",
    [S_BTN_REPORT] = L"Отчёт и статистика...",
    [S_REP_TITLE] = L"fringefix: отчёт и статистика",
    [S_REP_HINT] = L"Ничего не отправляется само. Внизу показано ровно то, что уйдёт на сервер, и ты решаешь, отправлять или нет. "
                   L"В отчёте нет имени пользователя, имени компьютера, путей, заголовков окон и постоянного идентификатора. "
                   L"Сервер видит сетевой адрес, но не записывает его.",
    [S_REP_COLLECT] = L"Вести статистику на этом компьютере (хранится только здесь)",
    [S_REP_SYSTEM] = L"Система: версия Windows, видеокарта, разрешение монитора, HDR",
    [S_REP_SETTINGS] = L"Мои настройки: сдвиги, резкость, автопауза",
    [S_REP_STATS] = L"Статистика: счётчики, ошибки и игры (только имена exe)",
    [S_REP_COMMENT] = L"Комментарий, по желанию",
    [S_REP_PREVIEW] = L"Что будет отправлено (точно этот текст)",
    [S_REP_SEND] = L"Отправить",
    [S_REP_ISSUE] = L"Открыть issue на GitHub",
    [S_REP_COPY] = L"Скопировать",
    [S_REP_SAVE] = L"Сохранить в файл...",
    [S_REP_CLEAR] = L"Очистить статистику",
    [S_REP_SENDING] = L"Отправляю...",
    [S_REP_SENT] = L"Отправлено, номер %ls. Спасибо",
    [S_REP_SEND_FAILED] = L"Не отправилось: %ls",
    [S_REP_NO_URL] = L"В этой сборке нет адреса приёма: отчёт можно скопировать, сохранить в файл или открыть issue",
    [S_REP_COPIED] = L"Скопировано в буфер обмена",
    [S_REP_SAVED] = L"Сохранено в файл",
    [S_REP_CLEARED] = L"Статистика очищена",
    [S_REP_ISSUE_BODY] = L"Вставь сюда отчёт (Ctrl+V), он уже в буфере обмена.",
    [S_REP_CONFIRM] = L"Отправить отчёт на сервер? Уйдёт ровно тот текст, что показан в окне.",
    [S_REP_TOO_BIG] = L"Отчёт слишком длинный, сократи комментарий",
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
    [S_AUTOPAUSE] = L"Auto-pause",
    [S_AP_OFF] = L"Off",
    [S_AP_FULLSCREEN] = L"In fullscreen apps",
    [S_AP_3D] = L"In fullscreen and 3D apps",
    [S_TIP_PAUSED] = L"paused, a game or fullscreen app is running",
    [S_BTN_WIZARD] = L"Setup wizard...",
    [S_WIZ_TITLE] = L"fringefix setup wizard",
    [S_WIZ_STEP] = L"Step %d of 3",
    [S_WIZ_T0] = L"Panel type",
    [S_WIZ_T1] = L"Vertical fringing",
    [S_WIZ_T2] = L"Horizontal fringing",
    [S_WIZ_B0] = L"Choose the panel type. For QD-OLED with triangular subpixels the wizard computes the "
                 L"shifts from the geometry: red and blue sit side by side at the bottom, green above them. "
                 L"What is left is to fit two strengths, vertical and horizontal. "
                 L"Plain RGB stripes need no correction.",
    [S_WIZ_B1] = L"Press the test pattern button and look at the horizontal lines in the right half. "
                 L"Move the vertical strength until the top and bottom edges of the lines turn grey. "
                 L"If the fringing got stronger, move the other way: a minus mirrors the layout.",
    [S_WIZ_B2] = L"Now look at the vertical lines in the left half and move the horizontal strength "
                 L"until their edges turn grey. Then press Finish: the values land in the settings "
                 L"window where they can be refined by hand.",
    [S_WIZ_TYPE_QD] = L"QD-OLED, triangular subpixels (gen 1-4)",
    [S_WIZ_TYPE_NONE] = L"RGB stripes (LCD, 5th-gen QD-OLED): no correction needed",
    [S_WIZ_STRENGTH_V] = L"Vertical strength",
    [S_WIZ_STRENGTH_H] = L"Horizontal strength",
    [S_WIZ_BACK] = L"Back",
    [S_WIZ_NEXT] = L"Next",
    [S_WIZ_FINISH] = L"Finish",
    [S_WIZ_CANCEL] = L"Cancel",
    [S_GAMEACTION] = L"Action in games",
    [S_GA_PAUSE] = L"Pause",
    [S_GA_LIGHT] = L"Light mode",
    [S_BTN_VERSIONS] = L"Versions...",
    [S_VER_TITLE] = L"fringefix versions",
    [S_VER_HINT] = L"Pick a version from the list: newer or older than the current one. A rollback installs the same way, settings are kept.",
    [S_VER_LOADING] = L"Loading the version list...",
    [S_VER_FAILED] = L"Could not get the list: %ls",
    [S_VER_LATEST] = L"  (latest)",
    [S_VER_CURRENT] = L"  (installed)",
    [S_VER_INSTALL] = L"Install",
    [S_VER_ROLLBACK] = L"Roll back",
    [S_VER_SAME] = L"Installed",
    [S_VER_CLOSE] = L"Close",
    [S_VER_CONFIRM_UP] = L"Install version %ls? You have %ls. The program will restart.",
    [S_VER_CONFIRM_DOWN] = L"Roll back to version %ls? You have %ls. The program will restart.",
    [S_BTN_PROGRAM] = L"Program...",
    [S_APP_TITLE] = L"fringefix program settings",
    [S_CARD_INTERFACE] = L"Interface",
    [S_CARD_GAMES] = L"Games",
    [S_CARD_DATA] = L"Updates and data",
    [S_LANGUAGE] = L"Language",
    [S_LANG_AUTO] = L"Auto (as in the system)",
    [S_LANG_RU] = L"Русский",
    [S_LANG_EN] = L"English",
    [S_THEME] = L"Theme",
    [S_THEME_AUTO] = L"Auto (as in the system)",
    [S_THEME_LIGHT] = L"Light",
    [S_THEME_DARK] = L"Dark",
    [S_BTN_REPORT] = L"Report and statistics...",
    [S_REP_TITLE] = L"fringefix: report and statistics",
    [S_REP_HINT] = L"Nothing is sent on its own. Below is exactly what would go to the server, and you decide whether to send it. "
                   L"The report has no user name, computer name, paths, window titles or permanent identifier. "
                   L"The server sees the network address but does not record it.",
    [S_REP_COLLECT] = L"Keep statistics on this computer (stored only here)",
    [S_REP_SYSTEM] = L"System: Windows version, GPU, monitor resolution, HDR",
    [S_REP_SETTINGS] = L"My settings: shifts, sharpness, auto-pause",
    [S_REP_STATS] = L"Statistics: counters, errors and games (exe names only)",
    [S_REP_COMMENT] = L"Comment, optional",
    [S_REP_PREVIEW] = L"What will be sent (exactly this text)",
    [S_REP_SEND] = L"Send",
    [S_REP_ISSUE] = L"Open a GitHub issue",
    [S_REP_COPY] = L"Copy",
    [S_REP_SAVE] = L"Save to file...",
    [S_REP_CLEAR] = L"Clear statistics",
    [S_REP_SENDING] = L"Sending...",
    [S_REP_SENT] = L"Sent, number %ls. Thank you",
    [S_REP_SEND_FAILED] = L"Not sent: %ls",
    [S_REP_NO_URL] = L"This build has no receiving address: the report can be copied, saved to a file or opened as an issue",
    [S_REP_COPIED] = L"Copied to the clipboard",
    [S_REP_SAVED] = L"Saved to a file",
    [S_REP_CLEARED] = L"Statistics cleared",
    [S_REP_ISSUE_BODY] = L"Paste the report here (Ctrl+V), it is already in the clipboard.",
    [S_REP_CONFIRM] = L"Send the report to the server? Exactly the text shown in the window goes out.",
    [S_REP_TOO_BIG] = L"The report is too long, shorten the comment",
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

ff_lang ff_i18n_get(void)
{
    return g_lang;
}

const wchar_t *ff_tr(str_id id)
{
    return ff_tr_lang(g_lang, id);
}
