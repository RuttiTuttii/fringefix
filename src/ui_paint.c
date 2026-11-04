#include <windows.h>

#include "i18n.h"
#include "theme.h"
#include "ui_layout.h"
#include "ui_paint.h"

#define S(v) MulDiv((v), dpi, 96)

static HFONT g_font;
static HFONT g_bold;
static HBRUSH g_bg;
static HBRUSH g_field;

void paint_free(void)
{
    if (g_font) {
        DeleteObject(g_font);
    }
    if (g_bold) {
        DeleteObject(g_bold);
    }
    if (g_bg) {
        DeleteObject(g_bg);
    }
    if (g_field) {
        DeleteObject(g_field);
    }
    g_font = g_bold = NULL;
    g_bg = g_field = NULL;
}

void paint_init(int dpi)
{
    const ff_palette *p = theme_get();

    paint_free();
    g_font = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                         CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_bold = CreateFontW(-MulDiv(10, dpi, 72), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0,
                         0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_bg = CreateSolidBrush(p->bg);
    g_field = CreateSolidBrush(p->field);
}

HFONT paint_font(void)
{
    return g_font;
}

HFONT paint_bold(void)
{
    return g_bold;
}

LRESULT paint_ctlcolor(UINT msg, HDC dc, HWND ctl, HWND field_ctl)
{
    const ff_palette *p = theme_get();
    int field = msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX || (ctl && ctl == field_ctl);

    SetTextColor(dc, p->fg);
    SetBkColor(dc, field ? p->field : p->bg);
    return (LRESULT)(field ? g_field : g_bg);
}

HBRUSH paint_bg(void)
{
    return g_bg;
}

HBRUSH paint_field(void)
{
    return g_field;
}

static void text(HDC dc, int x, int y, const wchar_t *s, COLORREF color)
{
    SetTextColor(dc, color);
    TextOutW(dc, x, y, s, (int)wcslen(s));
}

static void card(HDC dc, int c, int dpi, const ff_palette *p)
{
    HBRUSH fill = CreateSolidBrush(p->card);
    HPEN edge = CreatePen(PS_SOLID, 1, p->border);
    HGDIOBJ ob = SelectObject(dc, fill);
    HGDIOBJ op = SelectObject(dc, edge);
    int x = S(MARGIN);
    int y = S(card_y(c));

    RoundRect(dc, x, y, x + S(CARD_W), y + S(card_h(c)), S(10), S(10));
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(edge);
    DeleteObject(fill);

    SelectObject(dc, g_bold);
    text(dc, x + S(COL_LABEL), y + S(7), ff_tr(CARDS[c].title), p->accent);
    SelectObject(dc, g_font);
}

void paint_window(HWND h, int dpi, const wchar_t *status)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    const ff_palette *p = theme_get();
    RECT r;
    int i;

    GetClientRect(h, &r);
    FillRect(dc, &r, g_bg);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, g_font);

    text(dc, S(MARGIN), S(10), ff_tr(S_HINT), p->muted);
    for (i = 0; i < NCARDS; i++) {
        card(dc, i, dpi, p);
    }
    for (i = 0; i < NROWS; i++) {
        int x = S(MARGIN + COL_LABEL);
        int y = S(row_y(&ROWS[i])) + S(4);

        text(dc, x, y, ff_tr(ROWS[i].label), p->fg);
        /* единица измерения только у сдвигов, у резкости её нет */
        if (ROWS[i].idx < IDX_SHARPEN) {
            text(dc, S(MARGIN + COL_UNIT), y, L"px", p->muted);
        }
    }
    text(dc, S(MARGIN), S(STATUS_Y), status, p->muted);
    EndPaint(h, &ps);
}
