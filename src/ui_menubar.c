#include <windows.h>
#include <wchar.h>

#include "i18n.h"
#include "pattern.h"
#include "theme.h"
#include "ui.h"
#include "ui_layout.h"
#include "ui_menubar.h"
#include "ui_paint.h"
#include "ui_state.h"

#define NITEMS 4
#define PAD_X 12

static const str_id TITLES[NITEMS] = { S_M_FILE, S_M_EDIT, S_M_TOOLS, S_M_HELP };

/* прямоугольники заголовков в пикселях окна, пересчитываются при каждой отрисовке */
static RECT g_rect[NITEMS];
static int g_hover = -1;
static int g_open = -1;

static void layout(HDC dc, int dpi)
{
    int x = MulDiv(MARGIN - 4, dpi, 96);
    int i;

    SelectObject(dc, paint_font());
    for (i = 0; i < NITEMS; i++) {
        const wchar_t *t = ff_tr(TITLES[i]);
        SIZE sz;

        GetTextExtentPoint32W(dc, t, (int)wcslen(t), &sz);
        g_rect[i].left = x;
        g_rect[i].right = x + sz.cx + 2 * MulDiv(PAD_X, dpi, 96);
        g_rect[i].top = MulDiv(2, dpi, 96);
        g_rect[i].bottom = MulDiv(MENU_H, dpi, 96) - MulDiv(2, dpi, 96);
        x = g_rect[i].right;
    }
}

void menubar_paint(HDC dc, int dpi, int width)
{
    const ff_palette *p = theme_get();
    RECT strip;
    HBRUSH fill = CreateSolidBrush(p->card);
    HBRUSH hot = CreateSolidBrush(p->border);
    HPEN edge = CreatePen(PS_SOLID, 1, p->border);
    HGDIOBJ old_pen;
    int h = MulDiv(MENU_H, dpi, 96);
    int i;

    SetRect(&strip, 0, 0, width, h);
    FillRect(dc, &strip, fill);
    old_pen = SelectObject(dc, edge);
    MoveToEx(dc, 0, h - 1, NULL);
    LineTo(dc, width, h - 1);
    SelectObject(dc, old_pen);

    layout(dc, dpi);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, p->fg);
    for (i = 0; i < NITEMS; i++) {
        const wchar_t *t = ff_tr(TITLES[i]);
        RECT r = g_rect[i];

        if (i == g_hover || i == g_open) {
            HGDIOBJ ob = SelectObject(dc, hot);
            HGDIOBJ op = SelectObject(dc, GetStockObject(NULL_PEN));

            RoundRect(dc, r.left, r.top, r.right, r.bottom, MulDiv(6, dpi, 96), MulDiv(6, dpi, 96));
            SelectObject(dc, op);
            SelectObject(dc, ob);
        }
        DrawTextW(dc, t, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    DeleteObject(fill);
    DeleteObject(hot);
    DeleteObject(edge);
}

int menubar_hit(int x, int y)
{
    POINT pt = { x, y };
    int i;

    for (i = 0; i < NITEMS; i++) {
        if (PtInRect(&g_rect[i], pt)) {
            return i;
        }
    }
    return -1;
}

static void redraw(HWND h)
{
    RECT r;

    GetClientRect(h, &r);
    r.bottom = MulDiv(MENU_H, ui.dpi, 96);
    InvalidateRect(h, &r, FALSE);
}

void menubar_hover(HWND h, int item)
{
    if (item != g_hover) {
        g_hover = item;
        redraw(h);
    }
}

static void add(HMENU m, UINT id, str_id text, int checked)
{
    AppendMenuW(m, MF_STRING | (checked ? MF_CHECKED : 0), id, ff_tr(text));
}

static void sep(HMENU m)
{
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
}

static HMENU build(int item)
{
    HMENU m = CreatePopupMenu();
    const ff_config *c = &ui.app->cfg;

    if (item == 0) {
        add(m, ID_PROF_LOAD, S_M_LOAD, 0);
        add(m, ID_PROF_SAVE, S_M_SAVEPROF, 0);
        sep(m);
        add(m, ID_SAVE, S_M_SAVECFG, 0);
        add(m, ID_M_OPENDIR, S_M_OPENDIR, 0);
        sep(m);
        add(m, ID_M_HIDE, S_M_HIDE, 0);
        add(m, ID_M_QUIT, S_MENU_QUIT, 0);
    } else if (item == 1) {
        add(m, ID_M_FILTER, S_M_FILTER, ff_config_view_const(c, c->monitor)->enabled);
        add(m, ID_M_MASTER, S_M_MASTER, ui.app->enabled != 0);
        sep(m);
        add(m, ID_RESET, S_M_RESET, 0);
    } else if (item == 2) {
        add(m, ID_PATTERN, S_MENU_PATTERN, pattern_is_open());
        add(m, ID_M_APPS, S_BTN_APP_LISTS, 0);
        add(m, ID_M_DIAG, S_BTN_DIAG, 0);
        sep(m);
        add(m, ID_PROGRAM, S_M_PROGRAM, 0);
        add(m, ID_M_RESHADE, S_M_RESHADE, 0);
    } else {
        add(m, ID_M_CHANGES, S_M_CHANGES, 0);
        add(m, ID_M_UPDATE, S_MENU_UPDATES, 0);
        add(m, ID_M_VERSIONS, S_BTN_VERSIONS, 0);
        add(m, ID_M_REPORT, S_BTN_REPORT, 0);
        sep(m);
        add(m, ID_M_ABOUT, S_M_ABOUT, 0);
    }
    return m;
}

void menubar_open(HWND h, int item)
{
    HMENU m;
    POINT pt;
    int cmd;

    if (item < 0 || item >= NITEMS) {
        return;
    }
    m = build(item);
    pt.x = g_rect[item].left;
    pt.y = g_rect[item].bottom + MulDiv(2, ui.dpi, 96);
    ClientToScreen(h, &pt);
    g_open = item;
    redraw(h);
    cmd = (int)TrackPopupMenuEx(m, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, h, NULL);
    DestroyMenu(m);
    g_open = -1;
    g_hover = -1;
    redraw(h);
    if (cmd) {
        PostMessageW(h, WM_COMMAND, (WPARAM)cmd, 0);
    }
}
