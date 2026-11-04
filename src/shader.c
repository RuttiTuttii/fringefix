#define COBJMACROS
#include <windows.h>
#include <stdio.h>

#include "shader.h"

/* сдвиг каждого канала: одно аппаратное билинейное чтение плюс лапласиан, который возвращает
 * резкость, потерянную при интерполяции; для HDR (FP16) значения не зажимаются в 0..1 */
static const char HLSL[] =
    "Texture2D src : register(t0);\n"
    "SamplerState lin : register(s0);\n"
    "cbuffer cb : register(b0) {\n"
    "    float4 shiftRG;\n"   /* rx, ry, gx, gy */
    "    float4 shiftBX;\n"   /* bx, by, sharpen, hdr */
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
    "    int2 q = int2(floor(p));\n"
    "    float lap = ld(q + int2(-1, 0), ch) + ld(q + int2(1, 0), ch)\n"
    "              + ld(q + int2(0, -1), ch) + ld(q + int2(0, 1), ch) - 4.0 * ld(q, ch);\n"
    "    float k = min(1.72 * dot(f, f), 0.35) + shiftBX.z * 0.25;\n"
    "    v -= k * lap;\n"
    "    return shiftBX.w > 0.5 ? v : saturate(v);\n"
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

HRESULT ff_shader_compile(const char *entry, const char *target, ID3DBlob **blob, char *err,
                          size_t err_len)
{
    ID3DBlob *msg = NULL;
    HRESULT hr = D3DCompile(HLSL, sizeof HLSL - 1, "fringefix.hlsl", NULL, NULL, entry, target,
                            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, blob, &msg);

    if (FAILED(hr)) {
        snprintf(err, err_len, "shader %s: %s", entry,
                 msg ? (const char *)ID3D10Blob_GetBufferPointer(msg) : "compile failed");
    }
    if (msg) {
        ID3D10Blob_Release(msg);
    }
    return hr;
}
