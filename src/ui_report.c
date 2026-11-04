#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "i18n.h"
#include "report_actions.h"
#include "report_send.h"
#include "resource.h"
#include "stats_store.h"
#include "theme.h"
#include "ui.h"
#include "ui_paint.h"
#include "ui_state.h"
#include "version.h"

#define REP_W 640
#define REP_H 702
#define COMMENT_CHARS 400
#define STATUS_CHARS 320

enum {
    ID_R_COLLECT = 800,
    ID_R_SYS,
    ID_R_SET,
    ID_R_STATS,
    ID_R_COMMENT,
    ID_R_PREVIEW,
    ID_R_SEND,
    ID_R_ISSUE,
    ID_R_COPY,
    ID_R_SAVE,
    ID_R_CLEAR,
    ID_R_CLOSE
};

static const wchar_t CLASS_NAME[] = L"fringefix_report";
static HWND g_win;
static HWND g_preview;
static HWND g_comment;
static HFONT g_mono;
static wchar_t g_status[STATUS_CHARS];
/* ровно те байты, что уйдут по сети; окно предпросмотра показывает их же */
static char g_payload[FF_REPORT_MAX];
static int g_len;
static wchar_t g_preview_text[FF_REPORT_MAX * 2];

static HWND child(const wchar_t *cls, const wchar_t *text, DWORD style, DWORD ex, int x, int y,
                  int w, int h, int id)
{
    return CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h),
                           g_win, (HMENU)(INT_PTR)id, ui.app->inst, NULL);
}

static void set_status(const wchar_t *text)
{
    RECT r;

    wcsncpy(g_status, text, STATUS_CHARS - 1);
    g_status[STATUS_CHARS - 1] = L'\0';
    if (g_win) {
        SetRect(&r, 0, S(584), S(REP_W), S(608));
        InvalidateRect(g_win, &r, FALSE);
    }
}

static int checked(int id)
{
    return SendMessageW(GetDlgItem(g_win, id), BM_GETCHECK, 0, 0) == BST_CHECKED;
}

/* пересобирает отчёт из окна и показывает его же в предпросмотре; кнопки отправки зависят от сборки */
static void refresh(void)
{
    char comment[FF_COMMENT_MAX + 1];

    report_read_comment(g_comment, COMMENT_CHARS, comment);
    g_len = report_compose(ui.app, comment, checked(ID_R_SYS), checked(ID_R_SET), checked(ID_R_STATS),
                           g_payload, sizeof g_payload);
    if (g_len < 0) {
        g_len = 0;
        g_payload[0] = '\0';
        set_status(ff_tr(S_REP_TOO_BIG));
    }
    report_preview_text(g_payload, g_len, g_preview_text, FF_REPORT_MAX * 2);
    SetWindowTextW(g_preview, g_preview_text);
    EnableWindow(GetDlgItem(g_win, ID_R_SEND), FF_REPORT_URL[0] && g_len > 0);
    EnableWindow(GetDlgItem(g_win, ID_R_ISSUE), FF_REPO[0] && g_len > 0);
}

/* отчёт целиком кладётся в буфер обмена, а в ссылку идёт только заголовок и просьба вставить: у ссылки есть предел длины */
static void open_issue(void)
{
    wchar_t title[96];

    report_copy(g_win, g_preview_text);
    _snwprintf(title, 96, L"fringefix report %ls", L"" FF_VERSION);
    title[95] = L'\0';
    report_open_issue(g_win, title, ff_tr(S_REP_ISSUE_BODY));
    set_status(ff_tr(S_REP_COPIED));
}

static void do_send(void)
{
    if (MessageBoxW(g_win, ff_tr(S_REP_CONFIRM), L"fringefix",
                    MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND | MB_TOPMOST) != IDYES) {
        return;
    }
    set_status(ff_tr(S_REP_SENDING));
    EnableWindow(GetDlgItem(g_win, ID_R_SEND), FALSE);
    /* английскому интерфейсу первым идёт европейский сервер, русскому московский */
    report_send_async(ui.app->hwnd, FF_REPORT_URL, g_payload, (size_t)g_len,
                      ff_i18n_get() == FF_LANG_EN);
}

static void on_command(int id, int code)
{
    switch (id) {
    case ID_R_COLLECT:
        ui.app->cfg.stats = checked(ID_R_COLLECT);
        stats_store_set_enabled(ui.app->cfg.stats);
        app_save(ui.app);
        refresh();
        break;
    case ID_R_SYS:
    case ID_R_SET:
    case ID_R_STATS:
        refresh();
        break;
    case ID_R_COMMENT:
        if (code == EN_CHANGE) {
            refresh();
        }
        break;
    case ID_R_SEND:
        do_send();
        break;
    case ID_R_ISSUE:
        open_issue();
        break;
    case ID_R_COPY:
        report_copy(g_win, g_preview_text);
        set_status(ff_tr(S_REP_COPIED));
        break;
    case ID_R_SAVE:
        if (report_save_file(g_win, g_payload, g_len)) {
            set_status(ff_tr(S_REP_SAVED));
        }
        break;
    case ID_R_CLEAR:
        stats_store_clear();
        refresh();
        set_status(ff_tr(S_REP_CLEARED));
        break;
    case ID_R_CLOSE:
        DestroyWindow(g_win);
        break;
    }
}

static void paint(HWND h)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    const ff_palette *p = theme_get();
    RECT r;
    RECT hint;

    GetClientRect(h, &r);
    FillRect(dc, &r, paint_bg());
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, paint_font());
    SetTextColor(dc, p->muted);
    SetRect(&hint, S(24), S(14), S(REP_W - 24), S(78));
    DrawTextW(dc, ff_tr(S_REP_HINT), -1, &hint, DT_WORDBREAK);
    SetTextColor(dc, p->fg);
    TextOutW(dc, S(24), S(176), ff_tr(S_REP_COMMENT), (int)wcslen(ff_tr(S_REP_COMMENT)));
    TextOutW(dc, S(24), S(276), ff_tr(S_REP_PREVIEW), (int)wcslen(ff_tr(S_REP_PREVIEW)));
    SetTextColor(dc, p->muted);
    SetRect(&hint, S(24), S(584), S(REP_W - 24), S(608));
    DrawTextW(dc, g_status, -1, &hint, DT_WORDBREAK);
    EndPaint(h, &ps);
}

static void check_box(int id, str_id text, int y, int on)
{
    HWND c = child(L"BUTTON", ff_tr(text), BS_AUTOCHECKBOX | WS_TABSTOP, 0, 24, y, REP_W - 48, 22, id);

    SendMessageW(c, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
}

static void button(int id, str_id text, int col, int y)
{
    int bw = (REP_W - 48 - 2 * 12) / 3;

    child(L"BUTTON", ff_tr(text), BS_PUSHBUTTON | WS_TABSTOP, 0, 24 + col * (bw + 12), y, bw, 30, id);
}

static void build(void)
{
    check_box(ID_R_COLLECT, S_REP_COLLECT, 84, ui.app->cfg.stats);
    check_box(ID_R_SYS, S_REP_SYSTEM, 108, 1);
    check_box(ID_R_SET, S_REP_SETTINGS, 130, 1);
    check_box(ID_R_STATS, S_REP_STATS, 152, 1);
    g_comment = child(L"EDIT", L"", ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_TABSTOP,
                      WS_EX_CLIENTEDGE, 24, 194, REP_W - 48, 72, ID_R_COMMENT);
    SendMessageW(g_comment, EM_SETLIMITTEXT, COMMENT_CHARS, 0);
    g_preview = child(L"EDIT", L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_TABSTOP,
                      WS_EX_CLIENTEDGE, 24, 296, REP_W - 48, 282, ID_R_PREVIEW);
    button(ID_R_SEND, S_REP_SEND, 0, 614);
    button(ID_R_ISSUE, S_REP_ISSUE, 1, 614);
    button(ID_R_COPY, S_REP_COPY, 2, 614);
    button(ID_R_SAVE, S_REP_SAVE, 0, 652);
    button(ID_R_CLEAR, S_REP_CLEAR, 1, 652);
    button(ID_R_CLOSE, S_VER_CLOSE, 2, 652);
    ctl_restyle_window(g_win);
    /* предпросмотр моноширинным шрифтом, чтобы JSON читался */
    g_mono = CreateFontW(-MulDiv(9, ui.dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                         CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
    SendMessageW(g_preview, WM_SETFONT, (WPARAM)g_mono, TRUE);
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
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
        /* предпросмотр только для чтения присылает STATIC, красим его как поле */
        return paint_ctlcolor(m, (HDC)w, (HWND)l, (HWND)l == g_preview ? g_preview : NULL);
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
        if (g_mono) {
            DeleteObject(g_mono);
            g_mono = NULL;
        }
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void ui_report_show(ff_app *a)
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

    SetRect(&r, 0, 0, S(REP_W), S(REP_H));
    AdjustWindowRect(&r, style, FALSE);
    g_win = CreateWindowExW(WS_EX_TOPMOST | WS_EX_CONTROLPARENT, CLASS_NAME, ff_tr(S_REP_TITLE),
                            style, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                            NULL, NULL, a->inst, NULL);
    SendMessageW(g_win, WM_SETICON, ICON_BIG,
                 (LPARAM)LoadImageW(a->inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0));
    SendMessageW(g_win, WM_SETICON, ICON_SMALL,
                 (LPARAM)LoadImageW(a->inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    g_status[0] = L'\0';
    build();
    refresh();
    set_status(FF_REPORT_URL[0] ? L"" : ff_tr(S_REP_NO_URL));
    ShowWindow(g_win, SW_SHOW);
    SetForegroundWindow(g_win);
}

/* ответ сервера из фонового потока; окно могли закрыть, тогда результат просто освобождается */
void ui_report_on_result(ff_app *a, ff_report_result *res)
{
    wchar_t text[STATUS_CHARS];
    wchar_t arg[160];

    (void)a;
    if (g_win) {
        if (res->ok) {
            ui_utf8(res->id[0] ? res->id : "ok", arg, 160);
            _snwprintf(text, STATUS_CHARS, ff_tr(S_REP_SENT), arg);
        } else {
            ui_utf8(res->msg, arg, 160);
            _snwprintf(text, STATUS_CHARS, ff_tr(S_REP_SEND_FAILED), arg);
            EnableWindow(GetDlgItem(g_win, ID_R_SEND), FF_REPORT_URL[0] != '\0');
        }
        text[STATUS_CHARS - 1] = L'\0';
        set_status(text);
    }
    free(res);
}
