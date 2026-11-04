#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "http.h"
#include "sha256.h"
#include "update.h"

#define EXE_MAX (32 << 20)
#define SHA_MAX 4096

typedef struct {
    HWND notify;
    ff_update_info info;
} install_job;

/* первые 64 символа файла .sha256 должны быть hex; формат sha256sum "<hash>  <имя>" */
static int expected_hash(const unsigned char *text, size_t len, char out[65])
{
    size_t i;

    if (len < 64) {
        return -1;
    }
    for (i = 0; i < 64; i++) {
        if (!isxdigit(text[i])) {
            return -1;
        }
        out[i] = (char)tolower(text[i]);
    }
    out[64] = '\0';
    return 0;
}

static int write_file(const wchar_t *path, const unsigned char *data, size_t len)
{
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD wrote;

    if (f == INVALID_HANDLE_VALUE) {
        return -1;
    }
    /* размер ограничен EXE_MAX, влезает в один WriteFile */
    if (!WriteFile(f, data, (DWORD)len, &wrote, NULL) || wrote != (DWORD)len) {
        CloseHandle(f);
        DeleteFileW(path);
        return -1;
    }
    CloseHandle(f);
    return 0;
}

/* путь текущего exe и соседние .new и .old */
static int exe_paths(wchar_t path[MAX_PATH], wchar_t neu[MAX_PATH], wchar_t old[MAX_PATH])
{
    DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);

    if (n == 0 || n >= MAX_PATH - 5) {
        return -1;
    }
    _snwprintf(neu, MAX_PATH, L"%ls.new", path);
    _snwprintf(old, MAX_PATH, L"%ls.old", path);
    return 0;
}

/* работающий exe нельзя перезаписать, но можно переименовать: старый уходит в .old, новый встаёт на место */
static int swap_exe(const wchar_t *path, const wchar_t *neu, const wchar_t *old)
{
    DeleteFileW(old);
    if (!MoveFileExW(path, old, MOVEFILE_REPLACE_EXISTING)) {
        return -1;
    }
    if (!MoveFileExW(neu, path, MOVEFILE_REPLACE_EXISTING)) {
        MoveFileExW(old, path, MOVEFILE_REPLACE_EXISTING);
        return -1;
    }
    return 0;
}

static int install(const ff_update_info *info, ff_install_result *res)
{
    unsigned char *exe = NULL;
    unsigned char *sha = NULL;
    size_t exe_len = 0;
    size_t sha_len = 0;
    char want[65];
    char got[65];
    unsigned char digest[32];
    wchar_t neu[MAX_PATH];
    wchar_t old[MAX_PATH];
    int rc = -1;

    if (exe_paths(res->exe, neu, old) != 0) {
        snprintf(res->msg, sizeof res->msg, "cannot resolve exe path");
        return -1;
    }
    if (ff_http_get(info->rel.sha_url, SHA_MAX, &sha, &sha_len, res->msg, sizeof res->msg) != 0
        || ff_http_get(info->rel.exe_url, EXE_MAX, &exe, &exe_len, res->msg, sizeof res->msg) != 0) {
        goto done;
    }
    if (expected_hash(sha, sha_len, want) != 0) {
        snprintf(res->msg, sizeof res->msg, "sha256 file is malformed");
        goto done;
    }
    /* хеш и признак PE: битая загрузка или чужой файл не должен заменить рабочий exe */
    if (ff_sha256(exe, exe_len, digest) != 0) {
        snprintf(res->msg, sizeof res->msg, "download is too large");
        goto done;
    }
    ff_sha256_hex(digest, got);
    if (strcmp(got, want) != 0 || exe_len < 2 || exe[0] != 'M' || exe[1] != 'Z') {
        snprintf(res->msg, sizeof res->msg, "sha256 mismatch, update is not installed");
        goto done;
    }
    if (write_file(neu, exe, exe_len) != 0) {
        snprintf(res->msg, sizeof res->msg, "cannot write %s", "fringefix.exe.new");
        goto done;
    }
    if (swap_exe(res->exe, neu, old) != 0) {
        DeleteFileW(neu);
        snprintf(res->msg, sizeof res->msg, "cannot replace fringefix.exe (%lu)", GetLastError());
        goto done;
    }
    rc = 0;

done:
    free(exe);
    free(sha);
    return rc;
}

static DWORD WINAPI install_thread(LPVOID arg)
{
    install_job *job = arg;
    ff_install_result *res = calloc(1, sizeof *res);

    if (res) {
        res->ok = install(&job->info, res) == 0;
        if (!PostMessageW(job->notify, WM_APP_INSTALL_DONE, 0, (LPARAM)res)) {
            free(res);
        }
    }
    free(job);
    return 0;
}

void update_install_async(HWND notify, const ff_update_info *info)
{
    install_job *job = calloc(1, sizeof *job);
    HANDLE t;

    if (!job) {
        return;
    }
    job->notify = notify;
    job->info = *info;
    t = CreateThread(NULL, 0, install_thread, job, 0, NULL);
    if (!t) {
        free(job);
        return;
    }
    CloseHandle(t);
}

void update_cleanup(void)
{
    wchar_t path[MAX_PATH];
    wchar_t neu[MAX_PATH];
    wchar_t old[MAX_PATH];

    if (exe_paths(path, neu, old) == 0) {
        DeleteFileW(old);
        DeleteFileW(neu);
    }
}
