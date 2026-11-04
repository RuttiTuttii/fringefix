#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "i18n.h"
#include "output.h"
#include "report_actions.h"
#include "stats_store.h"
#include "sysinfo.h"
#include "version.h"

int report_compose(const ff_app *a, const char *comment, int sys, int settings, int stats,
                   char *payload, size_t n)
{
    ff_report r;
    ff_output_info oi;

    memset(&r, 0, sizeof r);
    snprintf(r.app_version, sizeof r.app_version, "%s", FF_VERSION);
    snprintf(r.language, sizeof r.language, "%s", ff_i18n_get() == FF_LANG_RU ? "ru" : "en");
    r.windows_build = sys_windows_build();
    if (ff_output_info_get(a->cfg.monitor, &oi) == 0) {
        snprintf(r.gpu, sizeof r.gpu, "%s", oi.gpu);
        r.width = oi.width;
        r.height = oi.height;
        r.hdr = oi.hdr;
    }
    r.cfg = a->cfg;
    stats_store_snapshot(&r.stats);
    snprintf(r.comment, sizeof r.comment, "%s", comment);
    r.include_system = sys;
    r.include_settings = settings;
    r.include_stats = stats;
    return ff_report_format(&r, payload, n);
}

void report_read_comment(HWND edit, int max_chars, char out[FF_COMMENT_MAX + 1])
{
    wchar_t wc[512];
    int n;
    int bytes = 0;

    if (max_chars > 511) {
        max_chars = 511;
    }
    GetWindowTextW(edit, wc, max_chars + 1);
    n = (int)wcslen(wc);
    while (n > 0
           && (bytes = WideCharToMultiByte(CP_UTF8, 0, wc, n, out, FF_COMMENT_MAX, NULL, NULL)) == 0) {
        n--;
    }
    out[n > 0 ? bytes : 0] = '\0';
}

void report_preview_text(const char *payload, int len, wchar_t *out, size_t out_chars)
{
    static wchar_t wide[FF_REPORT_MAX];
    int n = MultiByteToWideChar(CP_UTF8, 0, payload, len, wide, FF_REPORT_MAX - 1);
    size_t o = 0;
    int i;

    for (i = 0; i < n && o + 3 < out_chars; i++) {
        if (wide[i] == L'\n') {
            out[o++] = L'\r';
        }
        out[o++] = wide[i];
    }
    out[o] = L'\0';
}

void report_copy(HWND owner, const wchar_t *text)
{
    size_t bytes = (wcslen(text) + 1) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    void *p;

    if (!mem) {
        return;
    }
    p = GlobalLock(mem);
    memcpy(p, text, bytes);
    GlobalUnlock(mem);
    if (OpenClipboard(owner)) {
        EmptyClipboard();
        SetClipboardData(CF_UNICODETEXT, mem);
        CloseClipboard();
    } else {
        GlobalFree(mem);
    }
}

int report_save_file(HWND owner, const char *payload, int len)
{
    wchar_t path[MAX_PATH] = L"fringefix-report.json";
    OPENFILENAMEW o;
    HANDLE f;
    DWORD wrote = 0;

    memset(&o, 0, sizeof o);
    o.lStructSize = sizeof o;
    o.hwndOwner = owner;
    o.lpstrFilter = L"JSON (*.json)\0*.json\0";
    o.lpstrFile = path;
    o.nMaxFile = MAX_PATH;
    o.lpstrDefExt = L"json";
    o.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&o)) {
        return 0;
    }
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        return 0;
    }
    WriteFile(f, payload, (DWORD)len, &wrote, NULL);
    CloseHandle(f);
    return wrote == (DWORD)len;
}

/* процентное кодирование UTF-8 для параметров ссылки: только буквы, цифры и -._~ остаются как есть */
static void url_encode(const wchar_t *src, char *out, size_t n)
{
    char utf8[512];
    size_t i;
    size_t o = 0;

    WideCharToMultiByte(CP_UTF8, 0, src, -1, utf8, sizeof utf8, NULL, NULL);
    utf8[sizeof utf8 - 1] = '\0';
    for (i = 0; utf8[i] && o + 4 < n; i++) {
        unsigned char c = (unsigned char)utf8[i];

        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-'
            || c == '.' || c == '_' || c == '~') {
            out[o++] = (char)c;
        } else {
            o += (size_t)snprintf(out + o, n - o, "%%%02X", c);
        }
    }
    out[o] = '\0';
}

void report_open_issue(HWND owner, const wchar_t *title, const wchar_t *body)
{
    char t[256];
    char b[512];
    char url[1024];
    wchar_t wurl[1024];

    url_encode(title, t, sizeof t);
    url_encode(body, b, sizeof b);
    snprintf(url, sizeof url, "https://github.com/%s/issues/new?title=%s&body=%s", FF_REPO, t, b);
    MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 1024);
    ShellExecuteW(owner, L"open", wurl, NULL, NULL, SW_SHOWNORMAL);
}
