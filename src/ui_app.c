#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "applist.h"
#include "autostart.h"
#include "hotkey.h"
#include "i18n.h"
#include "resource.h"
#include "theme.h"
#include "ui.h"
#include "ui_layout.h"
#include "ui_paint.h"
#include "ui_state.h"
#include "version.h"

#define APP_W 520
#define APP_H 630
#define CARD_ROW 30
#define CARD_INNER_W 488
#define WM_REOPEN (WM_APP + 40)
#define TIMER_LANG 1
#define LANG_DELAY_MS 400

enum {
    ID_A_LANG = 700,
    ID_A_THEME,
    ID_A_PAUSE,
    ID_A_ACTION,
    ID_A_ADDAPP,
    ID_A_CHECK,
    ID_A_BETA,
    ID_A_AUTOSTART,
    ID_A_UPDATE,
    ID_A_VERSIONS,
    ID_A_REPORT,
    ID_A_HK_TOGGLE,
    ID_A_HK_QUIT,
    ID_A_CLOSE
};

static const wchar_t CLASS_NAME[] = L"fringefix_program";
static HWND g_win;
static HWND g_lang;
/* положение окна при пересоздании после смены языка */
static POINT g_pos;
static int g_have_pos;
static HWND g_action;
static HICON g_flags[3];

/* карточки окна: заголовок, верхний край и число строк при 96 dpi */
typedef struct {
    str_id title;
    int y;
    int h;
} card_t;

/* карточки идут сверху вниз с зазором CARD_GAP; автопауза и данные занимают три строки, автозапуск одну */
static const card_t CARDS_A[] = {
    { S_CARD_INTERFACE, 16, CARD_HEAD + 2 * CARD_ROW + CARD_PAD },
    { S_CARD_AUTOPAUSE, 120, CARD_HEAD + 3 * CARD_ROW + CARD_PAD },
    { S_CARD_DATA, 254, CARD_HEAD + 3 * CARD_ROW + CARD_PAD },
    { S_CARD_AUTOSTART, 388, CARD_HEAD + 1 * CARD_ROW + CARD_PAD },
    { S_CARD_HOTKEYS, 462, CARD_HEAD + 2 * CARD_ROW + CARD_PAD },
};
#define NCARDS_A ((int)(sizeof CARDS_A / sizeof CARDS_A[0]))

/* режим автопаузы по порядку в списке: значение auto_pause из config.h для каждой строки */
static const int PAUSE_VALUE[] = { 1, 3, 2, 0 };
#define NPAUSE ((int)(sizeof PAUSE_VALUE / sizeof PAUSE_VALUE[0]))

typedef struct {
    str_id label;
    int card;
    int row;
} label_t;

static const label_t LABELS[] = {
    { S_LANGUAGE, 0, 0 }, { S_THEME, 0, 1 }, { S_AUTOPAUSE, 1, 0 }, { S_GAMEACTION, 1, 1 },
    { S_HK_TOGGLE, 4, 0 }, { S_HK_QUIT, 4, 1 },
};

static HWND child(const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h,
                  int id)
{
    return CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h),
                           g_win, (HMENU)(INT_PTR)id, ui.app->inst, NULL);
}

static int row_top(int card, int row)
{
    return CARDS_A[card].y + CARD_HEAD + row * CARD_ROW;
}

static HWND combo(int card, int row, int id, int owner_draw)
{
    return child(L"COMBOBOX", L"", (owner_draw ? CBS_OWNERDRAWFIXED | CBS_HASSTRINGS : 0)
                 | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
                 MARGIN + COL_SLIDER, row_top(card, row) + 1, CARD_INNER_W - COL_SLIDER - COL_LABEL,
                 200, id);
}

static int pause_index(int value)
{
    int i;

    for (i = 0; i < NPAUSE; i++) {
        if (PAUSE_VALUE[i] == value) {
            return i;
        }
    }
    return 0;
}

/* действие в играх выбирается только при выключенной автопаузе, при включённой стоит заблокированная пауза */
static void sync_action(void)
{
    const ff_config *c = &ui.app->cfg;
    int free_choice = c->auto_pause == 0;

    SendMessageW(g_action, CB_SETCURSEL, (WPARAM)(free_choice ? c->game_action : 0), 0);
    EnableWindow(g_action, free_choice);
}

static void add_items(HWND c, const str_id *ids, int n, int selected)
{
    int i;

    for (i = 0; i < n; i++) {
        SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)ff_tr(ids[i]));
    }
    SendMessageW(c, CB_SETCURSEL, (WPARAM)selected, 0);
}

/* сочетание в поле в виде "Ctrl+Alt+F10" */
static void show_hotkey(int id, const ff_hotkey *h)
{
    char utf8[32];
    wchar_t text[32];

    if (ff_hotkey_format(h, utf8, sizeof utf8) < 0) {
        utf8[0] = '\0';
    }
    ui_utf8(utf8, text, 32);
    SetWindowTextW(GetDlgItem(g_win, id), text);
}

/* текст поля становится сочетанием: непонятное или занятое возвращается к прежнему с пояснением */
static void apply_hotkey(int id)
{
    ff_config *c = &ui.app->cfg;
    ff_hotkey *slot = id == ID_A_HK_TOGGLE ? &c->hotkey_toggle : &c->hotkey_quit;
    ff_hotkey old = *slot;
    ff_hotkey h;
    wchar_t text[32];
    char utf8[96];

    GetWindowTextW(GetDlgItem(g_win, id), text, 32);
    if (WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, sizeof utf8, NULL, NULL) == 0
        || ff_hotkey_parse(utf8, &h) != 0) {
        show_hotkey(id, slot);
        MessageBoxW(g_win, ff_tr(S_HK_BAD), L"fringefix", MB_ICONWARNING | MB_TOPMOST);
        return;
    }
    if (h.vk != old.vk || h.mods != old.mods) {
        *slot = h;
        /* бит 0 это сочетание включения, бит 1 выход: чужая неудача не мешает */
        if (ui_tray_bind_hotkeys(ui.app) & (id == ID_A_HK_TOGGLE ? 1 : 2)) {
            *slot = old;
            ui_tray_bind_hotkeys(ui.app);
            show_hotkey(id, slot);
            MessageBoxW(g_win, ff_tr(S_HK_BUSY), L"fringefix", MB_ICONWARNING | MB_TOPMOST);
            return;
        }
        app_save(ui.app);
    }
    show_hotkey(id, slot);
}

static void build(void)
{
    static const str_id LANGS[] = { S_LANG_AUTO, S_LANG_RU, S_LANG_EN };
    static const str_id THEMES[] = { S_THEME_AUTO, S_THEME_LIGHT, S_THEME_DARK };
    static const str_id PAUSES[] = { S_AP_FULLSCREEN, S_AP_3D_ONLY, S_AP_3D, S_AP_OFF };
    static const str_id ACTIONS[] = { S_GA_PAUSE, S_GA_LIGHT };
    const ff_config *c = &ui.app->cfg;
    int bw = (CARD_INNER_W - 2 * 16 - 2 * 8) / 3;
    int by = row_top(2, 2) + 1;
    HWND check;
    HWND beta;
    HWND autostart;
    int i;

    g_lang = combo(0, 0, ID_A_LANG, 1);
    add_items(g_lang, LANGS, 3, c->language);
    add_items(combo(0, 1, ID_A_THEME, 0), THEMES, 3, c->theme);
    add_items(combo(1, 0, ID_A_PAUSE, 0), PAUSES, NPAUSE, pause_index(c->auto_pause));
    g_action = combo(1, 1, ID_A_ACTION, 0);
    add_items(g_action, ACTIONS, 2, 0);
    sync_action();
    child(L"BUTTON", ff_tr(S_BTN_ADD_APP), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + 16,
          row_top(1, 2) + 1, CARD_INNER_W - 32, BTN_H, ID_A_ADDAPP);
    check = child(L"BUTTON", ff_tr(S_CHECK_UPDATES), BS_AUTOCHECKBOX | WS_TABSTOP,
                  MARGIN + COL_LABEL, row_top(2, 0), CARD_INNER_W - 2 * COL_LABEL, 24, ID_A_CHECK);
    SendMessageW(check, BM_SETCHECK, c->check_updates ? BST_CHECKED : BST_UNCHECKED, 0);
    beta = child(L"BUTTON", ff_tr(S_BETA_CHECK), BS_AUTOCHECKBOX | WS_TABSTOP,
                 MARGIN + COL_LABEL, row_top(2, 1), CARD_INNER_W - 2 * COL_LABEL, 24, ID_A_BETA);
    SendMessageW(beta, BM_SETCHECK, c->beta ? BST_CHECKED : BST_UNCHECKED, 0);
    child(L"BUTTON", ff_tr(S_MENU_UPDATES), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + 16, by, bw, BTN_H,
          ID_A_UPDATE);
    child(L"BUTTON", ff_tr(S_BTN_VERSIONS), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + 16 + bw + 8, by, bw,
          BTN_H, ID_A_VERSIONS);
    child(L"BUTTON", ff_tr(S_BTN_REPORT), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + 16 + 2 * (bw + 8), by,
          bw, BTN_H, ID_A_REPORT);
    autostart = child(L"BUTTON", ff_tr(S_AUTOSTART_CHECK), BS_AUTOCHECKBOX | WS_TABSTOP,
                      MARGIN + COL_LABEL, row_top(3, 0), CARD_INNER_W - 2 * COL_LABEL, 24,
                      ID_A_AUTOSTART);
    SendMessageW(autostart, BM_SETCHECK, autostart_get() ? BST_CHECKED : BST_UNCHECKED, 0);
    /* поля сочетаний: текст разбирается, когда поле теряет фокус */
    for (i = 0; i < 2; i++) {
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
                        S(MARGIN + COL_SLIDER), S(row_top(4, i) + 2), S(160), S(24), g_win,
                        (HMENU)(INT_PTR)(ID_A_HK_TOGGLE + i), ui.app->inst, NULL);
        show_hotkey(ID_A_HK_TOGGLE + i, i == 0 ? &c->hotkey_toggle : &c->hotkey_quit);
    }
    child(L"BUTTON", ff_tr(S_VER_CLOSE), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + CARD_INNER_W - 120,
          CARDS_A[NCARDS_A - 1].y + CARDS_A[NCARDS_A - 1].h + 16, 120, BTN_H, ID_A_CLOSE);
    ctl_restyle_window(g_win);
}

static void paint(HWND h)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    const ff_palette *p = theme_get();
    RECT r;
    int i;

    GetClientRect(h, &r);
    FillRect(dc, &r, paint_bg());
    SetBkMode(dc, TRANSPARENT);
    for (i = 0; i < NCARDS_A; i++) {
        paint_card(dc, ui.dpi, MARGIN, CARDS_A[i].y, CARD_INNER_W, CARDS_A[i].h,
                   ff_tr(CARDS_A[i].title));
    }
    SelectObject(dc, paint_font());
    SetTextColor(dc, p->fg);
    for (i = 0; i < (int)(sizeof LABELS / sizeof LABELS[0]); i++) {
        const wchar_t *t = ff_tr(LABELS[i].label);

        TextOutW(dc, S(MARGIN + COL_LABEL), S(row_top(LABELS[i].card, LABELS[i].row) + 4), t,
                 (int)wcslen(t));
    }
    EndPaint(h, &ps);
}

/* смена языка пересоздаёт и это окно, и окно настроек, чтобы все строки встали заново;
 * окно настроек, если оно было открыто, показывается снова на прежнем месте */
static void change_language(int mode)
{
    int settings_open = ui.win && IsWindowVisible(ui.win);

    ui.app->cfg.language = mode;
    ui_language_apply(mode);
    app_save(ui.app);
    PostMessageW(ui.app->hwnd, WM_APP_STATE, 0, 0);
    ui_settings_destroy();
    if (settings_open) {
        ui_settings_show(ui.app);
    }
    PostMessageW(g_win, WM_REOPEN, 0, 0);
}

/* выбор в списке языков применяется, когда пользователь остановился и список закрыт: стрелка или колесо
 * над закрытым списком шлют CBN_SELCHANGE на каждый шаг, а пересоздание окон отнимает у списка фокус */
static void apply_language_choice(void)
{
    int sel = (int)SendMessageW(g_lang, CB_GETCURSEL, 0, 0);

    KillTimer(g_win, TIMER_LANG);
    if (SendMessageW(g_lang, CB_GETDROPPEDSTATE, 0, 0)) {
        return;
    }
    if (sel != CB_ERR && sel != ui.app->cfg.language) {
        change_language(sel);
    }
}

/* запись в реестр не удалась: галочка возвращается к тому, что видит Windows */
static void set_autostart(int on)
{
    if (autostart_set(on) != 0) {
        MessageBoxW(g_win, ff_tr(S_AUTOSTART_FAILED), L"fringefix", MB_ICONERROR | MB_TOPMOST);
        SendMessageW(GetDlgItem(g_win, ID_A_AUTOSTART), BM_SETCHECK,
                     autostart_get() ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

/* окно с текстом, в который подставлено имя файла */
static int say(const str_id fmt, const wchar_t *name, UINT flags)
{
    wchar_t text[MAX_PATH + 128];

    _snwprintf(text, sizeof text / sizeof text[0], ff_tr(fmt), name);
    text[sizeof text / sizeof text[0] - 1] = L'\0';
    return MessageBoxW(g_win, text, L"fringefix", flags | MB_TOPMOST);
}

/* выбор exe в список обязательного применения (ignore_apps: на паузу никогда, что бы ни стояло в автопаузе);
 * уже стоящее в списке предлагается убрать, так список правится одной кнопкой.
 * shortcut: строка списка правится на месте, а поток захвата копирует настройки целиком, поэтому
 * в худшем случае одна проверка за полсекунды увидит список наполовину записанным */
static void edit_force_list(void)
{
    OPENFILENAMEW o;
    wchar_t path[MAX_PATH] = L"";
    const wchar_t *name;
    ff_config *c = &ui.app->cfg;

    memset(&o, 0, sizeof o);
    o.lStructSize = sizeof o;
    o.hwndOwner = g_win;
    o.lpstrFilter = L"EXE (*.exe)\0*.exe\0";
    o.lpstrFile = path;
    o.nMaxFile = MAX_PATH;
    o.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&o)) {
        return;
    }
    /* имя без папки для сообщений; в список и поиск уходит как есть, они сами берут имя из пути */
    name = wcsrchr(path, L'\\') ? wcsrchr(path, L'\\') + 1 : path;
    if (ff_app_list_has(c->ignore_apps, path)) {
        if (say(S_FORCE_ASK_REMOVE, name, MB_YESNO | MB_ICONQUESTION) == IDYES) {
            ff_app_list_remove(c->ignore_apps, sizeof c->ignore_apps, path);
            say(S_FORCE_REMOVED, name, MB_ICONINFORMATION);
        }
    } else if (ff_app_list_add(c->ignore_apps, sizeof c->ignore_apps, path) < 0) {
        say(S_FORCE_FAILED, name, MB_ICONWARNING);
        return;
    } else {
        say(S_FORCE_ADDED, name, MB_ICONINFORMATION);
    }
    app_wake(ui.app);
    app_save(ui.app);
}

static void on_command(int id, int code)
{
    ff_config *c = &ui.app->cfg;
    int sel = 0;

    if (code == CBN_SELCHANGE) {
        sel = (int)SendMessageW((HWND)GetDlgItem(g_win, id), CB_GETCURSEL, 0, 0);
    }
    if (id == ID_A_LANG) {
        if (code == CBN_SELCHANGE || code == CBN_CLOSEUP) {
            SetTimer(g_win, TIMER_LANG, LANG_DELAY_MS, NULL);
        }
    } else if (id == ID_A_THEME && code == CBN_SELCHANGE) {
        c->theme = sel;
        theme_set_mode(sel);
        ui_theme_apply();
        app_save(ui.app);
    } else if (id == ID_A_PAUSE && code == CBN_SELCHANGE) {
        c->auto_pause = PAUSE_VALUE[sel];
        sync_action();
        app_wake(ui.app);
        app_save(ui.app);
    } else if (id == ID_A_ACTION && code == CBN_SELCHANGE) {
        c->game_action = sel;
        app_wake(ui.app);
        app_save(ui.app);
    } else if (id == ID_A_CHECK) {
        c->check_updates = SendMessageW(GetDlgItem(g_win, ID_A_CHECK), BM_GETCHECK, 0, 0) == BST_CHECKED;
        app_save(ui.app);
    } else if (id == ID_A_BETA) {
        c->beta = SendMessageW(GetDlgItem(g_win, ID_A_BETA), BM_GETCHECK, 0, 0) == BST_CHECKED;
        app_save(ui.app);
    } else if ((id == ID_A_HK_TOGGLE || id == ID_A_HK_QUIT) && code == EN_KILLFOCUS) {
        apply_hotkey(id);
    } else if (id == ID_A_ADDAPP) {
        edit_force_list();
    } else if (id == ID_A_AUTOSTART) {
        set_autostart(SendMessageW(GetDlgItem(g_win, ID_A_AUTOSTART), BM_GETCHECK, 0, 0) == BST_CHECKED);
    } else if (id == ID_A_UPDATE && FF_REPO[0]) {
        ui_settings_set_status(ff_tr(S_CHECKING));
        update_check_async(ui.app->hwnd, FF_REPO, 1, c->beta);
    } else if (id == ID_A_UPDATE) {
        MessageBoxW(g_win, ff_tr(S_NO_REPO), L"fringefix", MB_ICONINFORMATION | MB_TOPMOST);
    } else if (id == ID_A_VERSIONS) {
        ui_versions_show(ui.app);
    } else if (id == ID_A_REPORT) {
        ui_report_show(ui.app);
    } else if (id == ID_A_CLOSE) {
        DestroyWindow(g_win);
    }
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT:
        paint(h);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_COMMAND:
        on_command(LOWORD(w), HIWORD(w));
        return 0;
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT *mi = (MEASUREITEMSTRUCT *)l;

        mi->itemHeight = (UINT)S(22);
        return TRUE;
    }
    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT *d = (const DRAWITEMSTRUCT *)l;
        wchar_t text[64];

        if (d->itemID == (UINT)-1 || d->itemID > 2) {
            return TRUE;
        }
        SendMessageW(d->hwndItem, CB_GETLBTEXT, d->itemID, (LPARAM)text);
        paint_draw_item(d, g_flags[d->itemID], text, ui.dpi);
        return TRUE;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
        /* списки окна красятся как поля, флажок и кнопки как фон */
        return paint_ctlcolor(m, (HDC)w, (HWND)l,
                              (HWND)l != GetDlgItem(h, ID_A_CHECK)
                                      && (HWND)l != GetDlgItem(h, ID_A_BETA)
                                      && (HWND)l != GetDlgItem(h, ID_A_AUTOSTART) ? (HWND)l : NULL);
    case WM_SETTINGCHANGE:
        if (l && wcscmp((const wchar_t *)l, L"ImmersiveColorSet") == 0) {
            ui_theme_apply();
        }
        return 0;
    case WM_TIMER:
        if (w == TIMER_LANG) {
            apply_language_choice();
        }
        return 0;
    case WM_REOPEN: {
        RECT r;

        GetWindowRect(h, &r);
        g_pos.x = r.left;
        g_pos.y = r.top;
        g_have_pos = 1;
        DestroyWindow(h);
        ui_app_show(ui.app);
        return 0;
    }
    case WM_CLOSE:
        /* закрытие крестиком не отнимает фокус у поля, поэтому правка сочетания принимается здесь */
        apply_hotkey(ID_A_HK_TOGGLE);
        apply_hotkey(ID_A_HK_QUIT);
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        g_win = NULL;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void load_flags(void)
{
    static const int IDS[3] = { IDI_FLAG_AUTO, IDI_FLAG_RU, IDI_FLAG_EN };
    int i;

    for (i = 0; i < 3; i++) {
        if (g_flags[i]) {
            DestroyIcon(g_flags[i]);
        }
        g_flags[i] = (HICON)LoadImageW(ui.app->inst, MAKEINTRESOURCEW(IDS[i]), IMAGE_ICON, S(16),
                                       S(16), 0);
    }
}

void ui_app_show(ff_app *a)
{
    WNDCLASSW wc;
    DWORD style = WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    RECT r;

    ui.app = a;
    if (g_win) {
        SetForegroundWindow(g_win);
        return;
    }
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = a->inst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    RegisterClassW(&wc);

    load_flags();
    SetRect(&r, 0, 0, S(APP_W), S(APP_H));
    AdjustWindowRect(&r, style, FALSE);
    g_win = CreateWindowExW(WS_EX_TOPMOST | WS_EX_CONTROLPARENT, CLASS_NAME, ff_tr(S_APP_TITLE),
                            style, g_have_pos ? g_pos.x : CW_USEDEFAULT,
                            g_have_pos ? g_pos.y : CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                            NULL, NULL, a->inst, NULL);
    g_have_pos = 0;
    SendMessageW(g_win, WM_SETICON, ICON_BIG,
                 (LPARAM)LoadImageW(a->inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));
    SendMessageW(g_win, WM_SETICON, ICON_SMALL,
                 (LPARAM)LoadImageW(a->inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
    build();
    ShowWindow(g_win, SW_SHOW);
    SetForegroundWindow(g_win);
}
