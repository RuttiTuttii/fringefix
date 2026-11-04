#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>

#include "slider.h"
#include "theme.h"

#define BIG_STEP 10

typedef struct {
    int lo;
    int hi;
    int pos;
    int drag;
} slider_t;

static slider_t *state(HWND h)
{
    return (slider_t *)GetWindowLongPtrW(h, GWLP_USERDATA);
}

/* радиус ползунка выводится из высоты окна, поэтому масштабируется вместе с dpi */
static int pad_of(HWND h)
{
    RECT r;

    GetClientRect(h, &r);
    return (r.bottom - r.top) / 3 + 2;
}

static int clamp_pos(const slider_t *s, int v)
{
    return v < s->lo ? s->lo : (v > s->hi ? s->hi : v);
}

static int pos_to_x(const slider_t *s, int width, int pad, int pos)
{
    if (s->hi == s->lo) {
        return pad;
    }
    return pad + (int)((long long)(pos - s->lo) * (width - 2 * pad) / (s->hi - s->lo));
}

static int x_to_pos(const slider_t *s, int width, int pad, int x)
{
    long long span = width - 2 * pad;
    long long v;

    if (span <= 0) {
        return s->lo;
    }
    /* округление к ближайшему шагу, а не вниз */
    v = ((long long)(x - pad) * (s->hi - s->lo) + span / 2) / span + s->lo;
    return clamp_pos(s, v < -1000000 ? -1000000 : (v > 1000000 ? 1000000 : (int)v));
}

static void set_pos(HWND h, slider_t *s, int pos, int notify)
{
    pos = clamp_pos(s, pos);
    if (pos == s->pos) {
        return;
    }
    s->pos = pos;
    InvalidateRect(h, NULL, FALSE);
    if (notify) {
        SendMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), FFN_CHANGED), (LPARAM)h);
    }
}

/* диапазон из двух положений работает как выключатель: клик переключает, а не ставит позицию под курсором */
static int is_switch(const slider_t *s)
{
    return s->hi - s->lo == 1;
}

static void fill_round(HDC dc, COLORREF color, int x0, int y0, int x1, int y1)
{
    HBRUSH b = CreateSolidBrush(color);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, GetStockObject(NULL_PEN));

    RoundRect(dc, x0, y0, x1 + 1, y1 + 1, y1 - y0, y1 - y0);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(b);
}

static void paint(HWND h, const slider_t *s)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    HDC mem = CreateCompatibleDC(dc);
    RECT r;
    HBITMAP bmp;
    HGDIOBJ old;
    HBRUSH bg;
    const ff_palette *p = theme_get();
    int w;
    int cy;
    int pad;
    int zero_x;
    int x;
    int thumb;
    int track;
    HBRUSH tb;
    HPEN ring;
    COLORREF thumb_color;
    HGDIOBJ obr;
    HGDIOBJ open;

    GetClientRect(h, &r);
    w = r.right;
    cy = r.bottom / 2;
    pad = pad_of(h);
    thumb = r.bottom / 3 + (GetFocus() == h ? 2 : 0);
    track = r.bottom / 14;
    bmp = CreateCompatibleBitmap(dc, r.right, r.bottom);
    old = SelectObject(mem, bmp);

    bg = CreateSolidBrush(p->card);
    FillRect(mem, &r, bg);
    DeleteObject(bg);

    /* дорожка, заливка от нуля до значения и отметка нуля */
    zero_x = pos_to_x(s, w, pad, clamp_pos(s, 0));
    x = pos_to_x(s, w, pad, s->pos);
    fill_round(mem, p->border, pad, cy - track, w - pad, cy + track);
    fill_round(mem, p->accent, zero_x < x ? zero_x : x, cy - track, zero_x < x ? x : zero_x, cy + track);
    if (s->lo < 0 && s->hi > 0) {
        HPEN tick = CreatePen(PS_SOLID, 1, p->muted);
        HGDIOBJ ot = SelectObject(mem, tick);

        MoveToEx(mem, zero_x, cy - thumb, NULL);
        LineTo(mem, zero_x, cy + thumb + 1);
        SelectObject(mem, ot);
        DeleteObject(tick);
    }

    /* ползунок: круг цвета акцента с кольцом цвета карточки */
    /* выключенный выключатель серый, чтобы положение читалось и без заливки дорожки */
    thumb_color = is_switch(s) && s->pos == s->lo ? p->muted : p->accent;
    tb = CreateSolidBrush(thumb_color);
    ring = CreatePen(PS_SOLID, 2, p->card);
    obr = SelectObject(mem, tb);
    open = SelectObject(mem, ring);
    Ellipse(mem, x - thumb, cy - thumb, x + thumb + 1, cy + thumb + 1);
    SelectObject(mem, open);
    SelectObject(mem, obr);
    DeleteObject(ring);
    DeleteObject(tb);

    BitBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(h, &ps);
}

static LRESULT on_key(HWND h, slider_t *s, WPARAM w)
{
    switch (w) {
    case VK_LEFT:
    case VK_DOWN:
        set_pos(h, s, s->pos - 1, 1);
        break;
    case VK_RIGHT:
    case VK_UP:
        set_pos(h, s, s->pos + 1, 1);
        break;
    case VK_PRIOR:
        set_pos(h, s, s->pos + BIG_STEP, 1);
        break;
    case VK_NEXT:
        set_pos(h, s, s->pos - BIG_STEP, 1);
        break;
    case VK_HOME:
        set_pos(h, s, s->lo, 1);
        break;
    case VK_END:
        set_pos(h, s, s->hi, 1);
        break;
    }
    return 0;
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    slider_t *s = state(h);
    RECT r;

    switch (m) {
    case WM_CREATE:
        s = calloc(1, sizeof *s);
        if (!s) {
            return -1;
        }
        s->lo = -100;
        s->hi = 100;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)s);
        return 0;
    case WM_NCDESTROY:
        free(s);
        SetWindowLongPtrW(h, GWLP_USERDATA, 0);
        break;
    case FFS_SETRANGE:
        s->lo = (int)w;
        s->hi = (int)l;
        s->pos = clamp_pos(s, s->pos);
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case FFS_SETPOS:
        s->pos = clamp_pos(s, (int)w);
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case FFS_GETPOS:
        return s->pos;
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint(h, s);
        return 0;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        SetFocus(h);
        if (is_switch(s)) {
            set_pos(h, s, s->pos == s->hi ? s->lo : s->hi, 1);
            return 0;
        }
        if (m == WM_LBUTTONDBLCLK) {
            set_pos(h, s, clamp_pos(s, 0), 1);
            return 0;
        }
        GetClientRect(h, &r);
        SetCapture(h);
        s->drag = 1;
        set_pos(h, s, x_to_pos(s, r.right, pad_of(h), GET_X_LPARAM(l)), 1);
        return 0;
    case WM_MOUSEMOVE:
        if (s->drag) {
            GetClientRect(h, &r);
            set_pos(h, s, x_to_pos(s, r.right, pad_of(h), GET_X_LPARAM(l)), 1);
        }
        return 0;
    case WM_LBUTTONUP:
        s->drag = 0;
        ReleaseCapture();
        return 0;
    case WM_MOUSEWHEEL:
        set_pos(h, s, s->pos + GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA * (GET_KEYSTATE_WPARAM(w) & MK_SHIFT ? BIG_STEP : 1), 1);
        return 0;
    case WM_KEYDOWN:
        return on_key(h, s, w);
    }
    return DefWindowProcW(h, m, w, l);
}

int slider_register(HINSTANCE inst)
{
    WNDCLASSW wc;

    memset(&wc, 0, sizeof wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.lpszClassName = FF_SLIDER_CLASS;
    return RegisterClassW(&wc) ? 0 : -1;
}
