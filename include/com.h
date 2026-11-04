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

#endif
