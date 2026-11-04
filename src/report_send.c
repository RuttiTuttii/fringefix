#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "http.h"
#include "report_send.h"

#define REPLY_MAX 4096

typedef struct {
    HWND notify;
    char url[256];
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

static DWORD WINAPI send_thread(LPVOID arg)
{
    send_job *job = arg;
    ff_report_result *res = calloc(1, sizeof *res);
    unsigned char *reply = NULL;
    size_t reply_len = 0;

    if (res) {
        res->ok = ff_http_post_json(job->url, job->body, job->len, REPLY_MAX, &reply, &reply_len,
                                    res->msg, sizeof res->msg) == 0;
        if (res->ok) {
            read_id((const char *)reply, res->id, sizeof res->id);
        }
        free(reply);
        if (!PostMessageW(job->notify, WM_APP_REPORT_DONE, 0, (LPARAM)res)) {
            free(res);
        }
    }
    free(job->body);
    free(job);
    return 0;
}

void report_send_async(HWND notify, const char *url, const char *body, size_t len)
{
    send_job *job = calloc(1, sizeof *job);
    HANDLE t;

    if (!job) {
        return;
    }
    job->body = malloc(len + 1);
    if (!job->body || snprintf(job->url, sizeof job->url, "%s", url) >= (int)sizeof job->url) {
        free(job->body);
        free(job);
        return;
    }
    memcpy(job->body, body, len);
    job->body[len] = '\0';
    job->len = len;
    job->notify = notify;
    t = CreateThread(NULL, 0, send_thread, job, 0, NULL);
    if (!t) {
        free(job->body);
        free(job);
        return;
    }
    CloseHandle(t);
}
