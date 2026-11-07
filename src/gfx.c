#define COBJMACROS
#include <windows.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bench.h"
#include "gfx.h"
#include "com.h"
#include "output.h"
#include "shader.h"

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

static const wchar_t OVERLAY_CLASS[] = L"fringefix_overlay";

void gfx_shutdown(ff_gfx *g)
{
    if (g->ctx) {
        ID3D11DeviceContext_ClearState(g->ctx);
    }
    gfx_prof_close(g);
    FF_REL(g->sig_uav);
    FF_REL(g->cs_diff);
    FF_REL(g->ps_k);
    free(g->meta);
    FF_REL(g->sig_stage);
    FF_REL(g->sig_rtv);
    FF_REL(g->sig_tex);
    FF_REL(g->sig_ps);
    FF_REL(g->cb);
    FF_REL(g->smp);
    FF_REL(g->ps);
    FF_REL(g->vs);
    FF_REL(g->rtv);
    FF_REL(g->sc);
    FF_REL(g->srv[0]);
    FF_REL(g->srv[1]);
    FF_REL(g->frame[0]);
    FF_REL(g->frame[1]);
    FF_REL(g->dup);
    FF_REL(g->ctx);
    FF_REL(g->dev);
    if (g->hwnd) {
        DestroyWindow(g->hwnd);
    }
    memset(g, 0, sizeof *g);
}

/* две копии кадра, текущая и предыдущая: дублированную текстуру шейдеру напрямую не отдать */
static int make_frames(ff_gfx *g, char *err)
{
    HRESULT hr;
    D3D11_TEXTURE2D_DESC td;
    int i;

    memset(&td, 0, sizeof td);
    td.Width = g->w;
    td.Height = g->h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = g->fmt;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    for (i = 0; i < 2; i++) {
        TRY(ID3D11Device_CreateTexture2D(g->dev, &td, NULL, &g->frame[i]), "CreateTexture2D");
        TRY(ID3D11Device_CreateShaderResourceView(g->dev, (ID3D11Resource *)g->frame[i], NULL,
                                                  &g->srv[i]), "CreateShaderResourceView");
    }
    return 0;

done:
    return -1;
}

/* устройство на адаптере монитора, захват рабочего стола и своя копия кадра для шейдера */
static int open_capture(ff_gfx *g, int monitor, RECT *rect, char *err)
{
    HRESULT hr;
    int rc = -1;
    IDXGIAdapter1 *ad = NULL;
    IDXGIOutput6 *out = NULL;
    DXGI_OUTPUT_DESC1 od;
    DXGI_OUTDUPL_DESC dd;
    DXGI_FORMAT want;

    if (ff_output_pick(monitor, &ad, &out) != 0) {
        FAILMSG("monitor not found");
    }
    {
        DXGI_ADAPTER_DESC1 desc;

        if (SUCCEEDED(IDXGIAdapter1_GetDesc1(ad, &desc))
            && WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, g->adapter, sizeof g->adapter, NULL,
                                   NULL) > 0) {
            bench_set_gpu_name(g->adapter);
        }
    }
    TRY(IDXGIOutput6_GetDesc1(out, &od), "GetDesc1");
    g->hdr = od.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
    *rect = od.DesktopCoordinates;

    TRY(D3D11CreateDevice((IDXGIAdapter *)ad, D3D_DRIVER_TYPE_UNKNOWN, NULL,
                          D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, D3D11_SDK_VERSION,
                          &g->dev, NULL, &g->ctx), "D3D11CreateDevice");

    /* формат кадра выбирает режим рабочего стола: FP16 scRGB в HDR, 8 бит в SDR */
    want = g->hdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM;
    TRY(IDXGIOutput6_DuplicateOutput1(out, (IUnknown *)g->dev, 0, 1, &want, &g->dup),
        "DuplicateOutput1");
    IDXGIOutputDuplication_GetDesc(g->dup, &dd);
    /* поверхность кадра приходит в запрошенном формате, а ModeDesc.Format это формат режима рабочего стола
     * и в HDR с ним не совпадает; настоящий формат ещё сверяется по первой полученной текстуре */
    g->fmt = want;
    g->w = dd.ModeDesc.Width;
    g->h = dd.ModeDesc.Height;
    /* у повёрнутого монитора поверхность лежит в ориентации панели, а окно и шейдер этого не учитывают:
     * вместо мусора поверх экрана остановка с понятной ошибкой */
    if (dd.Rotation != DXGI_MODE_ROTATION_IDENTITY && dd.Rotation != DXGI_MODE_ROTATION_UNSPECIFIED) {
        FAILMSG("rotated monitors are not supported, set the display orientation to landscape");
    }

    if (make_frames(g, err) != 0) {
        goto done;
    }
    rc = 0;

done:
    FF_REL(out);
    FF_REL(ad);
    return rc;
}

/* окно прозрачно для мыши и исключено из захвата: без этого кадр снимал бы сам оверлей */
static int open_window(ff_gfx *g, const RECT *rect, char *err)
{
    static int registered;

    if (!registered) {
        WNDCLASSW wc;

        memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = OVERLAY_CLASS;
        RegisterClassW(&wc);
        registered = 1;
    }
    g->hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT
                                  | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                              OVERLAY_CLASS, L"fringefix", WS_POPUP, rect->left, rect->top,
                              (int)g->w, (int)g->h, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!g->hwnd) {
        snprintf(err, GFX_ERR_LEN, "CreateWindowEx failed (%lu)", GetLastError());
        return -1;
    }
    SetLayeredWindowAttributes(g->hwnd, 0, 255, LWA_ALPHA);
    if (!SetWindowDisplayAffinity(g->hwnd, WDA_EXCLUDEFROMCAPTURE)) {
        snprintf(err, GFX_ERR_LEN, "SetWindowDisplayAffinity failed, need Windows 10 2004+");
        return -1;
    }
    return 0;
}

static int open_swapchain(ff_gfx *g, int gpu_priority, char *err)
{
    HRESULT hr;
    int rc = -1;
    IDXGIDevice *dxdev = NULL;
    IDXGIAdapter *ad = NULL;
    IDXGIFactory2 *fac = NULL;
    IDXGIDevice1 *dev1 = NULL;
    IDXGISwapChain3 *sc3 = NULL;
    DXGI_SWAP_CHAIN_DESC1 sd;

    TRY(ID3D11Device_QueryInterface(g->dev, &IID_IDXGIDevice, (void **)&dxdev), "IDXGIDevice");
    /* по умолчанию драйвер копит до трёх кадров в очереди, это прямая задержка; один кадр достаточно */
    if (SUCCEEDED(IDXGIDevice_QueryInterface(dxdev, &IID_IDXGIDevice1, (void **)&dev1))) {
        IDXGIDevice1_SetMaximumFrameLatency(dev1, 1);
    }
    /* ниже ноля оверлей уступает видеокарту игре; сбой тут не страшен, остаётся приоритет по умолчанию */
    IDXGIDevice_SetGPUThreadPriority(dxdev, gpu_priority);
    TRY(IDXGIDevice_GetAdapter(dxdev, &ad), "GetAdapter");
    TRY(IDXGIAdapter_GetParent(ad, &IID_IDXGIFactory2, (void **)&fac), "GetParent");

    memset(&sd, 0, sizeof sd);
    sd.Width = g->w;
    sd.Height = g->h;
    sd.Format = g->fmt;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    TRY(IDXGIFactory2_CreateSwapChainForHwnd(fac, (IUnknown *)g->dev, g->hwnd, &sd, NULL, NULL,
                                             &g->sc), "CreateSwapChainForHwnd");
    if (g->hdr) {
        TRY(IDXGISwapChain1_QueryInterface(g->sc, &IID_IDXGISwapChain3, (void **)&sc3),
            "IDXGISwapChain3");
        TRY(IDXGISwapChain3_SetColorSpace1(sc3, DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709),
            "SetColorSpace1");
    }
    rc = 0;

done:
    FF_REL(dev1);
    FF_REL(sc3);
    FF_REL(fac);
    FF_REL(ad);
    FF_REL(dxdev);
    return rc;
}

/* целевой вид на задний буфер цепочки */
static int make_rtv(ff_gfx *g, char *err)
{
    HRESULT hr;
    int rc = -1;
    ID3D11Texture2D *bb = NULL;

    TRY(IDXGISwapChain1_GetBuffer(g->sc, 0, &IID_ID3D11Texture2D, (void **)&bb), "GetBuffer");
    TRY(ID3D11Device_CreateRenderTargetView(g->dev, (ID3D11Resource *)bb, NULL, &g->rtv),
        "CreateRenderTargetView");
    rc = 0;

done:
    FF_REL(bb);
    return rc;
}

/* целевой буфер, шейдеры, константы и сэмплер */
static int open_pipeline(ff_gfx *g, char *err)
{
    HRESULT hr;
    int rc = -1;
    ID3DBlob *blob = NULL;
    D3D11_BUFFER_DESC bd;
    D3D11_SAMPLER_DESC sm;

    if (make_rtv(g, err) != 0) {
        goto done;
    }

    hr = ff_shader_compile("vsmain", "vs_5_0", &blob, err, GFX_ERR_LEN);
    if (FAILED(hr)) {
        goto done;
    }
    TRY(ID3D11Device_CreateVertexShader(g->dev, ID3D10Blob_GetBufferPointer(blob),
                                        ID3D10Blob_GetBufferSize(blob), NULL, &g->vs),
        "CreateVertexShader");
    FF_REL(blob);
    hr = ff_shader_compile("psmain", "ps_5_0", &blob, err, GFX_ERR_LEN);
    if (FAILED(hr)) {
        goto done;
    }
    TRY(ID3D11Device_CreatePixelShader(g->dev, ID3D10Blob_GetBufferPointer(blob),
                                       ID3D10Blob_GetBufferSize(blob), NULL, &g->ps),
        "CreatePixelShader");
    FF_REL(blob);
    /* шейдер ядра 3x3 нужен только движку 2: если он не собрался, оверлей работает классическим путём */
    {
        char e2[GFX_ERR_LEN];

        if (SUCCEEDED(ff_shader_compile("psk", "ps_5_0", &blob, e2, GFX_ERR_LEN))
            && FAILED(ID3D11Device_CreatePixelShader(g->dev, ID3D10Blob_GetBufferPointer(blob),
                                                     ID3D10Blob_GetBufferSize(blob), NULL, &g->ps_k))) {
            snprintf(e2, sizeof e2, "psk create failed");
        }
        if (!g->ps_k) {
            snprintf(g->note, sizeof g->note, "%s", e2);
        }
        FF_REL(blob);
    }

    memset(&bd, 0, sizeof bd);
    bd.ByteWidth = 224;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    TRY(ID3D11Device_CreateBuffer(g->dev, &bd, NULL, &g->cb), "CreateBuffer");

    memset(&sm, 0, sizeof sm);
    sm.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sm.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sm.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sm.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sm.MaxLOD = D3D11_FLOAT32_MAX;
    TRY(ID3D11Device_CreateSamplerState(g->dev, &sm, &g->smp), "CreateSamplerState");
    rc = 0;

done:
    FF_REL(blob);
    return rc;
}

int gfx_reformat(ff_gfx *g, DXGI_FORMAT fmt)
{
    HRESULT hr;
    char err[GFX_ERR_LEN];
    IDXGISwapChain3 *sc3 = NULL;
    int rc = -1;

    /* всё, что держит старый формат, отпускается до ResizeBuffers */
    ID3D11DeviceContext_ClearState(g->ctx);
    FF_REL(g->rtv);
    FF_REL(g->srv[0]);
    FF_REL(g->srv[1]);
    FF_REL(g->frame[0]);
    FF_REL(g->frame[1]);
    g->fmt = fmt;
    TRY(IDXGISwapChain1_ResizeBuffers(g->sc, 0, 0, 0, fmt, 0), "ResizeBuffers");
    if (g->hdr) {
        TRY(IDXGISwapChain1_QueryInterface(g->sc, &IID_IDXGISwapChain3, (void **)&sc3),
            "IDXGISwapChain3");
        TRY(IDXGISwapChain3_SetColorSpace1(sc3, DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709),
            "SetColorSpace1");
    }
    if (make_rtv(g, err) != 0 || make_frames(g, err) != 0) {
        goto done;
    }
    /* предыдущего кадра в новом формате нет: следующий считается первым и рисуется целиком */
    g->have_frame = 0;
    g->cur = 0;
    rc = 0;

done:
    FF_REL(sc3);
    return rc;
}

int gfx_init(ff_gfx *g, const ff_config *cfg, char *err)
{
    RECT rect;

    memset(g, 0, sizeof *g);
    err[0] = '\0';
    if (open_capture(g, cfg->monitor, &rect, err) != 0 || open_window(g, &rect, err) != 0
        || open_swapchain(g, cfg->gpu_priority, err) != 0 || open_pipeline(g, err) != 0
        || gfx_sig_open(g, err) != 0) {
        gfx_shutdown(g);
        return -1;
    }
    gfx_prof_open(g);
    g->alive = 1;
    return 0;
}
