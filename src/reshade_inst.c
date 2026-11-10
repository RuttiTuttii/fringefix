#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "fileio.h"
#include "reshade_inst.h"
#include "resource.h"
#include "rspaths.h"

#define ADDON_FILE L"fringefix.addon64"
#define EFFECT_FILE L"fringefix.fx"
#define INI_NAME L"ReShade.ini"
#define INI_MAX (256 * 1024)
#define DEFAULT_EFFECTS L"reshade-shaders\\Shaders"
#define PROXY_MIN_BYTES (1024 * 1024)

/* dll-прокси, под видом которых ReShade подключается к игре */
static const wchar_t *const PROXIES[] = {
    L"dxgi.dll", L"d3d9.dll", L"d3d10.dll", L"d3d11.dll", L"d3d12.dll", L"opengl32.dll", L"dinput8.dll",
    L"ReShade64.dll",
};

/* папка exe без хвостового разделителя; -1, если пути нет */
static int exe_dir(const wchar_t *exe, wchar_t dir[MAX_PATH])
{
    wchar_t *slash;

    if (!exe || wcslen(exe) >= MAX_PATH) {
        return -1;
    }
    wcscpy(dir, exe);
    slash = wcsrchr(dir, L'\\');
    if (!slash) {
        return -1;
    }
    *slash = L'\0';
    return 0;
}

static void join(const wchar_t *dir, const wchar_t *name, wchar_t out[MAX_PATH])
{
    _snwprintf(out, MAX_PATH, L"%ls\\%ls", dir, name);
    out[MAX_PATH - 1] = L'\0';
}

static int file_size(const wchar_t *path, ULONGLONG *size)
{
    WIN32_FILE_ATTRIBUTE_DATA fa;

    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fa)) {
        return 0;
    }
    *size = ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
    return 1;
}

/* ReShade.ini игры целиком в UTF-8; NULL, если файла нет. освобождает вызывающий */
static char *read_ini(const wchar_t *dir)
{
    wchar_t path[MAX_PATH];
    HANDLE f;
    char *buf;
    DWORD got = 0;

    join(dir, INI_NAME, path);
    f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    buf = malloc(INI_MAX);
    if (!buf) {
        CloseHandle(f);
        return NULL;
    }
    if (!ReadFile(f, buf, INI_MAX - 1, &got, NULL)) {
        got = 0;
    }
    CloseHandle(f);
    buf[got] = '\0';
    return buf;
}

/* путь из ini (UTF-8, абсолютный или от папки игры) в полный путь; пустой путь даёт саму папку */
static void resolve(const wchar_t *dir, const char *path, wchar_t out[MAX_PATH])
{
    wchar_t w[MAX_PATH];

    w[0] = L'\0';
    if (path[0] && MultiByteToWideChar(CP_UTF8, 0, path, -1, w, MAX_PATH) == 0) {
        w[0] = L'\0';
    }
    /* ReShade.ini допускает прямые слеши, а папки создаются по обратным */
    for (wchar_t *c = w; *c; c++) {
        if (*c == L'/') {
            *c = L'\\';
        }
    }
    if (ff_rs_is_absolute(path)) {
        wcsncpy(out, w, MAX_PATH - 1);
        out[MAX_PATH - 1] = L'\0';
    } else {
        const wchar_t *rel = w;

        while (rel[0] == L'.' && (rel[1] == L'\\' || rel[1] == L'/')) {
            rel += 2;
        }
        if (rel[0] == L'\0' || (rel[0] == L'.' && rel[1] == L'\0')) {
            wcsncpy(out, dir, MAX_PATH - 1);
            out[MAX_PATH - 1] = L'\0';
        } else {
            join(dir, rel, out);
        }
    }
}

/* куда ReShade игры смотрит за аддонами и эффектами */
static void target_dirs(const wchar_t *dir, wchar_t addon_dir[MAX_PATH], wchar_t effect_dir[MAX_PATH])
{
    char *ini = read_ini(dir);
    char value[MAX_PATH * 2];
    char first[MAX_PATH * 2];

    value[0] = '\0';
    if (ini && ff_rs_ini_get(ini, "ADDON", "AddonPath", value, sizeof value) && value[0]) {
        resolve(dir, value, addon_dir);
    } else {
        wcscpy(addon_dir, dir);
    }
    value[0] = '\0';
    if (ini && ff_rs_ini_get(ini, "GENERAL", "EffectSearchPaths", value, sizeof value)
        && ff_rs_first_path(value, first, sizeof first) > 0) {
        resolve(dir, first, effect_dir);
    } else {
        join(dir, DEFAULT_EFFECTS, effect_dir);
    }
    free(ini);
}

/* создаёт папку вместе с недостающими родителями */
static void make_dirs(const wchar_t *path)
{
    wchar_t tmp[MAX_PATH];
    wchar_t *p;

    wcsncpy(tmp, path, MAX_PATH - 1);
    tmp[MAX_PATH - 1] = L'\0';
    for (p = tmp + 3; *p; p++) {
        if (*p == L'\\') {
            *p = L'\0';
            CreateDirectoryW(tmp, NULL);
            *p = L'\\';
        }
    }
    CreateDirectoryW(tmp, NULL);
}

static const void *resource(HINSTANCE inst, int id, DWORD *size)
{
    HRSRC r = FindResourceW(inst, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW((ULONG_PTR)RT_RCDATA));
    HGLOBAL g = r ? LoadResource(inst, r) : NULL;

    *size = r ? SizeofResource(inst, r) : 0;
    return g ? LockResource(g) : NULL;
}

int rs_find_reshade(const wchar_t *exe)
{
    wchar_t dir[MAX_PATH];
    wchar_t path[MAX_PATH];
    ULONGLONG size;
    size_t i;

    if (exe_dir(exe, dir) != 0) {
        return 0;
    }
    join(dir, INI_NAME, path);
    if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) {
        return 1;
    }
    for (i = 0; i < sizeof PROXIES / sizeof PROXIES[0]; i++) {
        join(dir, PROXIES[i], path);
        if (file_size(path, &size) && size >= PROXY_MIN_BYTES) {
            return 1;
        }
    }
    return 0;
}

int rs_install(HINSTANCE inst, const wchar_t *exe, wchar_t addon[MAX_PATH], wchar_t effect[MAX_PATH])
{
    wchar_t dir[MAX_PATH];
    wchar_t addon_dir[MAX_PATH];
    wchar_t effect_dir[MAX_PATH];
    const void *dll;
    const void *fx;
    DWORD dll_size;
    DWORD fx_size;

    addon[0] = effect[0] = L'\0';
    if (exe_dir(exe, dir) != 0) {
        return RS_ERR_EXE;
    }
    dll = resource(inst, IDR_ADDON, &dll_size);
    fx = resource(inst, IDR_ADDON_FX, &fx_size);
    if (!dll || !fx || dll_size == 0 || fx_size == 0) {
        return RS_ERR_RESOURCE;
    }
    target_dirs(dir, addon_dir, effect_dir);
    make_dirs(addon_dir);
    make_dirs(effect_dir);
    join(addon_dir, ADDON_FILE, addon);
    join(effect_dir, EFFECT_FILE, effect);
    /* сначала аддон: чаще всего не пишется именно он, потому что игра держит dll, и тогда на месте остаётся
     * прежняя пара, а не новый эффект со старым аддоном. оба файла пишутся через временный и переименование */
    if (ff_write_file(addon, dll, dll_size) != 0) {
        return RS_ERR_ADDON;
    }
    if (ff_write_file(effect, fx, fx_size) != 0) {
        wcscpy(addon, effect);
        return RS_ERR_EFFECT;
    }
    return RS_OK;
}

int rs_export(HINSTANCE inst, const wchar_t *dir, wchar_t addon[MAX_PATH], wchar_t effect[MAX_PATH])
{
    wchar_t effect_dir[MAX_PATH];
    const void *dll;
    const void *fx;
    DWORD dll_size;
    DWORD fx_size;

    addon[0] = effect[0] = L'\0';
    if (!dir || dir[0] == L'\0' || wcslen(dir) + wcslen(DEFAULT_EFFECTS) + wcslen(EFFECT_FILE) + 4 >= MAX_PATH) {
        return RS_ERR_EXE;
    }
    dll = resource(inst, IDR_ADDON, &dll_size);
    fx = resource(inst, IDR_ADDON_FX, &fx_size);
    if (!dll || !fx || dll_size == 0 || fx_size == 0) {
        return RS_ERR_RESOURCE;
    }
    join(dir, DEFAULT_EFFECTS, effect_dir);
    make_dirs(effect_dir);
    join(dir, ADDON_FILE, addon);
    join(effect_dir, EFFECT_FILE, effect);
    if (ff_write_file(addon, dll, dll_size) != 0) {
        return RS_ERR_ADDON;
    }
    if (ff_write_file(effect, fx, fx_size) != 0) {
        wcscpy(addon, effect);
        return RS_ERR_EFFECT;
    }
    return RS_OK;
}

int rs_remove(const wchar_t *exe)
{
    wchar_t dir[MAX_PATH];
    wchar_t addon_dir[MAX_PATH];
    wchar_t effect_dir[MAX_PATH];
    wchar_t path[MAX_PATH];
    int removed = 0;

    if (exe_dir(exe, dir) != 0) {
        return -1;
    }
    target_dirs(dir, addon_dir, effect_dir);
    join(addon_dir, ADDON_FILE, path);
    removed += DeleteFileW(path) ? 1 : 0;
    join(effect_dir, EFFECT_FILE, path);
    removed += DeleteFileW(path) ? 1 : 0;
    return removed;
}
