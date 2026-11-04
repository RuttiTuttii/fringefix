#include <windows.h>
#include <wchar.h>

#include "fileio.h"

int ff_write_file(const wchar_t *path, const void *data, size_t len)
{
    wchar_t tmp[MAX_PATH];
    DWORD wrote = 0;
    HANDLE f;

    if (wcslen(path) + 5 >= MAX_PATH || len > 0xFFFFFFFFu) {
        return -1;
    }
    _snwprintf(tmp, MAX_PATH, L"%ls.tmp", path);
    tmp[MAX_PATH - 1] = L'\0';
    f = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        return -1;
    }
    /* размер ограничен буферами вызывающих, влезает в один WriteFile */
    if (!WriteFile(f, data, (DWORD)len, &wrote, NULL) || wrote != (DWORD)len || !FlushFileBuffers(f)) {
        CloseHandle(f);
        DeleteFileW(tmp);
        return -1;
    }
    CloseHandle(f);
    if (!MoveFileExW(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp);
        return -1;
    }
    return 0;
}
