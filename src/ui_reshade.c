#define COBJMACROS
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <stdio.h>
#include <wchar.h>

#include "i18n.h"
#include "reshade_inst.h"
#include "ui.h"
#include "ui_state.h"

/* последняя папка трёх кнопок ReShade (установить, удалить, скачать): общая, хранится в ini как UTF-8 */
static void last_dir(wchar_t out[MAX_PATH])
{
    out[0] = L'\0';
    if (ui.app->cfg.reshade_dir[0]
        && MultiByteToWideChar(CP_UTF8, 0, ui.app->cfg.reshade_dir, -1, out, MAX_PATH) == 0) {
        out[0] = L'\0';
    }
}

static void remember_dir(const wchar_t *dir)
{
    char utf8[FF_PATH_MAX];

    if (!dir[0] || WideCharToMultiByte(CP_UTF8, 0, dir, -1, utf8, sizeof utf8, NULL, NULL) == 0) {
        return;
    }
    if (strcmp(utf8, ui.app->cfg.reshade_dir) != 0) {
        strcpy(ui.app->cfg.reshade_dir, utf8);
        app_save(ui.app);
    }
}

/* папка файла без хвостового разделителя */
static void parent_dir(const wchar_t *path, wchar_t out[MAX_PATH])
{
    wchar_t *slash;

    wcsncpy(out, path, MAX_PATH - 1);
    out[MAX_PATH - 1] = L'\0';
    slash = wcsrchr(out, L'\\');
    if (slash) {
        *slash = L'\0';
    } else {
        out[0] = L'\0';
    }
}

/* диалог выбора exe игры, открывается там, где пользователь был в прошлый раз; путь в path, 0 при отмене */
static int pick_game(HWND owner, wchar_t path[MAX_PATH])
{
    OPENFILENAMEW o;
    wchar_t start[MAX_PATH];

    last_dir(start);
    memset(&o, 0, sizeof o);
    path[0] = L'\0';
    o.lStructSize = sizeof o;
    o.hwndOwner = owner;
    o.lpstrFilter = L"*.exe\0*.exe\0";
    o.lpstrFile = path;
    o.nMaxFile = MAX_PATH;
    o.lpstrInitialDir = start[0] ? start : NULL;
    o.lpstrTitle = ff_tr(S_RS_PICK);
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&o)) {
        return 0;
    }
    /* папка запоминается сразу после выбора: даже если установка не удастся, в следующий раз диалог откроется тут же */
    parent_dir(path, start);
    remember_dir(start);
    return 1;
}

/* диалог выбора папки с кнопкой «Сохранить» вместо «Выбор папки»; 0 при отмене */
static int pick_folder(HWND owner, wchar_t out[MAX_PATH])
{
    IFileOpenDialog *d = NULL;
    wchar_t start[MAX_PATH];
    int rc = 0;
    /* COM в этом потоке уже мог быть включён другим режимом: тогда он просто используется как есть */
    int co = SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED));

    last_dir(start);
    if (SUCCEEDED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog,
                                   (void **)&d))) {
        DWORD opts = 0;

        IFileOpenDialog_GetOptions(d, &opts);
        IFileOpenDialog_SetOptions(d, opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        IFileOpenDialog_SetTitle(d, ff_tr(S_RS_EXPORT_TITLE));
        IFileOpenDialog_SetOkButtonLabel(d, ff_tr(S_RS_EXPORT_BTN));
        if (start[0]) {
            IShellItem *item = NULL;

            if (SUCCEEDED(SHCreateItemFromParsingName(start, NULL, &IID_IShellItem, (void **)&item))) {
                IFileOpenDialog_SetFolder(d, item);
                IShellItem_Release(item);
            }
        }
        if (SUCCEEDED(IFileOpenDialog_Show(d, owner))) {
            IShellItem *res = NULL;

            if (SUCCEEDED(IFileOpenDialog_GetResult(d, &res))) {
                PWSTR path = NULL;

                if (SUCCEEDED(IShellItem_GetDisplayName(res, SIGDN_FILESYSPATH, &path))) {
                    wcsncpy(out, path, MAX_PATH - 1);
                    out[MAX_PATH - 1] = L'\0';
                    CoTaskMemFree(path);
                    rc = 1;
                }
                IShellItem_Release(res);
            }
        }
        IFileOpenDialog_Release(d);
    }
    if (co) {
        CoUninitialize();
    }
    return rc;
}

static void say(HWND owner, UINT icon, const wchar_t *fmt, const wchar_t *a, const wchar_t *b)
{
    wchar_t text[2 * MAX_PATH + 512];

    _snwprintf(text, sizeof text / sizeof text[0], fmt, a, b);
    text[sizeof text / sizeof text[0] - 1] = L'\0';
    MessageBoxW(owner, text, L"fringefix", icon | MB_TOPMOST);
}

void ui_reshade_install(HWND owner)
{
    wchar_t exe[MAX_PATH];
    wchar_t addon[MAX_PATH];
    wchar_t effect[MAX_PATH];
    int rc;

    if (!pick_game(owner, exe)) {
        return;
    }
    /* ReShade не ставится отсюда: без него аддон никто не загрузит, поэтому спрашиваем, прежде чем писать файлы */
    if (!rs_find_reshade(exe)
        && MessageBoxW(owner, ff_tr(S_RS_NO_RESHADE), L"fringefix", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST) != IDYES) {
        return;
    }
    rc = rs_install(ui.app->inst, exe, addon, effect);
    if (rc == RS_OK) {
        say(owner, MB_ICONINFORMATION, ff_tr(S_RS_DONE), addon, effect);
    } else if (rc == RS_ERR_RESOURCE) {
        MessageBoxW(owner, ff_tr(S_RS_NO_RES), L"fringefix", MB_ICONERROR | MB_TOPMOST);
    } else if (rc == RS_ERR_EXE) {
        MessageBoxW(owner, ff_tr(S_RS_BAD_PATH), L"fringefix", MB_ICONERROR | MB_TOPMOST);
    } else {
        say(owner, MB_ICONERROR, ff_tr(S_RS_FAILED), addon, L"");
    }
}

void ui_reshade_remove(HWND owner)
{
    wchar_t exe[MAX_PATH];
    wchar_t count[16];
    int n;

    if (!pick_game(owner, exe)) {
        return;
    }
    n = rs_remove(exe);
    if (n <= 0) {
        MessageBoxW(owner, ff_tr(S_RS_NOTHING), L"fringefix", MB_ICONINFORMATION | MB_TOPMOST);
        return;
    }
    _snwprintf(count, 16, L"%d", n);
    say(owner, MB_ICONINFORMATION, ff_tr(S_RS_REMOVED), count, L"");
}

void ui_reshade_export(HWND owner)
{
    wchar_t dir[MAX_PATH];
    wchar_t addon[MAX_PATH];
    wchar_t effect[MAX_PATH];
    int rc;

    if (!pick_folder(owner, dir)) {
        return;
    }
    remember_dir(dir);
    rc = rs_export(ui.app->inst, dir, addon, effect);
    if (rc == RS_OK) {
        say(owner, MB_ICONINFORMATION, ff_tr(S_RS_EXPORT_DONE), addon, effect);
    } else if (rc == RS_ERR_RESOURCE) {
        MessageBoxW(owner, ff_tr(S_RS_NO_RES), L"fringefix", MB_ICONERROR | MB_TOPMOST);
    } else {
        say(owner, MB_ICONERROR, ff_tr(S_RS_EXPORT_FAILED), addon, L"");
    }
}
