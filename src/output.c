#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "com.h"
#include "output.h"

/* ненулевой возврат останавливает перебор */
typedef int (*output_fn)(int n, IDXGIAdapter1 *adapter, IDXGIOutput *output, void *ctx);

static void each_output(output_fn fn, void *ctx)
{
    IDXGIFactory1 *fac = NULL;
    IDXGIAdapter1 *ad = NULL;
    int n = 0;
    int stop = 0;
    UINT ai;

    if (FAILED(CreateDXGIFactory1(&IID_IDXGIFactory1, (void **)&fac))) {
        return;
    }
    for (ai = 0; !stop && IDXGIFactory1_EnumAdapters1(fac, ai, &ad) == S_OK; ai++) {
        IDXGIOutput *o = NULL;
        UINT oi;

        for (oi = 0; !stop && IDXGIAdapter1_EnumOutputs(ad, oi, &o) == S_OK; oi++) {
            stop = fn(n++, ad, o, ctx);
            IDXGIOutput_Release(o);
        }
        IDXGIAdapter1_Release(ad);
    }
    IDXGIFactory1_Release(fac);
}

typedef struct {
    int index;
    IDXGIAdapter1 *adapter;
    IDXGIOutput6 *output;
} pick_ctx;

static int pick_cb(int n, IDXGIAdapter1 *ad, IDXGIOutput *out, void *ctx)
{
    pick_ctx *p = ctx;

    if (n != p->index) {
        return 0;
    }
    if (SUCCEEDED(IDXGIOutput_QueryInterface(out, &IID_IDXGIOutput6, (void **)&p->output))) {
        IDXGIAdapter1_AddRef(ad);
        p->adapter = ad;
    }
    return 1;
}

int ff_output_pick(int index, IDXGIAdapter1 **adapter, IDXGIOutput6 **output)
{
    pick_ctx p = { index, NULL, NULL };

    each_output(pick_cb, &p);
    if (!p.output) {
        return -1;
    }
    *adapter = p.adapter;
    *output = p.output;
    return 0;
}

typedef struct {
    wchar_t (*names)[OUTPUT_NAME_LEN];
    int max;
    int count;
} list_ctx;

static int list_cb(int n, IDXGIAdapter1 *ad, IDXGIOutput *out, void *ctx)
{
    list_ctx *l = ctx;
    DXGI_OUTPUT_DESC d;

    (void)ad;
    if (SUCCEEDED(IDXGIOutput_GetDesc(out, &d))) {
        _snwprintf(l->names[n], OUTPUT_NAME_LEN, L"%d: %ls (%ldx%ld)", n, d.DeviceName,
                   (long)(d.DesktopCoordinates.right - d.DesktopCoordinates.left),
                   (long)(d.DesktopCoordinates.bottom - d.DesktopCoordinates.top));
        l->names[n][OUTPUT_NAME_LEN - 1] = L'\0';
        l->count = n + 1;
    }
    return n + 1 >= l->max;
}

int ff_output_list(wchar_t names[][OUTPUT_NAME_LEN], int max)
{
    list_ctx l = { names, max, 0 };

    each_output(list_cb, &l);
    return l.count;
}

typedef struct {
    wchar_t (*names)[32];
    int max;
    int count;
} dev_ctx;

static int dev_cb(int n, IDXGIAdapter1 *ad, IDXGIOutput *out, void *ctx)
{
    dev_ctx *l = ctx;
    DXGI_OUTPUT_DESC d;

    (void)ad;
    if (SUCCEEDED(IDXGIOutput_GetDesc(out, &d))) {
        wcsncpy(l->names[n], d.DeviceName, 31);
        l->names[n][31] = L'\0';
        l->count = n + 1;
    }
    return n + 1 >= l->max;
}

int ff_output_devices(wchar_t names[][32], int max)
{
    dev_ctx l = { names, max, 0 };

    each_output(dev_cb, &l);
    return l.count;
}

int ff_output_info_get(int index, ff_output_info *info)
{
    IDXGIAdapter1 *ad = NULL;
    IDXGIOutput6 *out = NULL;
    DXGI_OUTPUT_DESC1 d;
    DXGI_ADAPTER_DESC1 ad_desc;
    int rc = -1;

    memset(info, 0, sizeof *info);
    if (ff_output_pick(index, &ad, &out) != 0) {
        return -1;
    }
    if (SUCCEEDED(IDXGIOutput6_GetDesc1(out, &d))) {
        info->width = (unsigned)(d.DesktopCoordinates.right - d.DesktopCoordinates.left);
        info->height = (unsigned)(d.DesktopCoordinates.bottom - d.DesktopCoordinates.top);
        info->hdr = d.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
        rc = 0;
    }
    if (SUCCEEDED(IDXGIAdapter1_GetDesc1(ad, &ad_desc))) {
        WideCharToMultiByte(CP_UTF8, 0, ad_desc.Description, -1, info->gpu, sizeof info->gpu, NULL, NULL);
        info->gpu[sizeof info->gpu - 1] = '\0';
    }
    FF_REL(out);
    FF_REL(ad);
    return rc;
}

int ff_output_rect(int index, RECT *rect)
{
    IDXGIAdapter1 *ad = NULL;
    IDXGIOutput6 *out = NULL;
    DXGI_OUTPUT_DESC1 d;
    int rc = -1;

    if (ff_output_pick(index, &ad, &out) != 0) {
        return -1;
    }
    if (SUCCEEDED(IDXGIOutput6_GetDesc1(out, &d))) {
        *rect = d.DesktopCoordinates;
        rc = 0;
    }
    FF_REL(out);
    FF_REL(ad);
    return rc;
}

HMONITOR ff_output_hmonitor(int index)
{
    IDXGIAdapter1 *ad = NULL;
    IDXGIOutput6 *out = NULL;
    DXGI_OUTPUT_DESC1 d;
    HMONITOR mon = NULL;

    if (ff_output_pick(index, &ad, &out) != 0) {
        return NULL;
    }
    if (SUCCEEDED(IDXGIOutput6_GetDesc1(out, &d))) {
        mon = d.Monitor;
    }
    FF_REL(out);
    FF_REL(ad);
    return mon;
}
