#include <windows.h>

#include "i18n.h"
#include "output.h"
#include "pattern.h"

#define STEP 4

static const wchar_t CLASS_NAME[] = L"fringefix_pattern";
static HWND g_win;

/* линии толщиной в один пиксель с шагом STEP: на них каёмка видна лучше всего */
static void draw_lines(HDC dc, const RECT *r)
{
    HBRUSH white = (HBRUSH)GetStockObject(WHITE_BRUSH);
    int half = r->right / 2;
    RECT line;
    int i;

    for (i = 0; i < half; i += STEP) {
        SetRect(&line, i, 0, i + 1, r->bottom);
        FillRect(dc, &line, white);
    }
    for (i = 0; i < r->bottom; i += STEP) {
        SetRect(&line, half, i, r->right, i + 1);
        FillRect(dc, &line, white);
    }
}

/* текст на чёрной плашке, чтобы линии его не портили */
static void draw_text(HDC dc, const RECT *r)
{
    HFONT font = CreateFontW(-MulDiv(14, GetDeviceCaps(dc, LOGPIXELSX), 72), 0, 0, 0, FW_NORMAL,
                             0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HGDIOBJ old = SelectObject(dc, font);
    RECT box = { r->right / 2 - 420, r->bottom / 2 - 120, r->right / 2 + 420, r->bottom / 2 + 120 };

    FillRect(dc, &box, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextW(dc, ff_tr(S_PATTERN_TEXT), -1, &box, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
    SelectObject(dc, old);
    DeleteObject(font);
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT r;

        GetClientRect(h, &r);
        FillRect(dc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
        draw_lines(dc, &r);
        draw_text(dc, &r);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_KEYDOWN:
        if (w == VK_ESCAPE) {
            DestroyWindow(h);
        }
        return 0;
    case WM_LBUTTONDOWN:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        g_win = NULL;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void pattern_show(HINSTANCE inst, int monitor)
{
    static int registered;
    RECT r;

    if (g_win) {
        SetForegroundWindow(g_win);
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
    if (ff_output_rect(monitor, &r) != 0) {
        SetRect(&r, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    }
    g_win = CreateWindowExW(0, CLASS_NAME, L"fringefix pattern", WS_POPUP | WS_VISIBLE, r.left,
                            r.top, r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
}
