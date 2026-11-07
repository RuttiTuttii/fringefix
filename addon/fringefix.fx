/* fringefix для ReShade: тот же фильтр, что движок 2 приложения, на кадре игры.
 * сдвиг каждого канала плюс лапласиан сведены к одному окну 3x3 на канал; веса считает аддон fringefix.addon64
 * (src/kernel.c) и кладёт в служебные uniform'ы ниже. сам по себе эффект без аддона ничего не меняет:
 * значения по умолчанию это единичное ядро. ставь эффект последним в списке: каёмка зависит от итоговой картинки */

texture ff_back : COLOR;
sampler ff_src { Texture = ff_back; };

/* смещение центра окна по каналам: r.x, r.y, g.x, g.y */
uniform float4 ff_base < hidden = true; > = float4(0.0, 0.0, 0.0, 0.0);
/* b.x, b.y, 1 если окна всех каналов стоят в одной клетке, 1 если фильтр включён */
uniform float4 ff_base2 < hidden = true; > = float4(0.0, 0.0, 1.0, 1.0);
/* веса: ff_kw{канал * 3 + строка} = (левый, средний, правый, 0); по умолчанию единичное ядро */
uniform float4 ff_kw0 < hidden = true; > = float4(0.0, 0.0, 0.0, 0.0);
uniform float4 ff_kw1 < hidden = true; > = float4(0.0, 1.0, 0.0, 0.0);
uniform float4 ff_kw2 < hidden = true; > = float4(0.0, 0.0, 0.0, 0.0);
uniform float4 ff_kw3 < hidden = true; > = float4(0.0, 0.0, 0.0, 0.0);
uniform float4 ff_kw4 < hidden = true; > = float4(0.0, 1.0, 0.0, 0.0);
uniform float4 ff_kw5 < hidden = true; > = float4(0.0, 0.0, 0.0, 0.0);
uniform float4 ff_kw6 < hidden = true; > = float4(0.0, 0.0, 0.0, 0.0);
uniform float4 ff_kw7 < hidden = true; > = float4(0.0, 1.0, 0.0, 0.0);
uniform float4 ff_kw8 < hidden = true; > = float4(0.0, 0.0, 0.0, 0.0);

/* пиксель с зажимом координат к краю кадра, как в движке приложения */
float3 ff_px(int2 p)
{
    return tex2Dfetch(ff_src, clamp(p, int2(0, 0), int2(BUFFER_WIDTH - 1, BUFFER_HEIGHT - 1))).rgb;
}

/* один канал окна 3x3 по трём строкам из трёх пикселей; mask выбирает канал */
float ff_row(float3 a, float3 b, float3 c, float3 mask, float4 w)
{
    return dot(w.xyz, float3(dot(a, mask), dot(b, mask), dot(c, mask)));
}

/* канал с собственной клеткой: девять чтений на канал */
float ff_chan(int2 o, float3 mask, float4 w0, float4 w1, float4 w2)
{
    return ff_row(ff_px(o + int2(-1, -1)), ff_px(o + int2(0, -1)), ff_px(o + int2(1, -1)), mask, w0)
         + ff_row(ff_px(o + int2(-1, 0)), ff_px(o), ff_px(o + int2(1, 0)), mask, w1)
         + ff_row(ff_px(o + int2(-1, 1)), ff_px(o + int2(0, 1)), ff_px(o + int2(1, 1)), mask, w2);
}

void ff_vs(uint id : SV_VertexID, out float4 pos : SV_Position, out float2 uv : TEXCOORD0)
{
    uv.x = (id == 2) ? 2.0 : 0.0;
    uv.y = (id == 1) ? 2.0 : 0.0;
    pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

float4 ff_ps(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    int2 o = int2(pos.xy);
    float3 r;

    if (ff_base2.w < 0.5) {
        return float4(ff_px(o), 1.0);
    }
    if (ff_base2.z > 0.5) {
        /* окна всех каналов в одной клетке: девять пикселей читаются один раз на три канала */
        int2 q = o + int2(ff_base.xy);
        float3 t00 = ff_px(q + int2(-1, -1)); float3 t10 = ff_px(q + int2(0, -1)); float3 t20 = ff_px(q + int2(1, -1));
        float3 t01 = ff_px(q + int2(-1, 0));  float3 t11 = ff_px(q);               float3 t21 = ff_px(q + int2(1, 0));
        float3 t02 = ff_px(q + int2(-1, 1));  float3 t12 = ff_px(q + int2(0, 1));  float3 t22 = ff_px(q + int2(1, 1));

        r.x = ff_row(t00, t10, t20, float3(1, 0, 0), ff_kw0) + ff_row(t01, t11, t21, float3(1, 0, 0), ff_kw1)
            + ff_row(t02, t12, t22, float3(1, 0, 0), ff_kw2);
        r.y = ff_row(t00, t10, t20, float3(0, 1, 0), ff_kw3) + ff_row(t01, t11, t21, float3(0, 1, 0), ff_kw4)
            + ff_row(t02, t12, t22, float3(0, 1, 0), ff_kw5);
        r.z = ff_row(t00, t10, t20, float3(0, 0, 1), ff_kw6) + ff_row(t01, t11, t21, float3(0, 0, 1), ff_kw7)
            + ff_row(t02, t12, t22, float3(0, 0, 1), ff_kw8);
    } else {
        r.x = ff_chan(o + int2(ff_base.xy), float3(1, 0, 0), ff_kw0, ff_kw1, ff_kw2);
        r.y = ff_chan(o + int2(ff_base.zw), float3(0, 1, 0), ff_kw3, ff_kw4, ff_kw5);
        r.z = ff_chan(o + int2(ff_base2.xy), float3(0, 0, 1), ff_kw6, ff_kw7, ff_kw8);
    }
    /* в FP16 (HDR) значения не зажимаются, как в приложении */
#if BUFFER_COLOR_BIT_DEPTH != 16
    r = saturate(r);
#endif
    return float4(r, 1.0);
}

technique fringefix < enabled = true; ui_label = "fringefix"; ui_tooltip = "Colour fringe correction for QD-OLED. Settings are in the fringefix tab."; >
{
    pass
    {
        VertexShader = ff_vs;
        PixelShader = ff_ps;
    }
}
