#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "applist.h"
#include "i18n.h"
#include "resource.h"
#include "theme.h"
#include "ui.h"
#include "ui_layout.h"
#include "ui_paint.h"
#include "ui_state.h"

#define APPS_W 520
#define CARD_INNER_W 488
#define LIST_H 110
#define CARD_H (CARD_HEAD + LIST_H + CARD_PAD)
#define APPS_TOP 16
#define LIST_W 300
#define BTN_X (MARGIN + 330)
#define BTN_W 144
#define BTN_HEIGHT 30
#define CLOSE_Y (APPS_TOP + 2 * CARD_H + CARD_GAP + 16)
#define APPS_H (CLOSE_Y + BTN_HEIGHT + 26)

#define PICK_W 440
#define PICK_H 430
#define PICK_PAD 16
#define PROC_MAX 256
#define EXE_CHARS 64
#define TITLE_CHARS 96
#define NAME_CHARS 64

/* два списка окна: 0 это ignore_apps (всегда применять фильтр), 1 это pause_apps (всегда на паузу) */
enum { LIST_APPLY, LIST_PAUSE, NLISTS };

enum {
    ID_L_LIST = 900,
    ID_L_ADD = 910,
    ID_L_DEL = 920,
    ID_L_CLOSE = 930,
    ID_P_EDIT = 940,
    ID_P_LIST
};

static const wchar_t CLASS_APPS[] = L"fringefix_apps";
static const wchar_t CLASS_PICK[] = L"fringefix_picker";

static HWND g_win;
static HWND g_list[NLISTS];
static POINT g_pos;
static int g_have_pos;

/* окно выбора программы: запись процесса с видимым окном */
typedef struct {
    wchar_t exe[EXE_CHARS];
    wchar_t title[TITLE_CHARS];
} proc_t;

static proc_t g_proc[PROC_MAX];
static int g_nproc;
static int g_shown[PROC_MAX];
static HWND g_pick;
static HWND g_pick_edit;
static HWND g_pick_list;
static wchar_t g_pick_out[NAME_CHARS];
static int g_pick_ok;

static char *list_of(int which)
{
    return which == LIST_APPLY ? ui.app->cfg.ignore_apps : ui.app->cfg.pause_apps;
}

static HWND make_child(HWND parent, const wchar_t *cls, const wchar_t *text, DWORD style, DWORD ex,
                       int x, int y, int w, int h, int id)
{
    return CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h),
                           parent, (HMENU)(INT_PTR)id, ui.app->inst, NULL);
}

static int card_top(int which)
{
    return APPS_TOP + which * (CARD_H + CARD_GAP);
}

static void fill(int which)
{
    char item[NAME_CHARS];
    wchar_t w[NAME_CHARS];
    int i;

    SendMessageW(g_list[which], LB_RESETCONTENT, 0, 0);
    for (i = 0; ff_app_list_item(list_of(which), i, item, sizeof item); i++) {
        ui_utf8(item, w, NAME_CHARS);
        SendMessageW(g_list[which], LB_ADDSTRING, 0, (LPARAM)w);
    }
}

static void changed(void)
{
    fill(LIST_APPLY);
    fill(LIST_PAUSE);
    app_wake(ui.app);
    app_save(ui.app);
}

/* добавляет программу в список, из другого списка убирает: в обоих сразу она значила бы противоположное */
static void add_to(int which, const wchar_t *name)
{
    char *list = list_of(which);
    char *other = list_of(which == LIST_APPLY ? LIST_PAUSE : LIST_APPLY);

    if (ff_app_list_add(list, FF_APPS_MAX, name) < 0) {
        wchar_t text[NAME_CHARS + 160];

        _snwprintf(text, sizeof text / sizeof text[0], ff_tr(S_FORCE_FAILED), name);
        text[sizeof text / sizeof text[0] - 1] = L'\0';
        MessageBoxW(g_win, text, L"fringefix", MB_ICONWARNING | MB_TOPMOST);
        return;
    }
    ff_app_list_remove(other, FF_APPS_MAX, name);
    changed();
}

static void remove_selected(int which)
{
    wchar_t name[NAME_CHARS];
    LRESULT sel = SendMessageW(g_list[which], LB_GETCURSEL, 0, 0);

    if (sel == LB_ERR || SendMessageW(g_list[which], LB_GETTEXT, (WPARAM)sel, (LPARAM)name) == LB_ERR) {
        return;
    }
    ff_app_list_remove(list_of(which), FF_APPS_MAX, name);
    changed();
}

/* окна программ, которые можно выбрать: видимое окно верхнего уровня с заголовком, без своих окон */
static BOOL CALLBACK enum_windows(HWND w, LPARAM unused)
{
    wchar_t title[TITLE_CHARS];
    wchar_t path[MAX_PATH];
    DWORD pid = 0;
    DWORD n = MAX_PATH;
    HANDLE h;
    BOOL ok;
    const wchar_t *base;
    const wchar_t *p;
    int i;

    (void)unused;
    if (g_nproc >= PROC_MAX || !IsWindowVisible(w) || GetWindow(w, GW_OWNER) != NULL
        || (GetWindowLongW(w, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) || GetWindowTextW(w, title, TITLE_CHARS) == 0) {
        return TRUE;
    }
    GetWindowThreadProcessId(w, &pid);
    if (pid == GetCurrentProcessId()) {
        return TRUE;
    }
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) {
        return TRUE;
    }
    ok = QueryFullProcessImageNameW(h, 0, path, &n);
    CloseHandle(h);
    if (!ok) {
        return TRUE;
    }
    base = wcsrchr(path, L'\\');
    base = base ? base + 1 : path;
    /* имя не из ASCII в список не попадёт, поэтому такие процессы не показываются; повторы схлопываются */
    for (p = base; *p; p++) {
        if (*p > 127) {
            return TRUE;
        }
    }
    if (wcslen(base) >= EXE_CHARS) {
        return TRUE;
    }
    for (i = 0; i < g_nproc; i++) {
        if (_wcsicmp(g_proc[i].exe, base) == 0) {
            return TRUE;
        }
    }
    wcscpy(g_proc[g_nproc].exe, base);
    wcscpy(g_proc[g_nproc].title, title);
    g_nproc++;
    return TRUE;
}

static int by_name(const void *a, const void *b)
{
    return _wcsicmp(((const proc_t *)a)->exe, ((const proc_t *)b)->exe);
}

/* список по строке поиска: имя процесса или заголовок окна; первая строка выбрана, чтобы Enter сразу добавлял */
static void refill_picker(void)
{
    wchar_t query[NAME_CHARS];
    int i;
    int shown = 0;

    GetWindowTextW(g_pick_edit, query, NAME_CHARS);
    SendMessageW(g_pick_list, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < g_nproc; i++) {
        wchar_t label[EXE_CHARS + TITLE_CHARS + 8];

        if (!ff_app_query_match(query, g_proc[i].exe, g_proc[i].title)) {
            continue;
        }
        _snwprintf(label, sizeof label / sizeof label[0], L"%ls    %ls", g_proc[i].exe, g_proc[i].title);
        label[sizeof label / sizeof label[0] - 1] = L'\0';
        SendMessageW(g_pick_list, LB_ADDSTRING, 0, (LPARAM)label);
        g_shown[shown++] = i;
    }
    if (shown > 0) {
        SendMessageW(g_pick_list, LB_SETCURSEL, 0, 0);
    }
}

/* выбранная строка или, если подходящих нет, набранное имя: программа ещё может быть не запущена */
static void accept_pick(void)
{
    LRESULT sel = SendMessageW(g_pick_list, LB_GETCURSEL, 0, 0);

    if (sel != LB_ERR && sel >= 0 && sel < PROC_MAX) {
        wcscpy(g_pick_out, g_proc[g_shown[sel]].exe);
    } else {
        wchar_t *a;
        size_t n;

        GetWindowTextW(g_pick_edit, g_pick_out, NAME_CHARS);
        a = g_pick_out;
        while (*a == L' ') {
            a++;
        }
        n = wcslen(a);
        while (n > 0 && a[n - 1] == L' ') {
            a[--n] = L'\0';
        }
        if (n == 0 || wcschr(a, L';')) {
            return;
        }
        memmove(g_pick_out, a, (n + 1) * sizeof(wchar_t));
    }
    g_pick_ok = 1;
    DestroyWindow(g_pick);
}

static LRESULT CALLBACK pick_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT r;

        GetClientRect(h, &r);
        FillRect(dc, &r, paint_bg());
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, paint_font());
        SetTextColor(dc, theme_get()->fg);
        r.left = S(PICK_PAD);
        r.right -= S(PICK_PAD);
        r.top = S(PICK_PAD);
        r.bottom = S(PICK_PAD + 32);
        DrawTextW(dc, ff_tr(S_PICK_HINT), -1, &r, DT_WORDBREAK);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_COMMAND:
        if (LOWORD(w) == IDOK || (LOWORD(w) == ID_P_LIST && HIWORD(w) == LBN_DBLCLK)) {
            accept_pick();
        } else if (LOWORD(w) == IDCANCEL) {
            DestroyWindow(h);
        } else if (LOWORD(w) == ID_P_EDIT && HIWORD(w) == EN_CHANGE) {
            refill_picker();
        }
        return 0;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        return paint_ctlcolor(m, (HDC)w, (HWND)l, (HWND)l);
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
        return paint_ctlcolor(m, (HDC)w, (HWND)l, NULL);
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        g_pick = NULL;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

/* окно выбора программы с поиском; ждёт закрытия во вложенном цикле. 1 и имя в out, если выбрано */
static int run_picker(HWND owner, wchar_t out[NAME_CHARS])
{
    static int registered;
    RECT r;
    MSG m;

    if (!registered) {
        WNDCLASSW wc;

        memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = pick_proc;
        wc.hInstance = ui.app->inst;
        wc.lpszClassName = CLASS_PICK;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        RegisterClassW(&wc);
        registered = 1;
    }
    g_nproc = 0;
    EnumWindows(enum_windows, 0);
    qsort(g_proc, (size_t)g_nproc, sizeof g_proc[0], by_name);

    GetWindowRect(owner, &r);
    SetRect(&r, r.left + S(24), r.top + S(24), r.left + S(24) + S(PICK_W), r.top + S(24) + S(PICK_H));
    AdjustWindowRect(&r, WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN, FALSE);
    g_pick = CreateWindowExW(WS_EX_TOPMOST | WS_EX_CONTROLPARENT, CLASS_PICK, ff_tr(S_PICK_TITLE),
                             WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN, r.left, r.top, r.right - r.left,
                             r.bottom - r.top, owner, NULL, ui.app->inst, NULL);
    if (!g_pick) {
        return 0;
    }
    g_pick_edit = make_child(g_pick, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE,
                             PICK_PAD, PICK_PAD + 40, PICK_W - 2 * PICK_PAD, 24, ID_P_EDIT);
    g_pick_list = make_child(g_pick, L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP,
                             WS_EX_CLIENTEDGE, PICK_PAD, PICK_PAD + 74, PICK_W - 2 * PICK_PAD, 270,
                             ID_P_LIST);
    make_child(g_pick, L"BUTTON", ff_tr(S_PICK_ADD), BS_DEFPUSHBUTTON | WS_TABSTOP, 0,
               PICK_W - PICK_PAD - 2 * 110 - 8, PICK_H - PICK_PAD - 30, 110, 30, IDOK);
    make_child(g_pick, L"BUTTON", ff_tr(S_PICK_CANCEL), BS_PUSHBUTTON | WS_TABSTOP, 0,
               PICK_W - PICK_PAD - 110, PICK_H - PICK_PAD - 30, 110, 30, IDCANCEL);
    ctl_restyle_window(g_pick);
    refill_picker();
    g_pick_ok = 0;
    EnableWindow(owner, FALSE);
    ShowWindow(g_pick, SW_SHOW);
    SetForegroundWindow(g_pick);
    SetFocus(g_pick_edit);

    while (g_pick && GetMessageW(&m, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(g_pick, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    /* WM_QUIT посреди выбора: окно закрывается, а сигнал выхода уходит дальше главному циклу */
    if (g_pick) {
        DestroyWindow(g_pick);
        PostQuitMessage(0);
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (g_pick_ok) {
        wcscpy(out, g_pick_out);
    }
    return g_pick_ok;
}

static void on_add(int which)
{
    wchar_t name[NAME_CHARS];

    if (run_picker(g_win, name)) {
        add_to(which, name);
    }
}

static void build(void)
{
    int which;

    for (which = 0; which < NLISTS; which++) {
        int y = card_top(which) + CARD_HEAD;

        g_list[which] = make_child(g_win, L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP,
                                   WS_EX_CLIENTEDGE, MARGIN + 14, y, LIST_W, LIST_H - 4,
                                   ID_L_LIST + which);
        make_child(g_win, L"BUTTON", ff_tr(S_APPS_ADD), BS_PUSHBUTTON | WS_TABSTOP, 0, BTN_X, y,
                   BTN_W, BTN_HEIGHT, ID_L_ADD + which);
        make_child(g_win, L"BUTTON", ff_tr(S_APPS_REMOVE), BS_PUSHBUTTON | WS_TABSTOP, 0, BTN_X,
                   y + BTN_HEIGHT + 6, BTN_W, BTN_HEIGHT, ID_L_DEL + which);
        fill(which);
    }
    make_child(g_win, L"BUTTON", ff_tr(S_VER_CLOSE), BS_PUSHBUTTON | WS_TABSTOP, 0,
               MARGIN + CARD_INNER_W - 120, CLOSE_Y, 120, BTN_HEIGHT, ID_L_CLOSE);
    ctl_restyle_window(g_win);
}

static LRESULT CALLBACK apps_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT r;

        GetClientRect(h, &r);
        FillRect(dc, &r, paint_bg());
        SetBkMode(dc, TRANSPARENT);
        paint_card(dc, ui.dpi, MARGIN, card_top(LIST_APPLY), CARD_INNER_W, CARD_H, ff_tr(S_APPS_APPLY));
        paint_card(dc, ui.dpi, MARGIN, card_top(LIST_PAUSE), CARD_INNER_W, CARD_H, ff_tr(S_APPS_PAUSE));
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_COMMAND: {
        int id = LOWORD(w);

        if (id >= ID_L_ADD && id < ID_L_ADD + NLISTS) {
            on_add(id - ID_L_ADD);
        } else if (id >= ID_L_DEL && id < ID_L_DEL + NLISTS) {
            remove_selected(id - ID_L_DEL);
        } else if (id == ID_L_CLOSE) {
            DestroyWindow(h);
        }
        return 0;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        return paint_ctlcolor(m, (HDC)w, (HWND)l, (HWND)l);
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
        return paint_ctlcolor(m, (HDC)w, (HWND)l, NULL);
    case WM_SETTINGCHANGE:
        if (l && wcscmp((const wchar_t *)l, L"ImmersiveColorSet") == 0) {
            ui_theme_apply();
        }
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

void ui_apps_show(ff_app *a)
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
    wc.lpfnWndProc = apps_proc;
    wc.hInstance = a->inst;
    wc.lpszClassName = CLASS_APPS;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    RegisterClassW(&wc);

    SetRect(&r, 0, 0, S(APPS_W), S(APPS_H));
    AdjustWindowRect(&r, style, FALSE);
    g_win = CreateWindowExW(WS_EX_TOPMOST | WS_EX_CONTROLPARENT, CLASS_APPS, ff_tr(S_APPS_TITLE), style,
                            g_have_pos ? g_pos.x : CW_USEDEFAULT, g_have_pos ? g_pos.y : CW_USEDEFAULT,
                            r.right - r.left, r.bottom - r.top, NULL, NULL, a->inst, NULL);
    g_have_pos = 0;
    if (!g_win) {
        return;
    }
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

int ui_apps_get_pos(POINT *p)
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

void ui_apps_show_at(ff_app *a, int x, int y)
{
    g_pos.x = x;
    g_pos.y = y;
    g_have_pos = 1;
    ui_apps_show(a);
}
