#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "app.h"
#include "profile.h"

#define PROFILE_MAX 4096

void app_profile_dir(const ff_app *a, wchar_t out[MAX_PATH])
{
    wchar_t *slash;

    wcsncpy(out, a->ini, MAX_PATH - 1);
    out[MAX_PATH - 1] = L'\0';
    slash = wcsrchr(out, L'\\');
    if (!slash || (size_t)(slash - out) + 10 >= MAX_PATH) {
        wcscpy(out, L"profiles");
    } else {
        wcscpy(slash + 1, L"profiles");
    }
    CreateDirectoryW(out, NULL);
}

void app_profile_basename(const wchar_t *path, wchar_t out[PROFILE_NAME_LEN])
{
    const wchar_t *slash = wcsrchr(path, L'\\');
    const wchar_t *name = slash ? slash + 1 : path;
    wchar_t *dot;

    wcsncpy(out, name, PROFILE_NAME_LEN - 1);
    out[PROFILE_NAME_LEN - 1] = L'\0';
    dot = wcsrchr(out, L'.');
    if (dot) {
        *dot = L'\0';
    }
}

int app_profile_list(const ff_app *a, wchar_t names[][PROFILE_NAME_LEN], int max)
{
    wchar_t pattern[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    int n = 0;

    app_profile_dir(a, pattern);
    if (wcslen(pattern) + 8 >= MAX_PATH) {
        return 0;
    }
    wcscat(pattern, L"\\*.json");
    h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && n < max) {
            app_profile_basename(fd.cFileName, names[n++]);
        }
    } while (n < max && FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}

int app_profile_load(ff_app *a, const wchar_t *path)
{
    char buf[PROFILE_MAX];
    DWORD got = 0;
    ff_config next = a->cfg;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);

    if (f == INVALID_HANDLE_VALUE) {
        return -1;
    }
    if (!ReadFile(f, buf, sizeof buf - 1, &got, NULL)) {
        got = 0;
    }
    CloseHandle(f);
    buf[got] = '\0';
    /* разбор идёт в копию: чужой файл не должен наполовину перезаписать рабочие настройки */
    if (ff_profile_parse(buf, &next) < 0) {
        return -2;
    }
    memcpy(a->cfg.shift, next.shift, sizeof a->cfg.shift);
    a->cfg.sharpen = next.sharpen;
    app_apply(a);
    return 0;
}

int app_profile_save(ff_app *a, const wchar_t *path)
{
    char buf[PROFILE_MAX];
    DWORD wrote = 0;
    HANDLE f;
    int n = ff_profile_format(&a->cfg, buf, sizeof buf);

    if (n < 0) {
        return -1;
    }
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        return -1;
    }
    if (!WriteFile(f, buf, (DWORD)n, &wrote, NULL) || wrote != (DWORD)n) {
        CloseHandle(f);
        return -1;
    }
    CloseHandle(f);
    return 0;
}
