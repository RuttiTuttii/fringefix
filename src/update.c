#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "http.h"
#include "update.h"
#include "version.h"

#define API_MAX (1 << 20)

typedef struct {
    HWND notify;
    int manual;
    char repo[96];
    ff_update_info info;
} job_t;

/* сравнивает текущую версию с последним релизом */
static void fetch_release(const char *repo, ff_update_info *info)
{
    char url[160];
    unsigned char *body = NULL;
    size_t len = 0;
    int cur[3];
    int rem[3];
    int rc;

    if (ff_version_parse(FF_VERSION, cur) != 0) {
        snprintf(info->err, sizeof info->err, "dev build, updates are disabled");
        return;
    }
    if (snprintf(url, sizeof url, "https://api.github.com/repos/%s/releases/latest", repo)
        >= (int)sizeof url) {
        snprintf(info->err, sizeof info->err, "repository name is too long");
        return;
    }
    if (ff_http_get(url, API_MAX, &body, &len, info->err, sizeof info->err) != 0) {
        return;
    }
    rc = ff_release_parse((const char *)body, repo, &info->rel);
    free(body);
    if (rc != 0) {
        snprintf(info->err, sizeof info->err,
                 "latest release has no fringefix.exe and fringefix.exe.sha256");
        return;
    }
    if (ff_version_parse(info->rel.tag, rem) != 0) {
        snprintf(info->err, sizeof info->err, "release tag %s is not vX.Y.Z", info->rel.tag);
        return;
    }
    info->newer = ff_version_cmp(rem, cur) > 0;
}

/* релизы по одной странице (30 по умолчанию у GitHub), этого хватает на откат на несколько версий назад */
static void fetch_list(const char *repo, ff_release_list *list)
{
    char url[160];
    unsigned char *body = NULL;
    size_t len = 0;
    int n;

    /* без ?per_page: ff_http_get отвергает адреса с ? и #, а страница GitHub по умолчанию и так на 30 релизов */
    if (snprintf(url, sizeof url, "https://api.github.com/repos/%s/releases", repo) >= (int)sizeof url) {
        snprintf(list->err, sizeof list->err, "repository name is too long");
        return;
    }
    if (ff_http_get(url, API_MAX, &body, &len, list->err, sizeof list->err) != 0) {
        return;
    }
    n = ff_release_list_parse((const char *)body, repo, list->rel, FF_RELEASES_MAX);
    free(body);
    if (n < 0) {
        snprintf(list->err, sizeof list->err, "bad repository name");
    } else if (n == 0) {
        snprintf(list->err, sizeof list->err, "no releases with fringefix.exe and its sha256");
    }
    list->count = n > 0 ? n : 0;
}

static DWORD WINAPI list_thread(LPVOID arg)
{
    job_t *job = arg;
    ff_release_list *list = calloc(1, sizeof *list);

    if (list) {
        fetch_list(job->repo, list);
        if (!PostMessageW(job->notify, WM_APP_RELEASE_LIST, 0, (LPARAM)list)) {
            free(list);
        }
    }
    free(job);
    return 0;
}

void update_list_async(HWND notify, const char *repo)
{
    job_t *job = calloc(1, sizeof *job);
    HANDLE t;

    if (!job) {
        return;
    }
    job->notify = notify;
    snprintf(job->repo, sizeof job->repo, "%s", repo);
    t = CreateThread(NULL, 0, list_thread, job, 0, NULL);
    if (!t) {
        free(job);
        return;
    }
    CloseHandle(t);
}

static DWORD WINAPI check_thread(LPVOID arg)
{
    job_t *job = arg;
    ff_update_info *info = calloc(1, sizeof *info);

    if (info) {
        info->manual = job->manual;
        fetch_release(job->repo, info);
        if (!PostMessageW(job->notify, WM_APP_UPDATE_INFO, 0, (LPARAM)info)) {
            free(info);
        }
    }
    free(job);
    return 0;
}

void update_check_async(HWND notify, const char *repo, int manual)
{
    job_t *job = calloc(1, sizeof *job);
    HANDLE t;

    if (!job) {
        return;
    }
    job->notify = notify;
    job->manual = manual;
    snprintf(job->repo, sizeof job->repo, "%s", repo);
    t = CreateThread(NULL, 0, check_thread, job, 0, NULL);
    if (!t) {
        free(job);
        return;
    }
    CloseHandle(t);
}
