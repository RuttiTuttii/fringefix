#ifndef FRINGEFIX_COM_H
#define FRINGEFIX_COM_H

/* освобождает COM-указатель и обнуляет его; работает и без COBJMACROS */
#define FF_REL(p)                                                    \
    do {                                                             \
        if (p) {                                                     \
            ((IUnknown *)(p))->lpVtbl->Release((IUnknown *)(p));     \
            (p) = NULL;                                              \
        }                                                            \
    } while (0)

/* внутренние помощники gfx*.c: ждут в области видимости hr, err (GFX_ERR_LEN байт) и метку done,
 * поэтому gfx.h подключается раньше */
#define FAILMSG(text)                           \
    do {                                        \
        snprintf(err, GFX_ERR_LEN, "%s", text); \
        goto done;                              \
    } while (0)

#define TRY(call, what)                                                                 \
    do {                                                                                \
        hr = (call);                                                                    \
        if (FAILED(hr)) {                                                               \
            snprintf(err, GFX_ERR_LEN, "%s failed (0x%08lx)", what, (unsigned long)hr); \
            goto done;                                                                  \
        }                                                                               \
    } while (0)

#endif
