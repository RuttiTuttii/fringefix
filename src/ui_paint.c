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
static HBRUSH g_card;

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
    if (g_card) {
        DeleteObject(g_card);
    }
    g_font = g_bold = NULL;
    g_bg = g_field = g_card = NULL;
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
    g_card = CreateSolidBrush(p->card);
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

LRESULT paint_ctlcolor_card(HDC dc)
{
    const ff_palette *p = theme_get();

    SetTextColor(dc, p->fg);
    SetBkColor(dc, p->card);
    return (LRESULT)g_card;
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

void paint_card(HDC dc, int dpi, int x96, int y96, int w96, int h96, const wchar_t *title)
{
    const ff_palette *p = theme_get();
    HBRUSH fill = CreateSolidBrush(p->card);
    HPEN edge = CreatePen(PS_SOLID, 1, p->border);
    HGDIOBJ ob = SelectObject(dc, fill);
    HGDIOBJ op = SelectObject(dc, edge);
    int x = S(x96);
    int y = S(y96);

    RoundRect(dc, x, y, x + S(w96), y + S(h96), S(10), S(10));
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(edge);
    DeleteObject(fill);

    SelectObject(dc, g_bold);
    text(dc, x + S(COL_LABEL), y + S(7), title, p->accent);
    SelectObject(dc, g_font);
}

/* смесь двух цветов: t из 256 даёт долю b */
static COLORREF mix(COLORREF a, COLORREF b, int t)
{
    return RGB((GetRValue(a) * (256 - t) + GetRValue(b) * t) / 256, (GetGValue(a) * (256 - t) + GetGValue(b) * t) / 256,
               (GetBValue(a) * (256 - t) + GetBValue(b) * t) / 256);
}

void paint_card_accent(HDC dc, int dpi, int x96, int y96, int w96, int h96, const wchar_t *title, const wchar_t *badge)
{
    const ff_palette *p = theme_get();
    HBRUSH fill = CreateSolidBrush(mix(p->card, p->accent, p->dark ? 34 : 22));
    HBRUSH bar = CreateSolidBrush(p->accent);
    HPEN edge = CreatePen(PS_SOLID, 1, mix(p->border, p->accent, 140));
    HGDIOBJ ob = SelectObject(dc, fill);
    HGDIOBJ op = SelectObject(dc, edge);
    int x = S(x96);
    int y = S(y96);
    RECT stripe;
    RECT pill;
    SIZE sz;

    RoundRect(dc, x, y, x + S(w96), y + S(h96), S(10), S(10));
    SelectObject(dc, op);
    SelectObject(dc, ob);
    /* полоса слева внутри рамки */
    SetRect(&stripe, x + S(1), y + S(10), x + S(5), y + S(h96) - S(10));
    FillRect(dc, &stripe, bar);

    SelectObject(dc, g_bold);
    SetBkMode(dc, TRANSPARENT);
    text(dc, x + S(COL_LABEL), y + S(7), title, p->accent);
    GetTextExtentPoint32W(dc, title, (int)wcslen(title), &sz);
    SelectObject(dc, g_font);
    /* плашка после заголовка */
    GetTextExtentPoint32W(dc, badge, (int)wcslen(badge), &sz);
    {
        SIZE head;

        SelectObject(dc, g_bold);
        GetTextExtentPoint32W(dc, title, (int)wcslen(title), &head);
        SelectObject(dc, g_font);
        SetRect(&pill, x + S(COL_LABEL) + head.cx + S(10), y + S(8), x + S(COL_LABEL) + head.cx + S(10) + sz.cx + S(14),
                y + S(8) + sz.cy + S(2));
    }
    {
        HBRUSH pb = CreateSolidBrush(p->accent);
        HGDIOBJ obr = SelectObject(dc, pb);
        HGDIOBJ opn = SelectObject(dc, GetStockObject(NULL_PEN));

        RoundRect(dc, pill.left, pill.top, pill.right, pill.bottom, S(10), S(10));
        SelectObject(dc, opn);
        SelectObject(dc, obr);
        DeleteObject(pb);
    }
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextW(dc, badge, -1, &pill, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DeleteObject(edge);
    DeleteObject(fill);
    DeleteObject(bar);
}

void paint_draw_item(const DRAWITEMSTRUCT *d, HICON icon, const wchar_t *label, int dpi)
{
    const ff_palette *p = theme_get();
    int selected = (d->itemState & ODS_SELECTED) != 0;
    HBRUSH bg = CreateSolidBrush(selected ? p->accent : p->field);
    int size = S(16);
    int mid = (d->rcItem.top + d->rcItem.bottom) / 2;

    FillRect(d->hDC, &d->rcItem, bg);
    DeleteObject(bg);
    if (icon) {
        DrawIconEx(d->hDC, d->rcItem.left + S(6), mid - size / 2, icon, size, size, 0, NULL,
                   DI_NORMAL);
    }
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, selected ? RGB(255, 255, 255) : p->fg);
    SelectObject(d->hDC, g_font);
    TextOutW(d->hDC, d->rcItem.left + S(6) + size + S(8), mid - S(8), label, (int)wcslen(label));
    if (d->itemState & ODS_FOCUS && !(d->itemState & ODS_COMBOBOXEDIT)) {
        DrawFocusRect(d->hDC, &d->rcItem);
    }
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
        paint_card(dc, dpi, MARGIN, card_y(i), CARD_W, card_h(i), ff_tr(CARDS[i].title));
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
    /* «О программе» справа в той же строке, круглая кнопка с вопросом стоит за подписью */
    {
        const wchar_t *label = ff_tr(S_ABOUT_LABEL);
        SIZE sz;

        GetTextExtentPoint32W(dc, label, (int)wcslen(label), &sz);
        text(dc, S(MARGIN + CARD_W - ABOUT_D - 8) - sz.cx, S(STATUS_Y), label, p->muted);
    }
    EndPaint(h, &ps);
}

void paint_about_button(const DRAWITEMSTRUCT *d)
{
    const ff_palette *p = theme_get();
    HBRUSH fill = CreateSolidBrush((d->itemState & ODS_SELECTED) ? mix(p->accent, RGB(0, 0, 0), 60) : p->accent);
    HGDIOBJ ob;
    HGDIOBJ op;
    RECT r = d->rcItem;

    FillRect(d->hDC, &r, g_bg);
    ob = SelectObject(d->hDC, fill);
    op = SelectObject(d->hDC, GetStockObject(NULL_PEN));
    Ellipse(d->hDC, r.left, r.top, r.right, r.bottom);
    SelectObject(d->hDC, op);
    SelectObject(d->hDC, ob);
    DeleteObject(fill);
    SelectObject(d->hDC, g_bold);
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, RGB(255, 255, 255));
    DrawTextW(d->hDC, L"?", 1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(d->hDC, g_font);
    if (d->itemState & ODS_FOCUS) {
        InflateRect(&r, 2, 2);
        DrawFocusRect(d->hDC, &r);
    }
}
