#include <windows.h>
#include <richedit.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
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

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#define ABOUT_W 480
#define CHANGES_W 600
#define CHANGES_H 520
#define CHANGES_MAX (96 * 1024)

/* страница проекта, когда сборка без репозитория для обновлений (локальная сборка, FF_REPO пуст) */
#define DEFAULT_REPO "RuttiTuttii/fringefix"

enum { ID_AB_CHANGES = 900, ID_AB_SITE, ID_AB_FOLDER, ID_AB_CLOSE, ID_CH_TEXT, ID_CH_CLOSE };

/* высоты абзацев зависят от языка, шрифта и масштаба, поэтому раскладка окна считается по тексту (всё при 96 dpi) */
typedef struct {
    int desc_h;
    int lic_y;
    int cred_y;
    int card_h;
    int btn_y;
    int win_h;
} about_lay;

static about_lay g_lay;
static const wchar_t ABOUT_CLASS[] = L"fringefix_about";
static const wchar_t CHANGES_CLASS[] = L"fringefix_changes";
static HWND g_about;
static HWND g_changes;

static HWND child(HWND parent, const wchar_t *cls, const wchar_t *text, DWORD style, DWORD ex, int x, int y, int w,
                  int h, int id)
{
    return CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h), parent,
                           (HMENU)(INT_PTR)id, ui.app->inst, NULL);
}

static void register_class(const wchar_t *name, WNDPROC proc)
{
    WNDCLASSW wc;

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = ui.app->inst;
    wc.lpszClassName = name;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    RegisterClassW(&wc);
}

static void set_icons(HWND h)
{
    SendMessageW(h, WM_SETICON, ICON_BIG,
                 (LPARAM)LoadImageW(ui.app->inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));
    SendMessageW(h, WM_SETICON, ICON_SMALL,
                 (LPARAM)LoadImageW(ui.app->inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
}

/* окно по центру окна настроек, а без него по умолчанию */
static HWND make_window(const wchar_t *cls, const wchar_t *title, int w96, int h96)
{
    DWORD style = WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    RECT r;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    RECT owner;
    HWND h;

    SetRect(&r, 0, 0, S(w96), S(h96));
    AdjustWindowRect(&r, style, FALSE);
    if (ui.win && IsWindowVisible(ui.win) && GetWindowRect(ui.win, &owner)) {
        x = owner.left + ((owner.right - owner.left) - (r.right - r.left)) / 2;
        y = owner.top + ((owner.bottom - owner.top) - (r.bottom - r.top)) / 2;
    }
    h = CreateWindowExW(WS_EX_TOPMOST | WS_EX_CONTROLPARENT, cls, title, style, x, y, r.right - r.left,
                        r.bottom - r.top, NULL, NULL, ui.app->inst, NULL);
    if (h) {
        set_icons(h);
    }
    return h;
}

static void open_target(const wchar_t *target)
{
    ShellExecuteW(NULL, L"open", target, NULL, NULL, SW_SHOWNORMAL);
}

static void about_command(int id)
{
    if (id == ID_AB_CHANGES) {
        ui_changes_show(ui.app);
    } else if (id == ID_AB_SITE) {
        wchar_t url[160];

        _snwprintf(url, 160, L"https://github.com/%hs", FF_REPO[0] ? FF_REPO : DEFAULT_REPO);
        url[159] = L'\0';
        open_target(url);
    } else if (id == ID_AB_FOLDER) {
        wchar_t dir[MAX_PATH];
        wchar_t *slash;

        wcsncpy(dir, ui.app->ini, MAX_PATH - 1);
        dir[MAX_PATH - 1] = L'\0';
        slash = wcsrchr(dir, L'\\');
        if (slash) {
            *slash = L'\0';
            open_target(dir);
        }
    } else if (id == ID_AB_CLOSE || id == IDCANCEL) {
        DestroyWindow(g_about);
    }
}

/* высота абзаца в 96-х долях при ширине текста окна */
static int para_h(HDC dc, const wchar_t *text)
{
    RECT r;

    SetRect(&r, 0, 0, S(ABOUT_W - 2 * MARGIN - 28), 0);
    DrawTextW(dc, text, -1, &r, DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
    return MulDiv(r.bottom, 96, ui.dpi);
}

static void about_measure(HDC dc)
{
    HGDIOBJ old = SelectObject(dc, paint_font());
    int cred_h;

    g_lay.desc_h = para_h(dc, ff_tr(S_ABOUT_DESC));
    g_lay.lic_y = 114 + g_lay.desc_h + 10;
    g_lay.cred_y = g_lay.lic_y + para_h(dc, ff_tr(S_ABOUT_LICENSE)) + 8;
    cred_h = para_h(dc, ff_tr(S_ABOUT_CREDITS));
    g_lay.card_h = g_lay.cred_y + cred_h;
    g_lay.btn_y = 14 + g_lay.card_h + 18;
    g_lay.win_h = g_lay.btn_y + BTN_H + 10 + BTN_H + 14;
    SelectObject(dc, old);
}

static void about_paint(HWND h)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    const ff_palette *p = theme_get();
    RECT r;
    wchar_t ver[96];
    HICON icon = (HICON)LoadImageW(ui.app->inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, S(64), S(64), 0);

    GetClientRect(h, &r);
    FillRect(dc, &r, paint_bg());
    SetBkMode(dc, TRANSPARENT);
    paint_card(dc, ui.dpi, MARGIN, 14, ABOUT_W - 2 * MARGIN, g_lay.card_h, L"");
    if (icon) {
        DrawIconEx(dc, S(MARGIN + 14), S(30), icon, S(64), S(64), 0, NULL, DI_NORMAL);
        DestroyIcon(icon);
    }
    SelectObject(dc, paint_bold());
    SetTextColor(dc, p->fg);
    TextOutW(dc, S(MARGIN + 94), S(34), L"fringefix", 9);
    SelectObject(dc, paint_font());
    SetTextColor(dc, p->muted);
    _snwprintf(ver, 96, ff_tr(S_VERSION), L"" FF_VERSION);
    ver[95] = L'\0';
    TextOutW(dc, S(MARGIN + 94), S(58), ver, (int)wcslen(ver));
    SetTextColor(dc, p->fg);
    TextOutW(dc, S(MARGIN + 94), S(80), ff_tr(S_ABOUT_AUTHOR), (int)wcslen(ff_tr(S_ABOUT_AUTHOR)));

    SetTextColor(dc, p->fg);
    SetRect(&r, S(MARGIN + 14), S(114), S(ABOUT_W - MARGIN - 14), S(114 + g_lay.desc_h));
    DrawTextW(dc, ff_tr(S_ABOUT_DESC), -1, &r, DT_WORDBREAK | DT_NOPREFIX);
    SetTextColor(dc, p->muted);
    SetRect(&r, S(MARGIN + 14), S(g_lay.lic_y), S(ABOUT_W - MARGIN - 14), S(g_lay.cred_y - 8));
    DrawTextW(dc, ff_tr(S_ABOUT_LICENSE), -1, &r, DT_WORDBREAK | DT_NOPREFIX);
    SetRect(&r, S(MARGIN + 14), S(g_lay.cred_y), S(ABOUT_W - MARGIN - 14), S(14 + g_lay.card_h));
    DrawTextW(dc, ff_tr(S_ABOUT_CREDITS), -1, &r, DT_WORDBREAK | DT_NOPREFIX);
    EndPaint(h, &ps);
}

static LRESULT CALLBACK about_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT:
        about_paint(h);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_COMMAND:
        about_command(LOWORD(w));
        return 0;
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
        return paint_ctlcolor(m, (HDC)w, (HWND)l, NULL);
    case WM_DPICHANGED:
        /* масштаб общий для окон: новый запоминается, окно открывается заново */
        ui_dpi_apply((int)HIWORD(w));
        DestroyWindow(h);
        ui_about_show(ui.app);
        return 0;
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        g_about = NULL;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void ui_about_show(ff_app *a)
{
    int bw = 104;
    int gap = 8;
    int by;
    HDC dc;

    ui.app = a;
    if (g_about) {
        SetForegroundWindow(g_about);
        return;
    }
    register_class(ABOUT_CLASS, about_proc);
    dc = GetDC(NULL);
    about_measure(dc);
    ReleaseDC(NULL, dc);
    by = g_lay.btn_y;
    g_about = make_window(ABOUT_CLASS, ff_tr(S_ABOUT_TITLE), ABOUT_W, g_lay.win_h);
    if (!g_about) {
        return;
    }
    child(g_about, L"BUTTON", ff_tr(S_M_CHANGES), BS_PUSHBUTTON | WS_TABSTOP, 0, MARGIN, by, bw + 18, BTN_H, ID_AB_CHANGES);
    child(g_about, L"BUTTON", ff_tr(S_ABOUT_SITE), BS_PUSHBUTTON | WS_TABSTOP, 0, MARGIN + bw + 18 + gap, by, bw + 18,
          BTN_H, ID_AB_SITE);
    child(g_about, L"BUTTON", ff_tr(S_ABOUT_FOLDER), BS_PUSHBUTTON | WS_TABSTOP, 0, MARGIN + 2 * (bw + 18 + gap), by,
          bw + 18, BTN_H, ID_AB_FOLDER);
    child(g_about, L"BUTTON", ff_tr(S_VER_CLOSE), BS_DEFPUSHBUTTON | WS_TABSTOP, 0, ABOUT_W - MARGIN - 90, by + BTN_H + 10, 90,
          BTN_H, ID_AB_CLOSE);
    ctl_restyle_window(g_about);
    ShowWindow(g_about, SW_SHOW);
    SetForegroundWindow(g_about);
}

/* текст ресурса в UTF-16 с переводами строк как в файле; malloc-буфер или NULL, если ресурса нет */
static wchar_t *load_changes_raw(HINSTANCE inst)
{
    int id = ff_i18n_get() == FF_LANG_RU ? IDR_CHANGES_RU : IDR_CHANGES_EN;
    HRSRC res = FindResourceW(inst, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW((ULONG_PTR)RT_RCDATA));
    HGLOBAL mem = res ? LoadResource(inst, res) : NULL;
    const char *src = mem ? LockResource(mem) : NULL;
    DWORD size = res ? SizeofResource(inst, res) : 0;
    wchar_t *wide;
    int n;

    if (!src || size == 0 || size > CHANGES_MAX) {
        return NULL;
    }
    n = MultiByteToWideChar(CP_UTF8, 0, src, (int)size, NULL, 0);
    wide = malloc(((size_t)n + 1) * sizeof *wide);
    if (!wide || MultiByteToWideChar(CP_UTF8, 0, src, (int)size, wide, n) != n) {
        free(wide);
        return NULL;
    }
    wide[n] = L'\0';
    return wide;
}

/* виды строк markdown, которые понимает показ: заголовки трёх уровней, пункт списка, обычный текст */
enum { MD_TEXT, MD_H1, MD_H2, MD_H3, MD_ITEM };

/* строка без разметки и её вид; обратные кавычки кода в показе не нужны, они убираются */
static int md_line(wchar_t *line, wchar_t **text)
{
    int kind = MD_TEXT;
    wchar_t *w;
    wchar_t *o;

    if (wcsncmp(line, L"### ", 4) == 0) {
        kind = MD_H3;
        line += 4;
    } else if (wcsncmp(line, L"## ", 3) == 0) {
        kind = MD_H2;
        line += 3;
    } else if (wcsncmp(line, L"# ", 2) == 0) {
        kind = MD_H1;
        line += 2;
    } else if (wcsncmp(line, L"- ", 2) == 0) {
        kind = MD_ITEM;
        line += 2;
    }
    for (w = o = line; *w; w++) {
        if (*w == L'[') {
            /* ссылка [текст](адрес) показывается одним текстом */
            wchar_t *close = wcsstr(w, L"](");
            wchar_t *end = close ? wcschr(close, L')') : NULL;

            if (end) {
                for (w++; w < close; w++) {
                    *o++ = *w;
                }
                w = end;
                continue;
            }
        }
        if (*w != L'`') {
            *o++ = *w;
        }
    }
    *o = L'\0';
    *text = line;
    return kind;
}

/* дописывает строку в конец поля RichEdit с оформлением по виду: заголовки жирные и крупнее, пункты с висячим отступом */
static void rich_append(HWND e, const wchar_t *text, int kind)
{
    const ff_palette *p = theme_get();
    CHARFORMAT2W cf;
    PARAFORMAT2 pf;
    wchar_t line[1024];
    static const int SIZE_TWIPS[] = { 180, 280, 230, 190, 180 };

    memset(&cf, 0, sizeof cf);
    cf.cbSize = sizeof cf;
    cf.dwMask = CFM_COLOR | CFM_BOLD | CFM_SIZE | CFM_FACE;
    cf.dwEffects = kind == MD_TEXT || kind == MD_ITEM ? 0 : CFE_BOLD;
    cf.crTextColor = kind == MD_H2 || kind == MD_H3 ? p->accent : p->fg;
    cf.yHeight = SIZE_TWIPS[kind];
    wcscpy(cf.szFaceName, L"Segoe UI");

    memset(&pf, 0, sizeof pf);
    pf.cbSize = sizeof pf;
    pf.dwMask = PFM_STARTINDENT | PFM_OFFSET | PFM_SPACEBEFORE | PFM_SPACEAFTER | PFM_TABSTOPS;
    pf.dySpaceBefore = kind == MD_H2 ? 360 : (kind == MD_H3 ? 200 : 0);
    pf.dySpaceAfter = kind == MD_ITEM ? 60 : 80;
    if (kind == MD_ITEM) {
        pf.dxStartIndent = 300;
        pf.dxOffset = -300;
        pf.cTabCount = 1;
        pf.rgxTabs[0] = 300;
    }
    _snwprintf(line, 1024, kind == MD_ITEM ? L"\x2022\t%ls\r" : L"%ls\r", text);
    line[1023] = L'\0';
    SendMessageW(e, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(e, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessageW(e, EM_SETPARAFORMAT, 0, (LPARAM)&pf);
    SendMessageW(e, EM_REPLACESEL, FALSE, (LPARAM)line);
}

/* markdown списка изменений в поле RichEdit построчно */
static void rich_fill(HWND e, wchar_t *raw)
{
    wchar_t *line = raw;

    while (*line) {
        wchar_t *eol = line;
        wchar_t *text;
        int kind;

        while (*eol && *eol != L'\n') {
            eol++;
        }
        {
            wchar_t saved = *eol;

            *eol = L'\0';
            if (eol > line && eol[-1] == L'\r') {
                eol[-1] = L'\0';
            }
            if (line[0] != L'\0') {
                kind = md_line(line, &text);
                rich_append(e, text, kind);
            }
            if (!saved) {
                break;
            }
        }
        line = eol + 1;
    }
    SendMessageW(e, EM_SETSEL, 0, 0);
}

/* запасной вид без RichEdit: те же строки простым текстом поля ввода; malloc-буфер */
static wchar_t *plain_from_md(wchar_t *raw)
{
    size_t n = wcslen(raw);
    wchar_t *out = malloc((n * 2 + 8) * sizeof *out);
    wchar_t *line = raw;
    size_t o = 0;

    if (!out) {
        return NULL;
    }
    while (*line) {
        wchar_t *eol = line;
        wchar_t *text;
        int kind;

        while (*eol && *eol != L'\n') {
            eol++;
        }
        {
            wchar_t saved = *eol;

            *eol = L'\0';
            if (eol > line && eol[-1] == L'\r') {
                eol[-1] = L'\0';
            }
            if (line[0] != L'\0') {
                kind = md_line(line, &text);
                if (kind == MD_ITEM) {
                    out[o++] = 0x2022;
                    out[o++] = L' ';
                }
                while (*text) {
                    out[o++] = *text++;
                }
            }
            out[o++] = L'\r';
            out[o++] = L'\n';
            if (!saved) {
                break;
            }
        }
        line = eol + 1;
    }
    out[o] = L'\0';
    return out;
}

static void changes_layout(HWND h)
{
    RECT r;

    GetClientRect(h, &r);
    MoveWindow(GetDlgItem(h, ID_CH_TEXT), S(MARGIN), S(MARGIN), r.right - S(2 * MARGIN),
               r.bottom - S(MARGIN + 54), TRUE);
    MoveWindow(GetDlgItem(h, ID_CH_CLOSE), r.right - S(MARGIN + 90), r.bottom - S(MARGIN + 30), S(90), S(BTN_H), TRUE);
}

static LRESULT CALLBACK changes_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_ERASEBKGND: {
        RECT r;

        GetClientRect(h, &r);
        FillRect((HDC)w, &r, paint_bg());
        return 1;
    }
    case WM_COMMAND:
        if (LOWORD(w) == ID_CH_CLOSE || LOWORD(w) == IDCANCEL) {
            DestroyWindow(h);
        }
        return 0;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        /* только чтение у поля даёт WM_CTLCOLORSTATIC: красим его как поле */
        return paint_ctlcolor(m, (HDC)w, (HWND)l, (HWND)l == GetDlgItem(h, ID_CH_TEXT) ? (HWND)l : NULL);
    case WM_DPICHANGED:
        ui_dpi_apply((int)HIWORD(w));
        DestroyWindow(h);
        ui_changes_show(ui.app);
        return 0;
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        g_changes = NULL;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void ui_changes_show(ff_app *a)
{
    wchar_t *raw;
    HWND edit = NULL;
    HMODULE rich = LoadLibraryW(L"Msftedit.dll");

    ui.app = a;
    if (g_changes) {
        SetForegroundWindow(g_changes);
        return;
    }
    register_class(CHANGES_CLASS, changes_proc);
    g_changes = make_window(CHANGES_CLASS, ff_tr(S_CHANGES_TITLE), CHANGES_W, CHANGES_H);
    if (!g_changes) {
        return;
    }
    raw = load_changes_raw(a->inst);
    if (rich && raw) {
        const ff_palette *p = theme_get();

        edit = child(g_changes, L"RICHEDIT50W", L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP,
                     WS_EX_CLIENTEDGE, MARGIN, MARGIN, CHANGES_W - 2 * MARGIN, CHANGES_H - MARGIN - 54, ID_CH_TEXT);
        if (edit) {
            SendMessageW(edit, EM_SETBKGNDCOLOR, 0, (LPARAM)p->field);
            SendMessageW(edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(S(10), S(10)));
            rich_fill(edit, raw);
            theme_apply_control(edit, 1);
        }
    }
    if (!edit) {
        /* без RichEdit или без ресурса: простой текст в обычном поле */
        wchar_t *plain = raw ? plain_from_md(raw) : NULL;

        edit = child(g_changes, L"EDIT", plain ? plain : ff_tr(S_CHANGES_NONE),
                     ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, MARGIN,
                     MARGIN, CHANGES_W - 2 * MARGIN, CHANGES_H - MARGIN - 54, ID_CH_TEXT);
        free(plain);
        SendMessageW(edit, EM_SETSEL, 0, 0);
    }
    free(raw);
    child(g_changes, L"BUTTON", ff_tr(S_VER_CLOSE), BS_DEFPUSHBUTTON | WS_TABSTOP, 0, CHANGES_W - MARGIN - 90,
          CHANGES_H - MARGIN - 30, 90, BTN_H, ID_CH_CLOSE);
    changes_layout(g_changes);
    ctl_restyle_window(g_changes);
    ShowWindow(g_changes, SW_SHOW);
    SetForegroundWindow(g_changes);
}
