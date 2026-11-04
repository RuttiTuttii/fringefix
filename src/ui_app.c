#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "i18n.h"
#include "resource.h"
#include "theme.h"
#include "ui.h"
#include "ui_layout.h"
#include "ui_paint.h"
#include "ui_state.h"
#include "version.h"

#define APP_W 520
#define APP_H 392
#define CARD_ROW 30
#define CARD_INNER_W 488
#define WM_REOPEN (WM_APP + 40)

enum {
    ID_A_LANG = 700,
    ID_A_THEME,
    ID_A_PAUSE,
    ID_A_ACTION,
    ID_A_CHECK,
    ID_A_UPDATE,
    ID_A_VERSIONS,
    ID_A_REPORT,
    ID_A_CLOSE
};

static const wchar_t CLASS_NAME[] = L"fringefix_program";
static HWND g_win;
static HWND g_lang;
static HICON g_flags[3];

/* карточки окна: заголовок, верхний край и число строк при 96 dpi */
typedef struct {
    str_id title;
    int y;
    int h;
} card_t;

static const card_t CARDS_A[] = {
    { S_CARD_INTERFACE, 16, CARD_HEAD + 2 * CARD_ROW + CARD_PAD },
    { S_CARD_GAMES, 120, CARD_HEAD + 2 * CARD_ROW + CARD_PAD },
    { S_CARD_DATA, 224, CARD_HEAD + 2 * CARD_ROW + CARD_PAD },
};

typedef struct {
    str_id label;
    int card;
    int row;
} label_t;

static const label_t LABELS[] = {
    { S_LANGUAGE, 0, 0 }, { S_THEME, 0, 1 }, { S_AUTOPAUSE, 1, 0 }, { S_GAMEACTION, 1, 1 },
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

static void add_items(HWND c, const str_id *ids, int n, int selected)
{
    int i;

    for (i = 0; i < n; i++) {
        SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)ff_tr(ids[i]));
    }
    SendMessageW(c, CB_SETCURSEL, (WPARAM)selected, 0);
}

static void build(void)
{
    static const str_id LANGS[] = { S_LANG_AUTO, S_LANG_RU, S_LANG_EN };
    static const str_id THEMES[] = { S_THEME_AUTO, S_THEME_LIGHT, S_THEME_DARK };
    static const str_id PAUSES[] = { S_AP_OFF, S_AP_FULLSCREEN, S_AP_3D };
    static const str_id ACTIONS[] = { S_GA_PAUSE, S_GA_LIGHT };
    const ff_config *c = &ui.app->cfg;
    int bw = (CARD_INNER_W - 2 * 16 - 2 * 8) / 3;
    int by = row_top(2, 1) + 1;
    HWND check;

    g_lang = combo(0, 0, ID_A_LANG, 1);
    add_items(g_lang, LANGS, 3, c->language);
    add_items(combo(0, 1, ID_A_THEME, 0), THEMES, 3, c->theme);
    add_items(combo(1, 0, ID_A_PAUSE, 0), PAUSES, 3, c->auto_pause);
    add_items(combo(1, 1, ID_A_ACTION, 0), ACTIONS, 2, c->game_action);
    check = child(L"BUTTON", ff_tr(S_CHECK_UPDATES), BS_AUTOCHECKBOX | WS_TABSTOP,
                  MARGIN + COL_LABEL, row_top(2, 0), CARD_INNER_W - 2 * COL_LABEL, 24, ID_A_CHECK);
    SendMessageW(check, BM_SETCHECK, c->check_updates ? BST_CHECKED : BST_UNCHECKED, 0);
    child(L"BUTTON", ff_tr(S_MENU_UPDATES), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + 16, by, bw, BTN_H,
          ID_A_UPDATE);
    child(L"BUTTON", ff_tr(S_BTN_VERSIONS), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + 16 + bw + 8, by, bw,
          BTN_H, ID_A_VERSIONS);
    child(L"BUTTON", ff_tr(S_BTN_REPORT), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + 16 + 2 * (bw + 8), by,
          bw, BTN_H, ID_A_REPORT);
    child(L"BUTTON", ff_tr(S_VER_CLOSE), BS_PUSHBUTTON | WS_TABSTOP, MARGIN + CARD_INNER_W - 120,
          CARDS_A[2].y + CARDS_A[2].h + 16, 120, BTN_H, ID_A_CLOSE);
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
    for (i = 0; i < 3; i++) {
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

/* смена языка пересоздаёт и это окно, и окно настроек, чтобы все строки встали заново */
static void change_language(int mode)
{
    ui.app->cfg.language = mode;
    ui_language_apply(mode);
    app_save(ui.app);
    PostMessageW(ui.app->hwnd, WM_APP_STATE, 0, 0);
    ui_settings_destroy();
    PostMessageW(g_win, WM_REOPEN, 0, 0);
}

static void on_command(int id, int code)
{
    ff_config *c = &ui.app->cfg;
    int sel = 0;

    if (code == CBN_SELCHANGE) {
        sel = (int)SendMessageW((HWND)GetDlgItem(g_win, id), CB_GETCURSEL, 0, 0);
    }
    if (id == ID_A_LANG && code == CBN_SELCHANGE) {
        change_language(sel);
    } else if (id == ID_A_THEME && code == CBN_SELCHANGE) {
        c->theme = sel;
        theme_set_mode(sel);
        ui_theme_apply();
        app_save(ui.app);
    } else if (id == ID_A_PAUSE && code == CBN_SELCHANGE) {
        c->auto_pause = sel;
        app_wake(ui.app);
        app_save(ui.app);
    } else if (id == ID_A_ACTION && code == CBN_SELCHANGE) {
        c->game_action = sel;
        app_wake(ui.app);
        app_save(ui.app);
    } else if (id == ID_A_CHECK) {
        c->check_updates = SendMessageW(GetDlgItem(g_win, ID_A_CHECK), BM_GETCHECK, 0, 0) == BST_CHECKED;
        app_save(ui.app);
    } else if (id == ID_A_UPDATE && FF_REPO[0]) {
        ui_settings_set_status(ff_tr(S_CHECKING));
        update_check_async(ui.app->hwnd, FF_REPO, 1);
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
                              (HWND)l != GetDlgItem(h, ID_A_CHECK) ? (HWND)l : NULL);
    case WM_SETTINGCHANGE:
        if (l && wcscmp((const wchar_t *)l, L"ImmersiveColorSet") == 0) {
            ui_theme_apply();
        }
        return 0;
    case WM_REOPEN:
        DestroyWindow(h);
        ui_app_show(ui.app);
        return 0;
    case WM_CLOSE:
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
                            style, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                            NULL, NULL, a->inst, NULL);
    SendMessageW(g_win, WM_SETICON, ICON_BIG,
                 (LPARAM)LoadImageW(a->inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0));
    SendMessageW(g_win, WM_SETICON, ICON_SMALL,
                 (LPARAM)LoadImageW(a->inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    build();
    ShowWindow(g_win, SW_SHOW);
    SetForegroundWindow(g_win);
}
