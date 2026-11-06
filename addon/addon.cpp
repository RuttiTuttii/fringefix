/* fringefix: ReShade-аддон. фильтр рисует эффект fringefix.fx (ядро 3x3 на канал), а здесь живут три вещи:
 * выбор настроек (живые значения приложения, потом его ini, потом собственные), заливка весов ядра в uniform'ы
 * эффекта и вкладка в оверлее ReShade с настройкой и замерами, как в окне диагностики приложения.
 * все обработчики событий берут одну блокировку: ReShade зовёт их из потоков рендера каждой цепочки */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unordered_map>

#define ImTextureID ImU64
#include <imgui.h>
#include <reshade.hpp>

extern "C" {
#include "benchstat.h"
#include "config.h"
#include "kernel.h"
#include "live.h"
#include "power.h"
}

#ifndef FF_VERSION
#define FF_VERSION "dev"
#endif

using namespace reshade::api;
using reshade::addon_event;
using reshade::get_config_value;
using reshade::set_config_value;

namespace {

constexpr int RING = 240;
/* метки времени на кадр и число кадров в полёте: результат видеокарты приходит с задержкой в несколько кадров */
constexpr int QPF = 16;
constexpr int QFRAMES = 4;
constexpr ULONGLONG POLL_MS = 500;
constexpr ULONGLONG POWER_MS = 250;
/* замеры идут, только пока вкладка открыта: так аддон ничего не стоит в обычной игре */
constexpr unsigned OVERLAY_GRACE = 3;
constexpr const char *EFFECT_FILE = "fringefix.fx";
constexpr const char *CFG_SECTION = "FRINGEFIX";
constexpr int INI_MAX = 8192;

enum { SRC_OWN, SRC_INI, SRC_APP };

struct ring {
    float v[RING] = {};
    int head = 0;
    int count = 0;

    void push(float x)
    {
        v[head] = x;
        head = (head + 1) % RING;
        if (count < RING) {
            count++;
        }
    }
    /* смещение первого значения для PlotLines */
    int offset() const { return count < RING ? 0 : head; }
};

struct settings {
    float shift[6] = {};
    float sharpen = 0.0f;
    int enabled = 1;

    bool same(const settings &o) const
    {
        return memcmp(shift, o.shift, sizeof shift) == 0 && sharpen == o.sharpen && enabled == o.enabled;
    }
};

struct rt_state {
    effect_uniform_variable kb{}, kb2{}, kw[9]{};
    int uniforms_found = 0;
    effect_technique tech{};
    bool tech_ok = false;
    bool want_marker = false;
    unsigned pushed_version = 0;
    ULONGLONG next_bind = 0;

    unsigned frame = 0;
    unsigned overlay_frame = 0;
    bool overlay_seen = false;

    bool timing_tried = false;
    bool timing_ok = false;
    query_heap heap{};
    uint64_t freq = 0;
    int qn[QFRAMES] = {};
    int qour[QFRAMES] = {};
    bool qbusy[QFRAMES] = {};
    int cur_slot = -1;

    LONGLONG cpu_t0 = 0;
    LONGLONG last_present = 0;
    ring gpu, cpu, frame_ms;
};

struct globals {
    bool inited = false;
    bool follow_app = true;
    settings own, eff, ini;
    int source = SRC_OWN;
    unsigned version = 1;
    ULONGLONG next_poll = 0;
    wchar_t ini_path[MAX_PATH] = {};
    FILETIME ini_time = {};
    bool ini_valid = false;
    bool marker = false;
    bool power_tried = false;
    bool power_ok = false;
    ULONGLONG next_power = 0;
    ring watts, util;
    float clock_mhz = -1.0f;
    CRITICAL_SECTION lock;
    std::unordered_map<effect_runtime *, rt_state> rts;
};

globals G;

struct guard {
    guard() { EnterCriticalSection(&G.lock); }
    ~guard() { LeaveCriticalSection(&G.lock); }
};

double qpc_ms(LONGLONG delta)
{
    static LARGE_INTEGER f = [] {
        LARGE_INTEGER x;
        QueryPerformanceFrequency(&x);
        return x;
    }();
    return 1000.0 * (double)delta / (double)f.QuadPart;
}

LONGLONG qpc_now()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

/* собственные значения лежат в ReShade.ini рядом с игрой: шесть сдвигов и резкость через запятую */
void load_own()
{
    char buf[160];
    size_t n = sizeof buf;
    float v[7];

    if (get_config_value(nullptr, CFG_SECTION, "Values", buf, &n)
        && sscanf(buf, "%f,%f,%f,%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6]) == 7) {
        for (int i = 0; i < 6; i++) {
            G.own.shift[i] = v[i] < -(float)FF_SHIFT_LIMIT ? -(float)FF_SHIFT_LIMIT
                             : v[i] > (float)FF_SHIFT_LIMIT ? (float)FF_SHIFT_LIMIT : v[i];
        }
        G.own.sharpen = v[6] < -(float)FF_SHARPEN_LIMIT ? -(float)FF_SHARPEN_LIMIT
                        : v[6] > (float)FF_SHARPEN_LIMIT ? (float)FF_SHARPEN_LIMIT : v[6];
    }
    n = sizeof buf;
    if (get_config_value(nullptr, CFG_SECTION, "Follow", buf, &n)) {
        G.follow_app = atoi(buf) != 0;
    }
}

void save_own()
{
    char buf[160];

    snprintf(buf, sizeof buf, "%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f", G.own.shift[0], G.own.shift[1],
             G.own.shift[2], G.own.shift[3], G.own.shift[4], G.own.shift[5], G.own.sharpen);
    set_config_value(nullptr, CFG_SECTION, "Values", static_cast<const char *>(buf));
    set_config_value(nullptr, CFG_SECTION, "Follow", G.follow_app ? "1" : "0");
}

void ensure_init()
{
    wchar_t dir[MAX_PATH];

    if (G.inited) {
        return;
    }
    G.inited = true;
    load_own();
    G.eff = G.own;
    if (GetEnvironmentVariableW(L"APPDATA", dir, MAX_PATH) > 0 && dir[0]) {
        _snwprintf(G.ini_path, MAX_PATH, L"%ls\\fringefix\\fringefix.ini", dir);
        G.ini_path[MAX_PATH - 1] = L'\0';
    }
}

/* ini приложения: перечитывается только при смене времени записи; запись в приложении атомарная,
 * поэтому полуфайла здесь не бывает */
bool read_ini(settings &out)
{
    WIN32_FILE_ATTRIBUTE_DATA fa;

    if (!G.ini_path[0] || !GetFileAttributesExW(G.ini_path, GetFileExInfoStandard, &fa)) {
        G.ini_valid = false;
        return false;
    }
    if (!G.ini_valid || CompareFileTime(&fa.ftLastWriteTime, &G.ini_time) != 0) {
        char buf[INI_MAX];
        DWORD got = 0;
        HANDLE f = CreateFileW(G.ini_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ff_config c;

        if (f == INVALID_HANDLE_VALUE) {
            return false;
        }
        if (!ReadFile(f, buf, sizeof buf - 1, &got, nullptr)) {
            got = 0;
        }
        CloseHandle(f);
        buf[got] = '\0';
        ff_config_defaults(&c);
        ff_config_parse(buf, &c);
        for (int i = 0; i < 6; i++) {
            G.ini.shift[i] = c.shift[i / 2][i % 2];
        }
        G.ini.sharpen = c.sharpen;
        G.ini.enabled = 1;
        G.ini_time = fa.ftLastWriteTime;
        G.ini_valid = true;
    }
    out = G.ini;
    return true;
}

/* выбор источника раз в полсекунды: приложение, потом его ini, потом собственные значения */
void poll_config()
{
    ULONGLONG now = GetTickCount64();
    settings n = G.own;
    int src = SRC_OWN;

    if (now < G.next_poll) {
        return;
    }
    G.next_poll = now + POLL_MS;
    if (G.follow_app) {
        float sh[6];
        float sp;
        int en;

        if (live_read(sh, &sp, &en)) {
            memcpy(n.shift, sh, sizeof sh);
            n.sharpen = sp;
            n.enabled = en;
            src = SRC_APP;
        } else if (read_ini(n)) {
            src = SRC_INI;
        }
    }
    G.source = src;
    if (!n.same(G.eff)) {
        G.eff = n;
        G.version++;
    }
}

rt_state *state(effect_runtime *rt)
{
    auto it = G.rts.find(rt);

    return it == G.rts.end() ? nullptr : &it->second;
}

/* uniform'ы и техника эффекта; дескрипторы живут до следующей перезагрузки эффектов */
void bind_effect(effect_runtime *rt, rt_state &s)
{
    const uint64_t old_tech = s.tech.handle;
    const uint64_t old_kb = s.kb.handle;
    const bool old_ok = s.tech_ok && s.uniforms_found == 11;

    s.uniforms_found = 0;
    s.tech_ok = false;
    rt->enumerate_uniform_variables(EFFECT_FILE, [&s](effect_runtime *r, effect_uniform_variable v) {
        char name[32];

        r->get_uniform_variable_name(v, name);
        if (strcmp(name, "ff_base") == 0) {
            s.kb = v;
            s.uniforms_found++;
        } else if (strcmp(name, "ff_base2") == 0) {
            s.kb2 = v;
            s.uniforms_found++;
        } else if (strncmp(name, "ff_kw", 5) == 0 && name[5] >= '0' && name[5] <= '8' && name[6] == '\0') {
            s.kw[name[5] - '0'] = v;
            s.uniforms_found++;
        }
    });
    rt->enumerate_techniques(EFFECT_FILE, [&s](effect_runtime *, effect_technique t) {
        s.tech = t;
        s.tech_ok = true;
    });
    /* новые дескрипторы или эффект появился заново: значения надо залить снова */
    if (!old_ok || s.tech.handle != old_tech || s.kb.handle != old_kb) {
        s.pushed_version = 0;
    }
}

void push_uniforms(effect_runtime *rt, rt_state &s)
{
    ff_kernel k;
    float kb[4];
    float kb2[4];

    if (s.uniforms_found != 11) {
        return;
    }
    ff_kernel_build(G.eff.shift, G.eff.sharpen, &k);
    kb[0] = (float)k.base[0][0];
    kb[1] = (float)k.base[0][1];
    kb[2] = (float)k.base[1][0];
    kb[3] = (float)k.base[1][1];
    kb2[0] = (float)k.base[2][0];
    kb2[1] = (float)k.base[2][1];
    kb2[2] = k.shared ? 1.0f : 0.0f;
    kb2[3] = G.eff.enabled ? 1.0f : 0.0f;
    rt->set_uniform_value_float(s.kb, kb, 4);
    rt->set_uniform_value_float(s.kb2, kb2, 4);
    for (int ch = 0; ch < 3; ch++) {
        for (int row = 0; row < 3; row++) {
            float w[4] = { k.w[ch][row * 3], k.w[ch][row * 3 + 1], k.w[ch][row * 3 + 2], 0.0f };

            rt->set_uniform_value_float(s.kw[ch * 3 + row], w, 4);
        }
    }
    s.pushed_version = G.version;
}

/* маркер для приложения: оверлей на паузе, пока хоть одна цепочка в процессе реально применяет эффект */
void update_marker()
{
    bool want = false;

    for (auto &p : G.rts) {
        want = want || p.second.want_marker;
    }
    if (want && !G.marker) {
        live_addon_announce();
    } else if (!want && G.marker) {
        live_addon_withdraw();
    }
    G.marker = want;
}

bool overlay_recent(const rt_state &s)
{
    return s.overlay_seen && s.frame - s.overlay_frame <= OVERLAY_GRACE;
}

void create_timing(effect_runtime *rt, rt_state &s)
{
    command_queue *q;

    if (s.timing_tried) {
        return;
    }
    s.timing_tried = true;
    q = rt->get_command_queue();
    s.freq = q ? q->get_timestamp_frequency() : 0;
    s.timing_ok = s.freq != 0 && rt->get_device()->create_query_heap(query_type::timestamp, QPF * QFRAMES, &s.heap);
}

void on_init(effect_runtime *rt)
{
    guard g;

    G.rts[rt];
}

void on_destroy(effect_runtime *rt)
{
    guard g;

    /* куча запросов не уничтожается: метки последних кадров могут быть ещё в полёте, а ждёт ли ReShade видеокарту
     * перед этим событием, заголовки не говорят; утечка 64 запросов на пересоздание цепочки безопаснее */
    G.rts.erase(rt);
    update_marker();
}

void on_reloaded(effect_runtime *rt)
{
    guard g;
    rt_state *s = state(rt);

    if (!s) {
        return;
    }
    ensure_init();
    bind_effect(rt, *s);
    s->pushed_version = 0;
    poll_config();
    push_uniforms(rt, *s);
}

/* пресет мог вернуть значения uniform'ов к сохранённым: заливаем свои заново */
void on_preset(effect_runtime *rt, const char *)
{
    guard g;
    rt_state *s = state(rt);

    if (s) {
        s->pushed_version = 0;
    }
}

void on_begin_effects(effect_runtime *rt, command_list *cmd, resource_view, resource_view)
{
    guard g;
    rt_state *s = state(rt);
    int slot;

    if (!s) {
        return;
    }
    /* слот прошлого кадра не должен жить дольше кадра: иначе метки пишутся без начала и без сбора */
    s->cur_slot = -1;
    if (!overlay_recent(*s)) {
        return;
    }
    s->cpu_t0 = qpc_now();
    if (!s->timing_ok) {
        return;
    }
    slot = (int)(s->frame % QFRAMES);
    if (s->qbusy[slot]) {
        return;
    }
    s->cur_slot = slot;
    s->qn[slot] = 1;
    s->qour[slot] = -1;
    s->qbusy[slot] = true;
    cmd->end_query(s->heap, query_type::timestamp, (uint32_t)(slot * QPF));
}

/* метка после каждой техники: время нашей это разница с меткой предыдущей */
void on_render_technique(effect_runtime *rt, effect_technique t, command_list *cmd, resource_view, resource_view)
{
    guard g;
    rt_state *s = state(rt);
    int slot;

    if (!s || s->cur_slot < 0) {
        return;
    }
    slot = s->cur_slot;
    if (s->qn[slot] >= QPF) {
        return;
    }
    cmd->end_query(s->heap, query_type::timestamp, (uint32_t)(slot * QPF + s->qn[slot]));
    if (s->tech_ok && t.handle == s->tech.handle) {
        s->qour[slot] = s->qn[slot];
    }
    s->qn[slot]++;
}

void on_finish_effects(effect_runtime *rt, command_list *, resource_view, resource_view)
{
    guard g;
    rt_state *s = state(rt);

    if (s && s->cpu_t0) {
        s->cpu.push((float)qpc_ms(qpc_now() - s->cpu_t0));
        s->cpu_t0 = 0;
    }
    if (s) {
        s->cur_slot = -1;
    }
}

/* забирает готовые результаты меток; ещё не готовые остаются до следующего кадра */
void collect_timing(effect_runtime *rt, rt_state &s)
{
    for (int slot = 0; slot < QFRAMES; slot++) {
        uint64_t res[QPF];

        if (!s.qbusy[slot] || (int)(s.frame % QFRAMES) == slot) {
            continue;
        }
        if (s.qour[slot] < 1) {
            s.qbusy[slot] = false;
            continue;
        }
        if (rt->get_device()->get_query_heap_results(s.heap, query_type::timestamp, (uint32_t)(slot * QPF),
                                                     (uint32_t)s.qn[slot], res, sizeof(uint64_t))) {
            s.gpu.push((float)(1000.0 * (double)(res[s.qour[slot]] - res[s.qour[slot] - 1]) / (double)s.freq));
            s.qbusy[slot] = false;
        }
    }
}

void on_present(effect_runtime *rt)
{
    guard g;
    rt_state *s = state(rt);

    if (!s) {
        return;
    }
    ensure_init();
    poll_config();
    /* дескрипторы живут до перезагрузки эффектов, а события о начале перезагрузки в API нет, и перезагрузка одного
     * эффекта могла не дать reloaded_effects: перепривязываем раз в полсекунды, чтобы не держать устаревшие.
     * заодно подхватывается эффект, установленный позже аддона */
    if (GetTickCount64() >= s->next_bind) {
        s->next_bind = GetTickCount64() + POLL_MS;
        bind_effect(rt, *s);
    }
    if (s->pushed_version != G.version) {
        push_uniforms(rt, *s);
    }
    s->want_marker = s->tech_ok && s->uniforms_found == 11 && rt->get_effects_state()
                     && rt->get_technique_state(s->tech);
    update_marker();

    if (overlay_recent(*s)) {
        LONGLONG now = qpc_now();

        if (s->last_present) {
            s->frame_ms.push((float)qpc_ms(now - s->last_present));
        }
        s->last_present = now;
        if (s->timing_ok) {
            collect_timing(rt, *s);
        }
    } else {
        s->last_present = 0;
    }
    s->frame++;
}

/* потребление видеокарты через NVML, четыре раза в секунду и только пока вкладка открыта */
void poll_power(effect_runtime *rt)
{
    ULONGLONG now = GetTickCount64();
    ff_power_sample p;

    if (!G.power_tried) {
        char desc[256] = "";

        G.power_tried = true;
        rt->get_device()->get_property(device_properties::description, desc);
        G.power_ok = power_open(desc) == 0;
    }
    if (!G.power_ok || now < G.next_power) {
        return;
    }
    G.next_power = now + POWER_MS;
    power_read(&p);
    if (p.watts >= 0.0f) {
        G.watts.push(p.watts);
        G.util.push(p.util_gpu);
        G.clock_mhz = p.clock_mhz;
    }
}

void plot(const char *id, const ring &r, const char *text, ImVec2 size)
{
    ImGui::PlotLines(id, r.v, r.count, r.offset(), text, FLT_MAX, FLT_MAX, size, (int)sizeof(float));
}

/* строка сводки: среднее и p95 по кольцу */
void stat_line(const char *label, const ring &r, ff_stat *out)
{
    ff_stat_compute(r.v, (unsigned)r.count, out);
    if (r.count == 0) {
        ImGui::Text("%s: -", label);
    } else {
        ImGui::Text("%s: avg %.3f  p95 %.3f  max %.3f ms", label, (double)out->mean, (double)out->p95,
                    (double)out->max);
    }
}

void slider(const char *label, float *v, float lim, bool *changed, bool *commit)
{
    if (ImGui::SliderFloat(label, v, -lim, lim, "%.3f", 0)) {
        *changed = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        *commit = true;
    }
}

void draw_overlay(effect_runtime *rt)
{
    static const char *const NAMES[6] = { "Red horizontal", "Red vertical", "Green horizontal",
                                          "Green vertical", "Blue horizontal", "Blue vertical" };
    guard g;
    rt_state *s = state(rt);
    settings view;
    bool editable;
    bool changed = false;
    bool commit = false;
    ff_stat st;

    if (!s) {
        return;
    }
    ensure_init();
    s->overlay_seen = true;
    s->overlay_frame = s->frame;
    create_timing(rt, *s);
    poll_power(rt);

    ImGui::Text("fringefix %s", FF_VERSION);
    if (!s->tech_ok || s->uniforms_found != 11) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                           "fringefix.fx is not loaded. Reinstall from the fringefix app or copy it into the "
                           "ReShade shaders folder, then press Reload.");
    } else if (!rt->get_technique_state(s->tech)) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                           "The fringefix technique is off in the Home tab. The fringefix app overlay stays paused "
                           "only while it is on.");
    }
    ImGui::Text("Source: %s", G.source == SRC_APP   ? "fringefix app, live"
                              : G.source == SRC_INI ? "fringefix.ini of the app"
                                                    : "own values of this tab");
    if (ImGui::Checkbox("Follow the fringefix app", &G.follow_app)) {
        G.next_poll = 0;
        save_own();
    }

    ImGui::SeparatorText("Shifts and sharpness");
    editable = G.source == SRC_OWN;
    view = editable ? G.own : G.eff;
    ImGui::BeginDisabled(!editable);
    for (int i = 0; i < 6; i++) {
        slider(NAMES[i], &view.shift[i], (float)FF_SHIFT_LIMIT, &changed, &commit);
    }
    slider("Sharpness", &view.sharpen, (float)FF_SHARPEN_LIMIT, &changed, &commit);
    ImGui::EndDisabled();
    if (editable && changed) {
        G.own = view;
        G.next_poll = 0;
    }
    if (editable && commit) {
        save_own();
    }
    if (editable && ImGui::Button("Reset own values", ImVec2(0, 0))) {
        G.own = settings();
        G.next_poll = 0;
        save_own();
    }
    if (!editable) {
        ImGui::TextDisabled("Edit in the fringefix app, or untick the box above to use own values.");
    }

    ImGui::SeparatorText("Timings while this tab is open");
    if (s->timing_ok) {
        stat_line("GPU, fringefix pass", s->gpu, &st);
    } else {
        ImGui::TextDisabled("GPU timestamps are not available in this graphics API.");
    }
    stat_line("CPU, ReShade effects", s->cpu, &st);
    stat_line("Frame interval", s->frame_ms, &st);
    if (s->frame_ms.count > 0 && st.mean > 0.0f) {
        ImGui::Text("Frame rate: %.1f", 1000.0 / (double)st.mean);
    }
    if (G.power_ok && G.watts.count > 0) {
        ImGui::Text("GPU power %.1f W, load %.0f %%, clock %.0f MHz", (double)G.watts.v[(G.watts.head + RING - 1) % RING],
                    (double)G.util.v[(G.util.head + RING - 1) % RING], (double)G.clock_mhz);
    } else {
        ImGui::TextDisabled("GPU power: NVIDIA drivers only (nvml.dll).");
    }

    if (s->timing_ok) {
        plot("##gpu", s->gpu, "GPU ms per frame", ImVec2(0, 56));
    }
    plot("##frame", s->frame_ms, "frame interval ms", ImVec2(0, 56));
    if (G.power_ok) {
        plot("##pwr", G.watts, "GPU power W", ImVec2(0, 56));
    }
    if (ImGui::Button("Reset timings", ImVec2(0, 0))) {
        s->gpu = ring();
        s->cpu = ring();
        s->frame_ms = ring();
        G.watts = ring();
        G.util = ring();
    }
}

}  // namespace

/* ReShade читает эти две строки из экспорта аддона */
extern "C" {
__declspec(dllexport) const char *NAME = "fringefix";
__declspec(dllexport) const char *DESCRIPTION =
    "Colour fringe correction for QD-OLED with the settings of the fringefix app.";
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        if (!reshade::register_addon(module)) {
            return FALSE;
        }
        InitializeCriticalSection(&G.lock);
        reshade::register_event<addon_event::init_effect_runtime>(on_init);
        reshade::register_event<addon_event::destroy_effect_runtime>(on_destroy);
        reshade::register_event<addon_event::reshade_reloaded_effects>(on_reloaded);
        reshade::register_event<addon_event::reshade_set_current_preset_path>(on_preset);
        reshade::register_event<addon_event::reshade_begin_effects>(on_begin_effects);
        reshade::register_event<addon_event::reshade_render_technique>(on_render_technique);
        reshade::register_event<addon_event::reshade_finish_effects>(on_finish_effects);
        reshade::register_event<addon_event::reshade_present>(on_present);
        reshade::register_overlay("fringefix", draw_overlay);
        break;
    case DLL_PROCESS_DETACH:
        /* при завершении процесса (reserved не ноль) ReShade уже выгружен, трогать его нельзя */
        if (reserved == nullptr) {
            reshade::unregister_overlay("fringefix", draw_overlay);
            reshade::unregister_event<addon_event::reshade_present>(on_present);
            reshade::unregister_event<addon_event::reshade_finish_effects>(on_finish_effects);
            reshade::unregister_event<addon_event::reshade_render_technique>(on_render_technique);
            reshade::unregister_event<addon_event::reshade_begin_effects>(on_begin_effects);
            reshade::unregister_event<addon_event::reshade_set_current_preset_path>(on_preset);
            reshade::unregister_event<addon_event::reshade_reloaded_effects>(on_reloaded);
            reshade::unregister_event<addon_event::destroy_effect_runtime>(on_destroy);
            reshade::unregister_event<addon_event::init_effect_runtime>(on_init);
            reshade::unregister_addon(module);
        }
        /* nvml.dll не выгружается здесь: FreeLibrary под блокировкой загрузчика запрещён, а при выходе процесса
         * потоки NVML уже остановлены; библиотеку закроет сам процесс */
        live_addon_withdraw();
        live_read_close();
        break;
    }
    return TRUE;
}
