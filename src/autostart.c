#include <windows.h>
#include <wchar.h>

#include "autostart.h"

#define RUN_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define RUN_VALUE L"fringefix"

/* путь к этому exe в кавычках: так Windows разбирает пробелы в пути */
static int quoted_exe(wchar_t out[MAX_PATH + 2])
{
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);

    if (n == 0 || n >= MAX_PATH) {
        return -1;
    }
    _snwprintf(out, MAX_PATH + 2, L"\"%ls\"", path);
    out[MAX_PATH + 1] = L'\0';
    return 0;
}

int autostart_get(void)
{
    wchar_t want[MAX_PATH + 2];
    wchar_t have[MAX_PATH + 2];
    DWORD size = sizeof have;
    DWORD type = 0;
    LONG rc;

    if (quoted_exe(want) != 0) {
        return 0;
    }
    rc = RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VALUE, RRF_RT_REG_SZ, &type, have, &size);
    /* exe переехал в другую папку: запись устарела, галочка снята, повторное включение её обновит */
    return rc == ERROR_SUCCESS && _wcsicmp(have, want) == 0;
}

int autostart_set(int on)
{
    HKEY key;
    LONG rc;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL)
        != ERROR_SUCCESS) {
        return -1;
    }
    if (on) {
        wchar_t want[MAX_PATH + 2];

        if (quoted_exe(want) != 0) {
            RegCloseKey(key);
            return -1;
        }
        rc = RegSetValueExW(key, RUN_VALUE, 0, REG_SZ, (const BYTE *)want,
                            (DWORD)((wcslen(want) + 1) * sizeof(wchar_t)));
    } else {
        rc = RegDeleteValueW(key, RUN_VALUE);
        /* записи и не было: выключено, как и просили */
        if (rc == ERROR_FILE_NOT_FOUND) {
            rc = ERROR_SUCCESS;
        }
    }
    RegCloseKey(key);
    return rc == ERROR_SUCCESS ? 0 : -1;
}
