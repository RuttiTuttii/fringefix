#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "shader.h"

/* сдвиг каждого канала: одно аппаратное билинейное чтение плюс лапласиан, который возвращает
 * резкость, потерянную при интерполяции; для HDR (FP16) значения не зажимаются в 0..1 */
static const char HLSL[] =
    "Texture2D src : register(t0);\n"
    "SamplerState lin : register(s0);\n"
    "cbuffer cb : register(b0) {\n"
    "    float4 shiftRG;\n"   /* rx, ry, gx, gy */
    "    float4 shiftBX;\n"   /* bx, by, sharpen, hdr */
    "    float4 mode;\n"      /* light, 0, 0, 0 */
    "};\n"
    "static float2 size;\n"
    "float ld(int2 c, int ch) {\n"
    "    c = clamp(c, int2(0, 0), int2(size) - 1);\n"
    "    float4 t = src.Load(int3(c, 0));\n"
    "    return t[ch];\n"
    "}\n"
    "float chan(float2 pix, float2 s, int ch) {\n"
    "    float2 f = s - round(s);\n"
    "    float2 p = pix - s;\n"
    "    float4 t = src.SampleLevel(lin, p / size, 0);\n"
    "    float v = t[ch];\n"
    "    if (mode.x > 0.5) {\n"
    "        return shiftBX.w > 0.5 ? v : saturate(v);\n"
    "    }\n"
    "    int2 q = int2(floor(p));\n"
    "    float lap = ld(q + int2(-1, 0), ch) + ld(q + int2(1, 0), ch)\n"
    "              + ld(q + int2(0, -1), ch) + ld(q + int2(0, 1), ch) - 4.0 * ld(q, ch);\n"
    "    float k = min(1.72 * dot(f, f), 0.35) + shiftBX.z * 0.35;\n"
    "    v -= k * lap;\n"
    "    return shiftBX.w > 0.5 ? v : saturate(v);\n"
    "}\n"
    /* сравнение кадров: пиксель вывода покрывает блок 16x16, единица значит что в блоке есть отличие;
     * сравниваются биты, поэтому NaN в HDR не считается изменением */
    "Texture2D prev : register(t1);\n"
    "uint4 psig(float4 pos : SV_Position) : SV_Target {\n"
    "    float w, h;\n"
    "    src.GetDimensions(w, h);\n"
    "    int2 o = int2(pos.xy) * 16;\n"
    "    [loop] for (int y = 0; y < 16; y++) {\n"
    "        [loop] for (int x = 0; x < 16; x++) {\n"
    "            int2 p = o + int2(x, y);\n"
    "            if (p.x < (int)w && p.y < (int)h) {\n"
    "                uint3 a = asuint(src.Load(int3(p, 0)).rgb);\n"
    "                uint3 b = asuint(prev.Load(int3(p, 0)).rgb);\n"
    "                if (any(a != b)) {\n"
    "                    return uint4(1, 0, 0, 0);\n"
    "                }\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "    return uint4(0, 0, 0, 0);\n"
    "}\n"
    "float4 vsmain(uint id : SV_VertexID) : SV_Position {\n"
    "    float2 uv = float2((id << 1) & 2, id & 2);\n"
    "    return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);\n"
    "}\n"
    "float4 psmain(float4 pos : SV_Position) : SV_Target {\n"
    "    float w, h;\n"
    "    src.GetDimensions(w, h);\n"
    "    size = float2(w, h);\n"
    "    float2 sr = shiftRG.xy, sg = shiftRG.zw, sb = shiftBX.xy;\n"
    /* общий для всех каналов сдвиг не виден, зато доли пикселя становятся меньше */
    "    float2 c = -(min(sr, min(sg, sb)) + max(sr, max(sg, sb))) * 0.5;\n"
    "    return float4(chan(pos.xy, sr + c, 0), chan(pos.xy, sg + c, 1), chan(pos.xy, sb + c, 2), 1);\n"
    "}\n";

/* расширения движка (бета) лежат в отдельном тексте: ошибка в нём не должна ломать классический путь.
 * константный буфер здесь шире классического: после трёх float4 идут окна ядра 3x3 (см. kernel.h) */
static const char HLSL_EXT[] =
    "Texture2D src : register(t0);\n"
    "Texture2D prev : register(t1);\n"
    "cbuffer cb : register(b0) {\n"
    "    float4 shiftRG;\n"
    "    float4 shiftBX;\n"       /* bx, by, sharpen, hdr */
    "    float4 mode;\n"
    "    int4 kbase;\n"           /* rx, ry, gx, gy */
    "    int4 kbase2;\n"          /* bx, by, shared, 0 */
    "    float4 kw[9];\n"         /* kw[канал * 3 + строка] = (w0, w1, w2, 0) */
    "};\n"
    /* то же самое готовым окном 3x3 на канал: веса посчитаны на процессоре (kernel.c), шейдер только читает
     * пиксели и умножает. при общей клетке девять пикселей читаются один раз на все три канала */
    "float4 ldk(int2 c, uint2 sz) {\n"
    "    return src.Load(int3(clamp(c, int2(0, 0), int2(sz) - 1), 0));\n"
    "}\n"
    "float4 psk(float4 pos : SV_Position) : SV_Target {\n"
    "    uint2 sz;\n"
    "    src.GetDimensions(sz.x, sz.y);\n"
    "    int2 o = int2(pos.xy);\n"
    "    int2 bs[3] = { kbase.xy, kbase.zw, kbase2.xy };\n"
    "    float3 r;\n"
    "    if (kbase2.z != 0) {\n"
    "        float4 t[9];\n"
    "        [unroll] for (int j = 0; j < 3; j++) {\n"
    "            [unroll] for (int i = 0; i < 3; i++) {\n"
    "                t[j * 3 + i] = ldk(o + bs[0] + int2(i - 1, j - 1), sz);\n"
    "            }\n"
    "        }\n"
    "        [unroll] for (int c = 0; c < 3; c++) {\n"
    "            float v = 0;\n"
    "            [unroll] for (int j = 0; j < 3; j++) {\n"
    "                v += kw[c * 3 + j].x * t[j * 3][c] + kw[c * 3 + j].y * t[j * 3 + 1][c]\n"
    "                   + kw[c * 3 + j].z * t[j * 3 + 2][c];\n"
    "            }\n"
    "            r[c] = v;\n"
    "        }\n"
    "    } else {\n"
    "        [unroll] for (int c = 0; c < 3; c++) {\n"
    "            float v = 0;\n"
    "            [unroll] for (int j = 0; j < 3; j++) {\n"
    "                [unroll] for (int i = 0; i < 3; i++) {\n"
    "                    float4 px = ldk(o + bs[c] + int2(i - 1, j - 1), sz);\n"
    "                    v += kw[c * 3 + j][i] * px[c];\n"
    "                }\n"
    "            }\n"
    "            r[c] = v;\n"
    "        }\n"
    "    }\n"
    "    return float4(shiftBX.w > 0.5 ? r : saturate(r), 1);\n"
    "}\n"
    /* сравнение кадров вычислительным шейдером: группа 16x16 потоков на блок 16x16, по пикселю на поток,
     * флаг блока собирается в общей памяти группы; результат тот же, что у psig, но на 8,3 млн потоков вместо 32 тысяч */
    "RWTexture2D<uint> flags : register(u0);\n"
    "groupshared uint any_diff;\n"
    "[numthreads(16, 16, 1)]\n"
    "void csdiff(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID) {\n"
    "    if (tid.x == 0 && tid.y == 0) {\n"
    "        any_diff = 0;\n"
    "    }\n"
    "    GroupMemoryBarrierWithGroupSync();\n"
    "    uint w, h;\n"
    "    src.GetDimensions(w, h);\n"
    "    int2 p = int2(gid.xy) * 16 + int2(tid.xy);\n"
    "    if (p.x < (int)w && p.y < (int)h) {\n"
    "        uint3 a = asuint(src.Load(int3(p, 0)).rgb);\n"
    "        uint3 b = asuint(prev.Load(int3(p, 0)).rgb);\n"
    "        if (any(a != b)) {\n"
    "            InterlockedOr(any_diff, 1);\n"
    "        }\n"
    "    }\n"
    "    GroupMemoryBarrierWithGroupSync();\n"
    "    if (tid.x == 0 && tid.y == 0) {\n"
    "        flags[gid.xy] = any_diff;\n"
    "    }\n"
    "}\n";

/* скомпилированные шейдеры живут до конца процесса: устройство пересоздаётся при каждом выходе из паузы,
 * а сборка расширений движка занимает около 60 мс. вызывается только из потока захвата */
#define CACHE_N 8
static struct {
    char entry[16];
    ID3DBlob *blob;
} g_cache[CACHE_N];

HRESULT ff_shader_compile(const char *entry, const char *target, ID3DBlob **blob, char *err,
                          size_t err_len)
{
    ID3DBlob *msg = NULL;
    int ext = strcmp(entry, "psk") == 0 || strcmp(entry, "csdiff") == 0;
    HRESULT hr;
    int i;

    for (i = 0; i < CACHE_N; i++) {
        if (g_cache[i].blob && strcmp(g_cache[i].entry, entry) == 0) {
            ID3D10Blob_AddRef(g_cache[i].blob);
            *blob = g_cache[i].blob;
            return S_OK;
        }
    }
    hr = D3DCompile(ext ? HLSL_EXT : HLSL, (ext ? sizeof HLSL_EXT : sizeof HLSL) - 1, "fringefix.hlsl",
                    NULL, NULL, entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, blob, &msg);

    if (FAILED(hr)) {
        snprintf(err, err_len, "shader %s: %s", entry,
                 msg ? (const char *)ID3D10Blob_GetBufferPointer(msg) : "compile failed");
    } else {
        for (i = 0; i < CACHE_N; i++) {
            if (!g_cache[i].blob && strlen(entry) < sizeof g_cache[i].entry) {
                strcpy(g_cache[i].entry, entry);
                ID3D10Blob_AddRef(*blob);
                g_cache[i].blob = *blob;
                break;
            }
        }
    }
    if (msg) {
        ID3D10Blob_Release(msg);
    }
    return hr;
}
