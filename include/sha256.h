#ifndef FRINGEFIX_SHA256_H
#define FRINGEFIX_SHA256_H

#include <stddef.h>

/* sha-256 одним вызовом; 0 при успехе, -1 если данных больше 2^60 байт */
int ff_sha256(const void *data, size_t len, unsigned char out[32]);

/* 32 байта в строку из 64 маленьких hex-символов и нуля */
void ff_sha256_hex(const unsigned char in[32], char out[65]);

#endif
