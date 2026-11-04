#include <windows.h>
#include <commdlg.h>
#include <string.h>

#include "ui.h"

/* общий диалог: путь в папке профилей по умолчанию; 1 при отказе пользователя */
static int pick(HWND owner, ff_app *a, int save, wchar_t path[MAX_PATH])
{
    OPENFILENAMEW o;
    wchar_t dir[MAX_PATH];

    app_profile_dir(a, dir);
    path[0] = L'\0';
    memset(&o, 0, sizeof o);
    o.lStructSize = sizeof o;
    o.hwndOwner = owner;
    o.lpstrFilter = L"JSON (*.json)\0*.json\0";
    o.lpstrFile = path;
    o.nMaxFile = MAX_PATH;
    o.lpstrInitialDir = dir;
    o.lpstrDefExt = L"json";
    o.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST
              | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    return (save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o)) ? 0 : 1;
}

int ui_profile_load_dialog(HWND owner, ff_app *a, wchar_t name[PROFILE_NAME_LEN])
{
    wchar_t path[MAX_PATH];
    int rc = pick(owner, a, 0, path);

    if (rc != 0) {
        return 1;
    }
    app_profile_basename(path, name);
    return app_profile_load(a, path);
}

int ui_profile_save_dialog(HWND owner, ff_app *a, wchar_t name[PROFILE_NAME_LEN])
{
    wchar_t path[MAX_PATH];
    int rc = pick(owner, a, 1, path);

    if (rc != 0) {
        return 1;
    }
    app_profile_basename(path, name);
    return app_profile_save(a, path);
}
