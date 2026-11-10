#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

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

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#define APP_W 520
#define APP_H 766
#define CARD_ROW 30
#define CARD_INNER_W 488
#define WM_REOPEN (WM_APP + 40)
#define TIMER_LANG 1
#define LANG_DELAY_MS 400

enum {
    ID_A_LANG = 700,
    ID_A_THEME,
    ID_A_PAUSE,
    ID_A_ADDAPP,
    ID_A_CHECK,
    ID_A_BETA,
    ID_A_AUTOSTART,
    ID_A_UPDATE,
    ID_A_VERSIONS,
    ID_A_REPORT,
    ID_A_HK_TOGGLE,
    ID_A_HK_QUIT,
    ID_A_DIAG,
    ID_A_RS_INSTALL,
    ID_A_RS_REMOVE,
    ID_A_RS_EXPORT,
    ID_A_CLOSE
};

static const wchar_t CLASS_NAME[] = L"fringefix_program";
static HWND g_win;
static HWND g_lang;
/* положение окна при пересоздании после смены языка */
static POINT g_pos;
static int g_have_pos;
static HICON g_flags[3];

/* карточки окна сверху вниз по порядку из правок: интерфейс, автопауза, ReShade, сочетания, автозапуск, данные.
 * высота считается по числу строк, верхний край накапливается с зазором CARD_GAP */
typedef struct {
    str_id title;
    int rows;
    /* карточка интеграции с ReShade выделена цветом: это отдельная возможность со своими правилами */
    int accent;
    int y;
    int h;
} card_t;

static card_t CARDS_A[] = {
    { S_CARD_INTERFACE, 2, 0, 0, 0 },
    { S_CARD_AUTOPAUSE, 2, 0, 0, 0 },
    { S_CARD_RESHADE, 3, 1, 0, 0 },
    { S_CARD_HOTKEYS, 2, 0, 0, 0 },
    { S_CARD_AUTOSTART, 1, 0, 0, 0 },
    { S_CARD_DATA, 4, 0, 0, 0 },
};
/* лишняя высота карточки под кнопку диагностики, которая стоит отдельной строкой под рядом кнопок */
#define DATA_EXTRA 6
#define NCARDS_A ((int)(sizeof CARDS_A / sizeof CARDS_A[0]))
enum { C_INTERFACE, C_PAUSE, C_RESHADE, C_HOTKEYS, C_AUTOSTART, C_DATA };

/* режим автопаузы по порядку в списке: значение auto_pause из config.h для каждой строки;
 * первым стоит режим по умолчанию */
static const int PAUSE_VALUE[] = { 2, 1, 3, 0 };
#define NPAUSE ((int)(sizeof PAUSE_VALUE / sizeof PAUSE_VALUE[0]))

typedef struct {
    str_id label;
    int card;
    int row;
} label_t;

static const label_t LABELS[] = {
    { S_LANGUAGE, C_INTERFACE, 0 }, { S_THEME, C_INTERFACE, 1 }, { S_AUTOPAUSE, C_PAUSE, 0 },
    { S_HK_TOGGLE, C_HOTKEYS, 0 }, { S_HK_QUIT, C_HOTKEYS, 1 },
};

static void layout_cards(void)
{
    int y = 16;
    int i;

    for (i = 0; i < NCARDS_A; i++) {
        CARDS_A[i].y = y;
        CARDS_A[i].h = CARD_HEAD + CARDS_A[i].rows * CARD_ROW + CARD_PAD + (i == C_DATA ? DATA_EXTRA : 0);
        y += CARDS_A[i].h + CARD_GAP;
    }
}

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
    /* порядок из правок: полноэкранные и 3D, полноэкранные, только 3D, выключена */
    static const str_id PAUSES[] = { S_AP_3D, S_AP_FULLSCREEN, S_AP_3D_ONLY, S_AP_OFF };
    const ff_config *c = &ui.app->cfg;
    int inner = CARD_INNER_W - 2 * COL_LABEL;
    int bw = (inner - 2 * 8) / 3;
    int by;
    int rw;
    int rb_small = 130;
    int rb_wide = inner - 2 * rb_small - 2 * 8;
    HWND check;
    HWND beta;
    HWND autostart;
    int i;

    /* верхние края карточек нужны всем строкам ниже */
    layout_cards();
    by = row_top(C_DATA, 2) + 1;
    rw = row_top(C_RESHADE, 2) + 1;
    g_lang = combo(C_INTERFACE, 0, ID_A_LANG, 1);
    add_items(g_lang, LANGS, 3, c->language);
    add_items(combo(C_INTERFACE, 1, ID_A_THEME, 0), THEMES, 3, c->theme);
    add_items(combo(C_PAUSE, 0, ID_A_PAUSE, 0), PAUSES, NPAUSE, pause_index(c->auto_pause));
    child(L"BUTTON", ff_tr(S_BTN_APP_LISTS), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + COL_LABEL,
          row_top(C_PAUSE, 1) + 1, inner, BTN_H, ID_A_ADDAPP);

    /* ReShade: пояснение занимает две строки карточки (рисуется в paint), под ним три кнопки */
    child(L"BUTTON", ff_tr(S_BTN_RS_INSTALL), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + COL_LABEL, rw, rb_small, BTN_H,
          ID_A_RS_INSTALL);
    child(L"BUTTON", ff_tr(S_BTN_RS_REMOVE), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + COL_LABEL + rb_small + 8, rw,
          rb_small, BTN_H, ID_A_RS_REMOVE);
    child(L"BUTTON", ff_tr(S_BTN_RS_EXPORT), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + COL_LABEL + 2 * (rb_small + 8), rw,
          rb_wide, BTN_H, ID_A_RS_EXPORT);

    /* поля сочетаний: текст разбирается, когда поле теряет фокус */
    for (i = 0; i < 2; i++) {
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
                        S(MARGIN + COL_SLIDER), S(row_top(C_HOTKEYS, i) + 2), S(160), S(24), g_win,
                        (HMENU)(INT_PTR)(ID_A_HK_TOGGLE + i), ui.app->inst, NULL);
        show_hotkey(ID_A_HK_TOGGLE + i, i == 0 ? &c->hotkey_toggle : &c->hotkey_quit);
    }
    autostart = child(L"BUTTON", ff_tr(S_AUTOSTART_CHECK), BS_AUTOCHECKBOX | WS_TABSTOP,
                      MARGIN + COL_LABEL, row_top(C_AUTOSTART, 0), inner, 24, ID_A_AUTOSTART);
    SendMessageW(autostart, BM_SETCHECK, autostart_get() ? BST_CHECKED : BST_UNCHECKED, 0);

    /* обновления и данные: две галочки, три кнопки в ряд и диагностика */
    check = child(L"BUTTON", ff_tr(S_CHECK_UPDATES), BS_AUTOCHECKBOX | WS_TABSTOP,
                  MARGIN + COL_LABEL, row_top(C_DATA, 0), inner, 24, ID_A_CHECK);
    SendMessageW(check, BM_SETCHECK, c->check_updates ? BST_CHECKED : BST_UNCHECKED, 0);
    beta = child(L"BUTTON", ff_tr(S_BETA_CHECK), BS_AUTOCHECKBOX | WS_TABSTOP,
                 MARGIN + COL_LABEL, row_top(C_DATA, 1), inner, 24, ID_A_BETA);
    SendMessageW(beta, BM_SETCHECK, c->beta ? BST_CHECKED : BST_UNCHECKED, 0);
    child(L"BUTTON", ff_tr(S_MENU_UPDATES), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + COL_LABEL, by, bw, BTN_H,
          ID_A_UPDATE);
    child(L"BUTTON", ff_tr(S_BTN_VERSIONS), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + COL_LABEL + bw + 8, by, bw,
          BTN_H, ID_A_VERSIONS);
    child(L"BUTTON", ff_tr(S_BTN_REPORT), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + COL_LABEL + 2 * (bw + 8), by,
          bw, BTN_H, ID_A_REPORT);
    child(L"BUTTON", ff_tr(S_BTN_DIAG), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + COL_LABEL, row_top(C_DATA, 3) + 1 + DATA_EXTRA,
          inner, BTN_H, ID_A_DIAG);
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
        if (CARDS_A[i].accent) {
            paint_card_accent(dc, ui.dpi, MARGIN, CARDS_A[i].y, CARD_INNER_W, CARDS_A[i].h,
                              ff_tr(CARDS_A[i].title), L"beta");
        } else {
            paint_card(dc, ui.dpi, MARGIN, CARDS_A[i].y, CARD_INNER_W, CARDS_A[i].h, ff_tr(CARDS_A[i].title));
        }
    }
    SelectObject(dc, paint_font());
    SetTextColor(dc, p->fg);
    for (i = 0; i < (int)(sizeof LABELS / sizeof LABELS[0]); i++) {
        const wchar_t *t = ff_tr(LABELS[i].label);

        TextOutW(dc, S(MARGIN + COL_LABEL), S(row_top(LABELS[i].card, LABELS[i].row) + 4), t,
                 (int)wcslen(t));
    }
    /* пояснение карточки ReShade на двух строках над кнопками */
    SetTextColor(dc, p->muted);
    SetRect(&r, S(MARGIN + COL_LABEL + 6), S(row_top(C_RESHADE, 0) + 2), S(MARGIN + CARD_INNER_W - COL_LABEL),
            S(row_top(C_RESHADE, 2) - 2));
    DrawTextW(dc, ff_tr(S_RS_NOTE), -1, &r, DT_WORDBREAK | DT_NOPREFIX);
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
        app_wake(ui.app);
        app_save(ui.app);
    } else if (id == ID_A_CHECK) {
        c->check_updates = SendMessageW(GetDlgItem(g_win, ID_A_CHECK), BM_GETCHECK, 0, 0) == BST_CHECKED;
        app_save(ui.app);
    } else if (id == ID_A_BETA) {
        c->beta = SendMessageW(GetDlgItem(g_win, ID_A_BETA), BM_GETCHECK, 0, 0) == BST_CHECKED;
        app_save(ui.app);
        /* бету выключили, а она стоит: проверка предложит стабильный релиз вместо неё */
        if (!c->beta && ff_version_beta(FF_VERSION) > 0 && FF_REPO[0]) {
            ui_settings_set_status(ff_tr(S_CHECKING));
            update_check_async(ui.app->hwnd, FF_REPO, 1, 0);
        }
    } else if ((id == ID_A_HK_TOGGLE || id == ID_A_HK_QUIT) && code == EN_KILLFOCUS) {
        apply_hotkey(id);
    } else if (id == ID_A_ADDAPP) {
        ui_apps_show(ui.app);
    } else if (id == ID_A_AUTOSTART) {
        set_autostart(SendMessageW(GetDlgItem(g_win, ID_A_AUTOSTART), BM_GETCHECK, 0, 0) == BST_CHECKED);
    } else if (id == ID_A_UPDATE) {
        ui_check_updates(ui.app);
    } else if (id == ID_A_VERSIONS) {
        ui_versions_show(ui.app);
    } else if (id == ID_A_REPORT) {
        ui_report_show(ui.app);
    } else if (id == ID_A_DIAG) {
        ui_bench_show(ui.app);
    } else if (id == ID_A_RS_INSTALL) {
        ui_reshade_install(g_win);
    } else if (id == ID_A_RS_REMOVE) {
        ui_reshade_remove(g_win);
    } else if (id == ID_A_RS_EXPORT) {
        ui_reshade_export(g_win);
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
        /* флажки лежат на карточках: фон цвета карточки, остальное красится как поля */
        if ((HWND)l == GetDlgItem(h, ID_A_CHECK) || (HWND)l == GetDlgItem(h, ID_A_BETA)
            || (HWND)l == GetDlgItem(h, ID_A_AUTOSTART)) {
            return paint_ctlcolor_card((HDC)w);
        }
        return paint_ctlcolor(m, (HDC)w, (HWND)l, (HWND)l);
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
    case WM_DPICHANGED:
        ui_dpi_apply((int)HIWORD(w));
        PostMessageW(h, WM_REOPEN, 0, 0);
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

int ui_app_get_pos(POINT *p)
{
    RECT r;

    if (!g_win || !GetWindowRect(g_win, &r)) {
        p->x = p->y = 0;
        return 0;
    }
    p->x = r.left;
    p->y = r.top;
    return 1;
}

void ui_app_show_at(ff_app *a, int x, int y)
{
    g_pos.x = x;
    g_pos.y = y;
    g_have_pos = 1;
    ui_app_show(a);
}
