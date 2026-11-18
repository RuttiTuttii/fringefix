#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "crash.h"
#include "log.h"
#include "version.h"

#define STACK_WORDS 512
#define LOG_LINE 224
/* запас стека на обработчик: без него переполнение стека рождает исключение, которое некуда записать */
#define STACK_GUARANTEE (96 * 1024)
/* исключения, которые драйверы и библиотеки глотают штатно, в журнале шумят: пишем только первые */
#define FIRST_CHANCE_BUDGET 64
/* сколько раз подряд программа сама поднимается после сбоя; ручной запуск обнуляет счёт */
#define MAX_RELAUNCH 3
/* сторож: проверка раз в секунду; поток без новых отметок этапа дольше HANG_MS считается зависшим. устройство обычно
 * поднимается за треть секунды, первая сборка шейдеров занимает секунды, пятнадцати хватает с большим запасом */
#define WATCH_MS 1000
#define HANG_MS 15000
/* сколько сторож ждёт отчёт о зависании: стек зависшего потока может лежать в коде, держащем замок загрузчика */
#define REPORT_WAIT_MS 5000

static const char *volatile g_stage[FF_MAX_MONITORS];
static volatile DWORD g_tid[FF_MAX_MONITORS];
/* время последней отметки этапа каждого потока: по нему сторож видит, что поток замолчал */
static volatile ULONGLONG g_beat[FF_MAX_MONITORS];
static wchar_t g_path[MAX_PATH];
static wchar_t g_dump[MAX_PATH];
static volatile LONG g_budget = FIRST_CHANCE_BUDGET;
static volatile LONG g_busy;
static int g_crashes;

void crash_mark(int worker, const char *stage)
{
    if (worker >= 0 && worker < FF_MAX_MONITORS) {
        g_stage[worker] = stage;
        g_tid[worker] = GetCurrentThreadId();
        g_beat[worker] = GetTickCount64();
    }
}

const char *crash_stage(int worker)
{
    return worker >= 0 && worker < FF_MAX_MONITORS && g_stage[worker] ? g_stage[worker] : "-";
}

void crash_guard_stack(void)
{
    ULONG bytes = STACK_GUARANTEE;

    SetThreadStackGuarantee(&bytes);
}

static const char *exc_name(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        return "ACCESS_VIOLATION";
    case EXCEPTION_IN_PAGE_ERROR:
        return "IN_PAGE_ERROR";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_PRIV_INSTRUCTION:
        return "PRIV_INSTRUCTION";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:
        return "INT_OVERFLOW";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        return "FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        return "ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_STACK_OVERFLOW:
        return "STACK_OVERFLOW";
    case EXCEPTION_BREAKPOINT:
        return "BREAKPOINT";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        return "DATATYPE_MISALIGNMENT";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        return "NONCONTINUABLE_EXCEPTION";
    case 0xC0000374:
        return "HEAP_CORRUPTION";
    case 0xC0000409:
        return "STACK_BUFFER_OVERRUN_OR_FASTFAIL";
    case 0xC0000008:
        return "INVALID_HANDLE";
    case 0xC0000017:
        return "NO_MEMORY";
    case 0xC000012D:
        return "COMMITMENT_LIMIT";
    case 0xE06D7363:
        return "CPP_EXCEPTION";
    case 0xE0434352:
        return "CLR_EXCEPTION";
    default:
        return "unknown";
    }
}

/* имя модуля и смещение адреса в нём; чужой адрес (драйвер) тоже полезен в журнале */
static void where(ULONG_PTR addr, char *out, size_t cap)
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
    snprintf(out, cap, "0x%llx %s+0x%llx", (unsigned long long)addr, utf8, (unsigned long long)(addr - (ULONG_PTR)m));
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

/* адреса возврата нашего exe со стека потока: грубая трасса, читается через nm -n по той же сборке */
static void scan_stack(HANDLE f, const char *label, ULONG_PTR rsp)
{
    ULONG_PTR stack[STACK_WORDS];
    SIZE_T got = 0;
    ULONG_PTR image = (ULONG_PTR)GetModuleHandleW(NULL);
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)image;
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(image + (ULONG_PTR)dos->e_lfanew);
    ULONG_PTR image_end = image + nt->OptionalHeader.SizeOfImage;
    int i;
    int shown = 0;

    if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)rsp, stack, sizeof stack, &got)) {
        return;
    }
    for (i = 0; i < (int)(got / sizeof stack[0]) && shown < 24; i++) {
        if (stack[i] > image && stack[i] < image_end) {
            put(f, "%s stack[%d] exe+0x%llx\r\n", label, i, (unsigned long long)(stack[i] - image));
            shown++;
        }
    }
}

/* где сейчас остальные потоки захвата: упавший монитор часто виноват не один, второй стоит в драйвере на том же вызове.
 * потоки замораживаются; при сбое они не возобновляются, процесс всё равно умирает, а при зависании без перезапуска
 * (resume) возвращаются к работе. what это подпись строки в журнале: "crash" или "hang" */
static void other_workers(HANDLE f, const char *what, int resume)
{
    int i;

    for (i = 0; i < FF_MAX_MONITORS; i++) {
        DWORD tid = g_tid[i];
        HANDLE t;
        CONTEXT c;
        char at[160];
        char label[24];

        if (!tid || tid == GetCurrentThreadId()) {
            continue;
        }
        t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, tid);
        if (!t) {
            continue;
        }
        memset(&c, 0, sizeof c);
        c.ContextFlags = CONTEXT_CONTROL;
        if (SuspendThread(t) != (DWORD)-1 && GetThreadContext(t, &c)) {
            where((ULONG_PTR)c.Rip, at, sizeof at);
            put(f, "worker %d thread %lu stage %s rip %s\r\n", i, tid, crash_stage(i), at);
            ff_log(i, "at %s: thread %lu stage %s rip %s", what, tid, crash_stage(i), at);
            snprintf(label, sizeof label, "worker %d", i);
            scan_stack(f, label, (ULONG_PTR)c.Rsp);
            if (resume) {
                ResumeThread(t);
            }
        }
        CloseHandle(t);
    }
}

/* minidump со стеками всех потоков и списком модулей: открывается в WinDbg или Visual Studio, а рядом лежит текстовый crash.log */
static void write_dump(EXCEPTION_POINTERS *ep)
{
    typedef BOOL(WINAPI * dump_fn)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                                   PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
    HMODULE dbg = LoadLibraryW(L"dbghelp.dll");
    FARPROC proc = dbg ? GetProcAddress(dbg, "MiniDumpWriteDump") : NULL;
    dump_fn fn;
    HANDLE h;
    MINIDUMP_EXCEPTION_INFORMATION mei;
    BOOL ok;

    if (!proc) {
        ff_log(-1, "minidump skipped: dbghelp.dll not available (%lu)", GetLastError());
        return;
    }
    /* прямое приведение указателя на функцию к другому типу -Wpedantic не пропускает */
    memcpy(&fn, &proc, sizeof fn);
    h = CreateFileW(g_dump, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        ff_log(-1, "minidump skipped: cannot create file (%lu)", GetLastError());
        return;
    }
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;
    /* без исключения (зависание) в дамп идут только стеки потоков */
    ok = fn(GetCurrentProcess(), GetCurrentProcessId(), h,
            (MINIDUMP_TYPE)(MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules), ep ? &mei : NULL, NULL, NULL);
    CloseHandle(h);
    ff_log(-1, "minidump %s, last error %lu", ok ? "written to crash.dmp" : "failed", ok ? 0UL : GetLastError());
}

/* сбой внутри чужого кода (хуки оверлеев правят dxgi.dll прямо в процессе) нам не починить, но без перезапуска
 * фильтр пропадает до ручного запуска. процесс-преемник ждёт освобождения мьютекса единственного экземпляра сам (--relaunch) */
static int relaunch(void)
{
    wchar_t exe[MAX_PATH];
    wchar_t cmd[MAX_PATH + 48];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    if (g_crashes >= MAX_RELAUNCH) {
        ff_log(-1, "relaunch skipped: %d crashes in a row", g_crashes);
        return 0;
    }
    if (GetModuleFileNameW(NULL, exe, MAX_PATH) == 0) {
        return 0;
    }
    _snwprintf(cmd, sizeof cmd / sizeof cmd[0], L"\"%ls\" --relaunch --crashes=%d", exe, g_crashes + 1);
    cmd[sizeof cmd / sizeof cmd[0] - 1] = L'\0';
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        ff_log(-1, "relaunch failed (%lu)", GetLastError());
        return 0;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    ff_log(-1, "relaunched after the crash (%d of %d)", g_crashes + 1, MAX_RELAUNCH);
    return 1;
}

static void on_abort(int sig)
{
    int i;

    (void)sig;
    ff_log(-1, "abort() called, see the last stages below");
    for (i = 0; i < FF_MAX_MONITORS; i++) {
        if (g_stage[i]) {
            ff_log(i, "last stage: %s", g_stage[i]);
        }
    }
    _exit(3);
}

/* до обработчиков программы и до фильтра: ловит исключения, которые потом проглотит драйвер или __try, и переполнение
 * стека. записывает код, адрес, модуль и поток, шум отсекается бюджетом */
static LONG CALLBACK first_chance(EXCEPTION_POINTERS *ep)
{
    const EXCEPTION_RECORD *er = ep->ExceptionRecord;
    char at[160];
    int i;

    /* отладочный вывод, имя потока и прерывание отладчика приходят штатно */
    if (er->ExceptionCode == 0x40010006 || er->ExceptionCode == 0x4001000A || er->ExceptionCode == 0x406D1388
        || er->ExceptionCode == EXCEPTION_BREAKPOINT || er->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    /* два старших бита 11 это коды ошибок; остальное (предупреждения, информация) не пишем, кроме исключений C++ */
    if ((er->ExceptionCode >> 30) != 3) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (InterlockedExchange(&g_busy, 1)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    /* переполнение стека пишется всегда: после него процесс почти наверняка умрёт */
    if (er->ExceptionCode == EXCEPTION_STACK_OVERFLOW || InterlockedDecrement(&g_budget) >= 0) {
        where((ULONG_PTR)er->ExceptionAddress, at, sizeof at);
        ff_log(-1, "exception 0x%08lx %s at %s (first chance)", er->ExceptionCode, exc_name(er->ExceptionCode), at);
        if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2) {
            ff_log(-1, "  %s address 0x%llx",
                   er->ExceptionInformation[0] == 0 ? "read" : (er->ExceptionInformation[0] == 1 ? "write" : "execute"),
                   (unsigned long long)er->ExceptionInformation[1]);
        }
        for (i = 0; i < FF_MAX_MONITORS; i++) {
            if (g_tid[i] == GetCurrentThreadId()) {
                ff_log(i, "  in worker thread, stage %s", crash_stage(i));
            }
        }
    }
    InterlockedExchange(&g_busy, 0);
    return EXCEPTION_CONTINUE_SEARCH;
}

static LONG WINAPI filter(EXCEPTION_POINTERS *ep)
{
    HANDLE f = CreateFileW(g_path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    const EXCEPTION_RECORD *er = ep->ExceptionRecord;
    const CONTEXT *c = ep->ContextRecord;
    DWORD last_error = GetLastError();
    SYSTEMTIME st;
    ULONG_PTR image = (ULONG_PTR)GetModuleHandleW(NULL);
    char at[160];
    int i;

    where((ULONG_PTR)er->ExceptionAddress, at, sizeof at);
    /* строка в основном журнале пишется первой: файл crash.log мог не открыться */
    ff_log(-1, "CRASH exception 0x%08lx %s at %s, last error %lu, details in crash.log and crash.dmp", er->ExceptionCode,
           exc_name(er->ExceptionCode), at, last_error);
    for (i = 0; i < FF_MAX_MONITORS; i++) {
        if (g_stage[i]) {
            ff_log(i, "last stage: %s", g_stage[i]);
        }
    }
    if (f != INVALID_HANDLE_VALUE) {
        GetLocalTime(&st);
        put(f, "\r\nfringefix %s, %04u-%02u-%02u %02u:%02u:%02u, thread %lu\r\n", FF_VERSION, st.wYear, st.wMonth,
            st.wDay, st.wHour, st.wMinute, st.wSecond, GetCurrentThreadId());
        put(f, "exception 0x%08lx %s, flags 0x%lx, image base 0x%llx, last error %lu\r\n", er->ExceptionCode,
            exc_name(er->ExceptionCode), er->ExceptionFlags, (unsigned long long)image, last_error);
        put(f, "at %s\r\n", at);
        if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2) {
            put(f, "access violation: %s address 0x%llx\r\n",
                er->ExceptionInformation[0] == 0 ? "read" : (er->ExceptionInformation[0] == 1 ? "write" : "execute"),
                (unsigned long long)er->ExceptionInformation[1]);
        } else if (er->ExceptionCode == EXCEPTION_IN_PAGE_ERROR && er->NumberParameters >= 3) {
            put(f, "in page error: address 0x%llx, status 0x%llx\r\n", (unsigned long long)er->ExceptionInformation[1],
                (unsigned long long)er->ExceptionInformation[2]);
        }
        put(f, "rax %016llx rbx %016llx rcx %016llx rdx %016llx\r\n", (unsigned long long)c->Rax,
            (unsigned long long)c->Rbx, (unsigned long long)c->Rcx, (unsigned long long)c->Rdx);
        put(f, "rsi %016llx rdi %016llx rbp %016llx rsp %016llx\r\n", (unsigned long long)c->Rsi,
            (unsigned long long)c->Rdi, (unsigned long long)c->Rbp, (unsigned long long)c->Rsp);
        put(f, "r8  %016llx r9  %016llx r10 %016llx r11 %016llx\r\n", (unsigned long long)c->R8,
            (unsigned long long)c->R9, (unsigned long long)c->R10, (unsigned long long)c->R11);
        for (i = 0; i < FF_MAX_MONITORS; i++) {
            if (g_stage[i]) {
                put(f, "worker %d last stage: %s\r\n", i, g_stage[i]);
            }
        }
        scan_stack(f, "crashed", (ULONG_PTR)c->Rsp);
        other_workers(f, "crash", 0);
        CloseHandle(f);
    }
    write_dump(ep);
    /* после своего запуска процесс завершается сам: системное окно сбоя поверх игры не нужно */
    if (relaunch()) {
        TerminateProcess(GetCurrentProcess(), er->ExceptionCode);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* отчёт о зависании в отдельном потоке: чтение стека зависшего потока может упереться в замок, который тот держит
 * (загрузчик модулей), и тогда сторож не вправе встать вместе с ним */
static DWORD WINAPI hang_report(LPVOID arg)
{
    HANDLE f = CreateFileW(g_path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    SYSTEMTIME st;
    int worker = (int)(INT_PTR)arg;

    if (f != INVALID_HANDLE_VALUE) {
        GetLocalTime(&st);
        put(f, "\r\nfringefix %s, %04u-%02u-%02u %02u:%02u:%02u, hang\r\n", FF_VERSION, st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond);
        put(f, "worker %d made no progress, last stage: %s\r\n", worker, crash_stage(worker));
        other_workers(f, "hang", 1);
        CloseHandle(f);
    }
    write_dump(NULL);
    return 0;
}

static void on_hang(int worker, ULONGLONG silent_ms)
{
    HANDLE t;
    int i;

    ff_log(-1, "HANG worker %d made no progress for %llu s, last stage: %s", worker,
           (unsigned long long)(silent_ms / 1000), crash_stage(worker));
    for (i = 0; i < FF_MAX_MONITORS; i++) {
        if (g_stage[i]) {
            ff_log(i, "last stage: %s", g_stage[i]);
        }
    }
    t = CreateThread(NULL, 0, hang_report, (LPVOID)(INT_PTR)worker, 0, NULL);
    if (t) {
        if (WaitForSingleObject(t, REPORT_WAIT_MS) == WAIT_TIMEOUT) {
            ff_log(-1, "hang report did not finish in %d s, going on", REPORT_WAIT_MS / 1000);
        }
        CloseHandle(t);
    }
    /* замёрзший захват держит на экране последний кадр поверх всего; остаётся только запустить программу заново */
    if (relaunch()) {
        TerminateProcess(GetCurrentProcess(), 0xDEAD0001u);
    }
}

static DWORD WINAPI watch_main(LPVOID arg)
{
    ULONGLONG last = GetTickCount64();

    (void)arg;
    for (;;) {
        ULONGLONG now;
        int i;

        Sleep(WATCH_MS);
        now = GetTickCount64();
        /* сам сторож не получал времени дольше четырёх периодов: система спала или процесс стоял на отладчике,
         * это не зависание потоков захвата; отсчёт начинается заново */
        if (now - last > 4 * WATCH_MS) {
            for (i = 0; i < FF_MAX_MONITORS; i++) {
                g_beat[i] = now;
            }
            last = now;
            continue;
        }
        last = now;
        for (i = 0; i < FF_MAX_MONITORS; i++) {
            const char *st = g_stage[i];

            /* смотрятся только запуск и закрытие устройства: кадровые стадии (present, acquire, copy, compare, draw) могут
             * законно ждать, например пока монитор спит, а вызов запуска без ответа это ровно то, что видно в журнале */
            if (!g_tid[i] || !st || now - g_beat[i] < HANG_MS
                || !(strncmp(st, "init", 4) == 0 || strcmp(st, "shutdown") == 0 || strcmp(st, "upload") == 0)) {
                continue;
            }
            on_hang(i, now - g_beat[i]);
            /* перезапуск не вышел (три сбоя подряд): отчёт один на программу, дальше сторож молчит */
            return 0;
        }
    }
}

void crash_watch_start(void)
{
    HANDLE t = CreateThread(NULL, 0, watch_main, NULL, 0, NULL);

    if (t) {
        CloseHandle(t);
    }
}

void crash_install(void)
{
    const wchar_t *opt = wcsstr(GetCommandLineW(), L"--crashes=");

    g_crashes = opt ? _wtoi(opt + 10) : 0;
    ff_log_path(L"crash.log", g_path);
    ff_log_path(L"crash.dmp", g_dump);
    crash_guard_stack();
    AddVectoredExceptionHandler(1, first_chance);
    SetUnhandledExceptionFilter(filter);
    /* abort() из библиотек C обходит фильтр исключений: ловим его отдельно */
    signal(SIGABRT, on_abort);
}
