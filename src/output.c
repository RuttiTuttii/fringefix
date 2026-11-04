#define COBJMACROS
#include <windows.h>
#include <stdio.h>

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
