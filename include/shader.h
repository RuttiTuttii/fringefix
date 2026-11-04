#ifndef FRINGEFIX_SHADER_H
#define FRINGEFIX_SHADER_H

#include <d3d11.h>
#include <d3dcompiler.h>
#include <stddef.h>

/* компилирует точку входа из встроенного HLSL (vsmain, psmain); текст ошибки компилятора кладёт в err */
HRESULT ff_shader_compile(const char *entry, const char *target, ID3DBlob **blob, char *err,
                          size_t err_len);

#endif
