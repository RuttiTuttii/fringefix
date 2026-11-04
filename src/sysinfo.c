#include <windows.h>
#include <string.h>

#include "sysinfo.h"

typedef LONG(WINAPI *rtl_get_version_fn)(OSVERSIONINFOW *);

/* GetVersionEx врёт без манифеста совместимости, RtlGetVersion из ntdll отдаёт настоящий номер */
unsigned sys_windows_build(void)
{
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    FARPROC proc = nt ? GetProcAddress(nt, "RtlGetVersion") : NULL;
    rtl_get_version_fn fn;
    OSVERSIONINFOW v;

    if (!proc) {
        return 0;
    }
    /* прямое приведение указателя на функцию к другому типу -Wpedantic не пропускает */
    memcpy(&fn, &proc, sizeof fn);
    memset(&v, 0, sizeof v);
    v.dwOSVersionInfoSize = sizeof v;
    return fn(&v) == 0 ? (unsigned)v.dwBuildNumber : 0;
}
