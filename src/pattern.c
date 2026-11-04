#include <windows.h>
#include <string.h>

#include "i18n.h"
#include "pattern.h"

/* толщина линий в физических пикселях: тонкая линия в один пиксель и есть проба субпикселей, поэтому dpi
 * её не меняет. шаг при 96 dpi, на другом масштабе умножается на dpi окна настроек */
#define THIN_W 1
#define THIN_STEP 14
#define THICK_W 5
#define THICK_STEP 10
#define BANDS 3

static const wchar_t CLASS_NAME[] = L"fringefix_pattern";
static HWND g_win;
static UINT g_dpi = 96;

/* линии толщиной w с шагом step в прямоугольнике r: горизонтальные идут сверху вниз, вертикальные слева направо */
static void draw_lines(HDC dc, const RECT *r, int horizontal, int w, int step, HBRUSH ink)
{
    RECT line;
    int i;

    if (step < w + 1) {
        step = w + 1;
    }
    if (horizontal) {
        for (i = r->top; i < r->bottom; i += step) {
            SetRect(&line, r->left, i, r->right, i + w < r->bottom ? i + w : r->bottom);
            FillRect(dc, &line, ink);
        }
    } else {
        for (i = r->left; i < r->right; i += step) {
            SetRect(&line, i, r->top, i + w < r->right ? i + w : r->right, r->bottom);
            FillRect(dc, &line, ink);
        }
    }
}

/* алфавит в нижней полосе; высота букв берётся от высоты полосы, чтобы четыре строки помещались при любом dpi */
static void draw_alphabet(HDC dc, const RECT *r, COLORREF color)
{
    int px = (r->bottom - r->top) / 5;
    HFONT font = CreateFontW(-px, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                             CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HGDIOBJ old = SelectObject(dc, font);
    RECT box = *r;

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, ff_tr(S_PATTERN_ALPHABET), -1, &box, DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dc, old);
    DeleteObject(font);
}

/* одна из двух частей: фон, две полосы линий (слева горизонтальные, справа вертикальные) и полоса букв */
static void draw_half(HDC dc, const RECT *half, int dark)
{
    HBRUSH paper = (HBRUSH)GetStockObject(dark ? BLACK_BRUSH : WHITE_BRUSH);
    HBRUSH ink = (HBRUSH)GetStockObject(dark ? WHITE_BRUSH : BLACK_BRUSH);
    int band = (half->bottom - half->top) / BANDS;
    int mid = half->left + (half->right - half->left) / 2;
    RECT r;

    FillRect(dc, half, paper);

    SetRect(&r, half->left, half->top, mid, half->top + band);
    draw_lines(dc, &r, 1, THIN_W, MulDiv(THIN_STEP, g_dpi, 96), ink);
    SetRect(&r, mid, half->top, half->right, half->top + band);
    draw_lines(dc, &r, 0, THIN_W, MulDiv(THIN_STEP, g_dpi, 96), ink);

    SetRect(&r, half->left, half->top + band, mid, half->top + 2 * band);
    draw_lines(dc, &r, 1, THICK_W, MulDiv(THICK_STEP, g_dpi, 96), ink);
    SetRect(&r, mid, half->top + band, half->right, half->top + 2 * band);
    draw_lines(dc, &r, 0, THICK_W, MulDiv(THICK_STEP, g_dpi, 96), ink);

    SetRect(&r, half->left, half->top + 2 * band, half->right, half->bottom);
    draw_alphabet(dc, &r, dark ? RGB(255, 255, 255) : RGB(0, 0, 0));
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT r;
        RECT half;

        GetClientRect(h, &r);
        SetRect(&half, r.left, r.top, r.right, r.top + r.bottom / 2);
        draw_half(dc, &half, 0);
        SetRect(&half, r.left, r.top + r.bottom / 2, r.right, r.bottom);
        draw_half(dc, &half, 1);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        g_win = NULL;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

/* прямоугольник паттерна для окна anchor: справа от него, а если на мониторе не помещается, то слева */
static void place(HWND anchor, RECT *out)
{
    RECT a;
    MONITORINFO mi;
    int w;

    GetWindowRect(anchor, &a);
    g_dpi = GetDpiForWindow(anchor);
    if (g_dpi == 0) {
        g_dpi = 96;
    }
    w = (a.right - a.left) / 2;
    memset(&mi, 0, sizeof mi);
    mi.cbSize = sizeof mi;
    GetMonitorInfoW(MonitorFromWindow(anchor, MONITOR_DEFAULTTONEAREST), &mi);
    if (a.right + w <= mi.rcMonitor.right) {
        SetRect(out, a.right, a.top, a.right + w, a.bottom);
    } else {
        SetRect(out, a.left - w, a.top, a.left, a.bottom);
    }
}

void pattern_close(void)
{
    if (g_win) {
        DestroyWindow(g_win);
    }
}

int pattern_is_open(void)
{
    return g_win != NULL;
}

void pattern_follow(HWND anchor)
{
    RECT r;

    if (!g_win) {
        return;
    }
    place(anchor, &r);
    SetWindowPos(g_win, HWND_TOPMOST, r.left, r.top, r.right - r.left, r.bottom - r.top,
                 SWP_NOACTIVATE);
    InvalidateRect(g_win, NULL, FALSE);
}

void pattern_show(HINSTANCE inst, HWND anchor)
{
    static int registered;
    RECT r;

    if (g_win) {
        pattern_follow(anchor);
        return;
    }
    if (!registered) {
        WNDCLASSW wc = { 0 };

        wc.lpfnWndProc = proc;
        wc.hInstance = inst;
        wc.lpszClassName = CLASS_NAME;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        RegisterClassW(&wc);
        registered = 1;
    }
    place(anchor, &r);
    /* не берёт фокус, чтобы окно настроек оставалось активным; поверх оверлея, как и окно настроек */
    g_win = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, CLASS_NAME,
                            L"fringefix pattern", WS_POPUP | WS_VISIBLE, r.left, r.top,
                            r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
}
