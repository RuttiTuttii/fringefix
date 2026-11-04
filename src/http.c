#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winhttp.h>

#include "http.h"
#include "version.h"

#define URL_MAX 512
#define HOST_MAX 256
#define PATH_MAX_W 768
#define READ_STEP 65536

#define FAIL(what)                                                                  \
    do {                                                                            \
        snprintf(err, err_len, "%s failed (%lu)", what, (unsigned long)GetLastError()); \
        goto done;                                                                  \
    } while (0)

int ff_http_get(const char *url, size_t max, unsigned char **out, size_t *out_len, char *err,
                size_t err_len)
{
    wchar_t wurl[URL_MAX];
    wchar_t wagent[64];
    wchar_t host[HOST_MAX];
    wchar_t path[PATH_MAX_W];
    URL_COMPONENTS uc;
    HINTERNET ses = NULL;
    HINTERNET con = NULL;
    HINTERNET req = NULL;
    unsigned char *buf = NULL;
    size_t len = 0;
    size_t cap = 0;
    DWORD status = 0;
    DWORD status_len = sizeof status;
    int rc = -1;

    *out = NULL;
    *out_len = 0;
    if (strpbrk(url, "?#") || MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, URL_MAX) == 0) {
        snprintf(err, err_len, "bad url");
        return -1;
    }
    _snwprintf(wagent, 64, L"fringefix/" L"" FF_VERSION);
    wagent[63] = L'\0';

    memset(&uc, 0, sizeof uc);
    uc.dwStructSize = sizeof uc;
    uc.lpszHostName = host;
    uc.dwHostNameLength = HOST_MAX;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = PATH_MAX_W;
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS) {
        snprintf(err, err_len, "only https urls are allowed");
        return -1;
    }

    ses = WinHttpOpen(wagent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) {
        FAIL("WinHttpOpen");
    }
    WinHttpSetTimeouts(ses, 10000, 10000, 15000, 30000);
    con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) {
        FAIL("WinHttpConnect");
    }
    req = WinHttpOpenRequest(con, L"GET", path, NULL, WINHTTP_NO_REFERER,
                             WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!req) {
        FAIL("WinHttpOpenRequest");
    }
    if (!WinHttpSendRequest(req, L"Accept: application/vnd.github+json\r\n", (DWORD)-1L,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        || !WinHttpReceiveResponse(req, NULL)) {
        FAIL("request");
    }
    if (!WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_len,
                             WINHTTP_NO_HEADER_INDEX)) {
        FAIL("WinHttpQueryHeaders");
    }
    if (status != 200) {
        snprintf(err, err_len, status == 404
                                   ? "HTTP 404, repository is private or has no releases"
                                   : "HTTP %lu", (unsigned long)status);
        goto done;
    }

    for (;;) {
        DWORD avail = 0;
        DWORD got = 0;
        size_t need;

        if (!WinHttpQueryDataAvailable(req, &avail)) {
            FAIL("WinHttpQueryDataAvailable");
        }
        if (avail == 0) {
            break;
        }
        if (len > max || avail > max - len) {
            snprintf(err, err_len, "response is larger than %lu bytes", (unsigned long)max);
            goto done;
        }
        need = len + avail + 1;
        if (need > cap) {
            unsigned char *grown = realloc(buf, need + READ_STEP);

            if (!grown) {
                snprintf(err, err_len, "out of memory");
                goto done;
            }
            buf = grown;
            cap = need + READ_STEP;
        }
        if (!WinHttpReadData(req, buf + len, avail, &got)) {
            FAIL("WinHttpReadData");
        }
        len += got;
    }
    if (!buf) {
        buf = calloc(1, 1);
        if (!buf) {
            snprintf(err, err_len, "out of memory");
            goto done;
        }
    }
    buf[len] = '\0';
    *out = buf;
    *out_len = len;
    buf = NULL;
    rc = 0;

done:
    free(buf);
    if (req) {
        WinHttpCloseHandle(req);
    }
    if (con) {
        WinHttpCloseHandle(con);
    }
    if (ses) {
        WinHttpCloseHandle(ses);
    }
    return rc;
}
