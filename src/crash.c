#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "crash.h"
#include "version.h"

#define STACK_WORDS 512
#define LOG_LINE 192

static const char *volatile g_stage[FF_MAX_MONITORS];
static wchar_t g_path[MAX_PATH];

void crash_mark(int worker, const char *stage)
{
    if (worker >= 0 && worker < FF_MAX_MONITORS) {
        g_stage[worker] = stage;
    }
}

static void put(HANDLE f, const char *fmt, ...)
{
    char line[LOG_LINE];
    DWORD w;
    int n;
    __builtin_va_list ap;

    __builtin_va_start(ap, fmt);
    n = vsnprintf(line, sizeof line, fmt, ap);
    __builtin_va_end(ap);
    if (n > 0) {
        WriteFile(f, line, (DWORD)(n < LOG_LINE ? n : LOG_LINE - 1), &w, NULL);
    }
}

/* имя модуля и смещение адреса в нём; чужой адрес (драйвер) тоже полезен в журнале */
static void describe(HANDLE f, const char *what, ULONG_PTR addr)
{
    HMODULE m = NULL;
    wchar_t path[MAX_PATH] = L"?";
    const wchar_t *name = path;
    const wchar_t *slash;
    char utf8[96] = "?";

    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)addr, &m)) {
        GetModuleFileNameW(m, path, MAX_PATH);
        slash = wcsrchr(path, L'\\');
        name = slash ? slash + 1 : path;
        WideCharToMultiByte(CP_UTF8, 0, name, -1, utf8, sizeof utf8 - 1, NULL, NULL);
    }
    put(f, "%s 0x%llx %s+0x%llx\r\n", what, (unsigned long long)addr, utf8,
        (unsigned long long)(addr - (ULONG_PTR)m));
}

static LONG WINAPI filter(EXCEPTION_POINTERS *ep)
{
    HANDLE f = CreateFileW(g_path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    const EXCEPTION_RECORD *er = ep->ExceptionRecord;
    ULONG_PTR stack[STACK_WORDS];
    SIZE_T got = 0;
    SYSTEMTIME st;
    ULONG_PTR image = (ULONG_PTR)GetModuleHandleW(NULL);
    ULONG_PTR image_end = image;
    int i;
    int shown = 0;

    if (f == INVALID_HANDLE_VALUE) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    /* размер образа exe из заголовка PE: адреса возврата в нём можно сопоставить со сборкой */
    {
        const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)image;
        const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(image + (ULONG_PTR)dos->e_lfanew);

        image_end = image + nt->OptionalHeader.SizeOfImage;
    }
    GetLocalTime(&st);
    put(f, "\r\nfringefix %s, %04u-%02u-%02u %02u:%02u:%02u, thread %lu\r\n", FF_VERSION, st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, GetCurrentThreadId());
    put(f, "exception 0x%08lx, image base 0x%llx\r\n", er->ExceptionCode, (unsigned long long)image);
    describe(f, "at", (ULONG_PTR)er->ExceptionAddress);
    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2) {
        put(f, "access violation: %s address 0x%llx\r\n",
            er->ExceptionInformation[0] == 0 ? "read" : (er->ExceptionInformation[0] == 1 ? "write" : "execute"),
            (unsigned long long)er->ExceptionInformation[1]);
    }
    for (i = 0; i < FF_MAX_MONITORS; i++) {
        if (g_stage[i]) {
            put(f, "worker %d last stage: %s\r\n", i, g_stage[i]);
        }
    }
    /* адреса возврата нашего exe со стека упавшего потока: грубая трасса, читается через nm -n по той же сборке */
    if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)ep->ContextRecord->Rsp, stack, sizeof stack, &got)) {
        for (i = 0; i < (int)(got / sizeof stack[0]) && shown < 24; i++) {
            if (stack[i] > image && stack[i] < image_end) {
                put(f, "stack[%d] exe+0x%llx\r\n", i, (unsigned long long)(stack[i] - image));
                shown++;
            }
        }
    }
    CloseHandle(f);
    return EXCEPTION_CONTINUE_SEARCH;
}

void crash_install(void)
{
    wchar_t dir[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"APPDATA", dir, MAX_PATH);

    if (n == 0 || n >= MAX_PATH - 32) {
        wcscpy(g_path, L"crash.log");
    } else {
        _snwprintf(g_path, MAX_PATH, L"%ls\\fringefix", dir);
        g_path[MAX_PATH - 1] = L'\0';
        CreateDirectoryW(g_path, NULL);
        _snwprintf(g_path, MAX_PATH, L"%ls\\fringefix\\crash.log", dir);
        g_path[MAX_PATH - 1] = L'\0';
    }
    SetUnhandledExceptionFilter(filter);
}
