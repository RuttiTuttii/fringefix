#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "http.h"
#include "report_send.h"
#include "urllist.h"

#define REPLY_MAX 4096

typedef struct {
    HWND notify;
    char urls[FF_URLS_MAX * FF_URL_ITEM];
    int reverse;
    char *body;
    size_t len;
} send_job;

/* из ответа {"id": "..."} достаётся только строка id; всё остальное в ответе игнорируется */
static void read_id(const char *reply, char *id, size_t n)
{
    const char *p = strstr(reply, "\"id\"");
    size_t i = 0;

    id[0] = '\0';
    if (!p || !(p = strchr(p + 4, ':')) || !(p = strchr(p, '"'))) {
        return;
    }
    for (p++; *p && *p != '"' && i + 1 < n; p++) {
        id[i++] = isalnum((unsigned char)*p) || *p == '-' || *p == '_' ? *p : '_';
    }
    id[i] = '\0';
}

/* ошибка клиента (4xx, кроме 429) повторится на любом сервере, перебирать дальше незачем */
static int is_client_error(const char *msg)
{
    return strncmp(msg, "HTTP 4", 6) == 0 && strncmp(msg, "HTTP 429", 8) != 0;
}

/* пробует адреса по очереди до первого успеха; в res остаётся результат последней попытки */
static void send_to_any(const send_job *job, ff_report_result *res)
{
    char urls[FF_URLS_MAX][FF_URL_ITEM];
    int n = ff_urllist_split(job->urls, urls, FF_URLS_MAX, job->reverse);
    int i;

    snprintf(res->msg, sizeof res->msg, "no receiving address");
    for (i = 0; i < n; i++) {
        unsigned char *reply = NULL;
        size_t reply_len = 0;

        res->ok = ff_http_post_json(urls[i], job->body, job->len, REPLY_MAX, &reply, &reply_len,
                                    res->msg, sizeof res->msg) == 0;
        if (res->ok) {
            read_id((const char *)reply, res->id, sizeof res->id);
        }
        free(reply);
        if (res->ok || is_client_error(res->msg)) {
            return;
        }
    }
}

static DWORD WINAPI send_thread(LPVOID arg)
{
    send_job *job = arg;
    ff_report_result *res = calloc(1, sizeof *res);

    if (res) {
        send_to_any(job, res);
        if (!PostMessageW(job->notify, WM_APP_REPORT_DONE, 0, (LPARAM)res)) {
            free(res);
        }
    }
    free(job->body);
    free(job);
    return 0;
}

void report_send_async(HWND notify, const char *urls, const char *body, size_t len, int reverse)
{
    send_job *job = calloc(1, sizeof *job);
    HANDLE t;

    if (!job) {
        return;
    }
    job->body = malloc(len + 1);
    if (!job->body || snprintf(job->urls, sizeof job->urls, "%s", urls) >= (int)sizeof job->urls) {
        free(job->body);
        free(job);
        return;
    }
    memcpy(job->body, body, len);
    job->body[len] = '\0';
    job->len = len;
    job->reverse = reverse;
    job->notify = notify;
    t = CreateThread(NULL, 0, send_thread, job, 0, NULL);
    if (!t) {
        free(job->body);
        free(job);
        return;
    }
    CloseHandle(t);
}
