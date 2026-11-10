#include "live.h"

#include <sddl.h>
#include <stdio.h>
#include <string.h>

/* читать могут все процессы пользователя, в том числе игра с повышенными правами, писать только он сам:
 * без явного списка доступа объект процесса с админ-токеном не открылся бы из обычного приложения, а без записи
 * для пользователя перезапущенное приложение не смогло бы открыть объект, который ещё держит аддон.
 * SDDL строится с SID пользователя: "OW" (владелец) на этой системе не годится, права владельца неявные только на чтение ACL */
#define SDDL_READ_ALL_FMT L"D:(A;;GR;;;WD)(A;;GA;;;%ls)"
#define SDDL_SYNC_ALL L"D:(A;;0x100000;;;WD)"
#define READ_RETRIES 64
/* команды от аддона: писать и читать может любой процесс пользователя, данные это одно значение выключателя */
#define SDDL_CMD_ALL L"D:(A;;GRGW;;;WD)"

static HANDLE g_map_w;
static ff_live *g_live_w;
static HANDLE g_map_r;
static const ff_live *g_live_r;
static HANDLE g_app_proc;
static unsigned g_app_pid;
static HANDLE g_mark;

typedef struct {
    volatile LONG seq;
    volatile LONG enabled;
} ff_cmd;

static HANDLE g_cmd_map;
static ff_cmd *g_cmd;
static LONG g_cmd_seen;

/* атрибуты с заданным списком доступа; sd освобождает вызывающий через LocalFree */
static int make_attrs(const wchar_t *sddl, SECURITY_ATTRIBUTES *sa, PSECURITY_DESCRIPTOR *sd)
{
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, sd, NULL)) {
        return -1;
    }
    sa->nLength = sizeof *sa;
    sa->lpSecurityDescriptor = *sd;
    sa->bInheritHandle = FALSE;
    return 0;
}

/* SDDL со всеми на чтение и текущим пользователем на всё; 0 или -1 */
static int user_sddl(wchar_t out[128])
{
    HANDLE tok = NULL;
    BYTE buf[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
    DWORD got = 0;
    wchar_t *sid = NULL;
    int rc = -1;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        if (GetTokenInformation(tok, TokenUser, buf, sizeof buf, &got)
            && ConvertSidToStringSidW(((TOKEN_USER *)buf)->User.Sid, &sid)) {
            _snwprintf(out, 128, SDDL_READ_ALL_FMT, sid);
            out[127] = L'\0';
            LocalFree(sid);
            rc = 0;
        }
        CloseHandle(tok);
    }
    return rc;
}

int live_publish_open(void)
{
    SECURITY_ATTRIBUTES sa;
    PSECURITY_DESCRIPTOR sd = NULL;
    wchar_t sddl[128];

    if (g_live_w) {
        return 0;
    }
    if (user_sddl(sddl) != 0 || make_attrs(sddl, &sa, &sd) != 0) {
        return -1;
    }
    g_map_w = CreateFileMappingW(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, sizeof(ff_live), FF_LIVE_NAME);
    LocalFree(sd);
    if (!g_map_w) {
        return -1;
    }
    g_live_w = MapViewOfFile(g_map_w, FILE_MAP_WRITE, 0, 0, sizeof(ff_live));
    if (!g_live_w) {
        CloseHandle(g_map_w);
        g_map_w = NULL;
        return -1;
    }
    g_live_w->pid = GetCurrentProcessId();
    g_live_w->magic = FF_LIVE_MAGIC;
    /* память команд необязательна: без неё ползунок аддона просто не достаёт до приложения */
    if (!g_cmd && make_attrs(SDDL_CMD_ALL, &sa, &sd) == 0) {
        g_cmd_map = CreateFileMappingW(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, sizeof(ff_cmd), FF_CMD_NAME);
        LocalFree(sd);
        g_cmd = g_cmd_map ? MapViewOfFile(g_cmd_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ff_cmd)) : NULL;
        if (g_cmd) {
            g_cmd_seen = g_cmd->seq;
        }
    }
    return 0;
}

void live_publish(const ff_config *cfg, int master, const wchar_t (*devices)[FF_LIVE_DEV_LEN], int n)
{
    int m;

    /* память не открылась при запуске (старый объект ещё держит аддон или её закрыли): пробуем снова на каждой правке */
    if (!g_live_w && live_publish_open() != 0) {
        return;
    }
    /* нечётное значение на время записи, чётное после: читатель повторяет чтение, если увидел нечётное
     * или разное значение до и после */
    InterlockedIncrement(&g_live_w->seq);
    g_live_w->master = master ? 1 : 0;
    g_live_w->selected = cfg->monitor;
    g_live_w->count = n < 0 ? 0 : (n > FF_MAX_MONITORS ? FF_MAX_MONITORS : n);
    for (m = 0; m < FF_MAX_MONITORS; m++) {
        ff_live_view *v = &g_live_w->view[m];
        int i;

        for (i = 0; i < 6; i++) {
            v->shift[i] = cfg->view[m].shift[i / 2][i % 2];
        }
        v->sharpen = cfg->view[m].sharpen;
        v->enabled = cfg->view[m].enabled ? 1 : 0;
        v->device[0] = L'\0';
        if (devices && m < g_live_w->count) {
            wcsncpy(v->device, devices[m], FF_LIVE_DEV_LEN - 1);
            v->device[FF_LIVE_DEV_LEN - 1] = L'\0';
        }
    }
    g_live_w->pid = GetCurrentProcessId();
    InterlockedIncrement(&g_live_w->seq);
}

void live_publish_close(void)
{
    if (g_cmd) {
        UnmapViewOfFile(g_cmd);
        g_cmd = NULL;
    }
    if (g_cmd_map) {
        CloseHandle(g_cmd_map);
        g_cmd_map = NULL;
    }
    if (g_live_w) {
        UnmapViewOfFile(g_live_w);
        g_live_w = NULL;
    }
    if (g_map_w) {
        CloseHandle(g_map_w);
        g_map_w = NULL;
    }
}

void live_read_close(void)
{
    if (g_live_r) {
        UnmapViewOfFile(g_live_r);
        g_live_r = NULL;
    }
    if (g_map_r) {
        CloseHandle(g_map_r);
        g_map_r = NULL;
    }
    if (g_app_proc) {
        CloseHandle(g_app_proc);
        g_app_proc = NULL;
    }
    g_app_pid = 0;
}

/* приложение живо, пока его процесс не завершился */
static int app_alive(unsigned pid)
{
    if (pid != g_app_pid) {
        if (g_app_proc) {
            CloseHandle(g_app_proc);
        }
        g_app_proc = OpenProcess(SYNCHRONIZE, FALSE, pid);
        g_app_pid = pid;
    }
    return g_app_proc && WaitForSingleObject(g_app_proc, 0) == WAIT_TIMEOUT;
}

int live_read(ff_live *out)
{
    int attempt;

    if (!g_live_r) {
        g_map_r = OpenFileMappingW(FILE_MAP_READ, FALSE, FF_LIVE_NAME);
        if (!g_map_r) {
            return 0;
        }
        g_live_r = MapViewOfFile(g_map_r, FILE_MAP_READ, 0, 0, sizeof(ff_live));
        if (!g_live_r) {
            live_read_close();
            return 0;
        }
    }
    for (attempt = 0; attempt < READ_RETRIES; attempt++) {
        ff_live snap;
        LONG before = g_live_r->seq;

        if (before & 1) {
            /* писатель мог быть вытеснен посреди записи: уступаем ему процессор */
            SwitchToThread();
            continue;
        }
        MemoryBarrier();
        snap = *g_live_r;
        MemoryBarrier();
        if (g_live_r->seq != before) {
            continue;
        }
        if (snap.magic != FF_LIVE_MAGIC || !app_alive(snap.pid)) {
            /* приложение закрыто: держать старый объект нельзя, перезапущенное приложение создаст его заново */
            live_read_close();
            return 0;
        }
        *out = snap;
        return 1;
    }
    return 0;
}

int live_cmd_poll(int *enabled)
{
    LONG seq;

    if (!g_cmd) {
        return 0;
    }
    seq = g_cmd->seq;
    if (seq == g_cmd_seen) {
        return 0;
    }
    g_cmd_seen = seq;
    *enabled = g_cmd->enabled != 0;
    return 1;
}

int live_cmd_send(int enabled)
{
    static HANDLE map;
    static ff_cmd *cmd;

    if (!cmd) {
        map = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, FF_CMD_NAME);
        cmd = map ? MapViewOfFile(map, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(ff_cmd)) : NULL;
        if (!cmd) {
            if (map) {
                CloseHandle(map);
                map = NULL;
            }
            return 0;
        }
    }
    /* значение раньше счётчика: опрос по новому счётчику уже видит новое значение */
    cmd->enabled = enabled ? 1 : 0;
    MemoryBarrier();
    InterlockedIncrement(&cmd->seq);
    return 1;
}

const ff_live_view *live_pick_view(const ff_live *snap, const wchar_t *device)
{
    LONG i;
    LONG sel = snap->selected < 0 ? 0 : (snap->selected >= FF_MAX_MONITORS ? FF_MAX_MONITORS - 1 : snap->selected);

    if (device && device[0]) {
        for (i = 0; i < snap->count && i < FF_MAX_MONITORS; i++) {
            if (_wcsicmp(snap->view[i].device, device) == 0) {
                return &snap->view[i];
            }
        }
    }
    return &snap->view[sel];
}

static void mark_name(DWORD pid, wchar_t out[48])
{
    _snwprintf(out, 48, FF_ADDON_MARK_FMT, (unsigned long)pid);
    out[47] = L'\0';
}

void live_addon_announce(void)
{
    SECURITY_ATTRIBUTES sa;
    PSECURITY_DESCRIPTOR sd = NULL;
    wchar_t name[48];

    if (g_mark || make_attrs(SDDL_SYNC_ALL, &sa, &sd) != 0) {
        return;
    }
    mark_name(GetCurrentProcessId(), name);
    g_mark = CreateMutexW(&sa, FALSE, name);
    LocalFree(sd);
}

void live_addon_withdraw(void)
{
    if (g_mark) {
        CloseHandle(g_mark);
        g_mark = NULL;
    }
}

int live_addon_present(DWORD pid)
{
    wchar_t name[48];
    HANDLE h;

    mark_name(pid, name);
    h = OpenMutexW(SYNCHRONIZE, FALSE, name);
    if (!h) {
        return 0;
    }
    CloseHandle(h);
    return 1;
}
