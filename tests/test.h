#ifndef FRINGEFIX_TEST_H
#define FRINGEFIX_TEST_H

#include <stdio.h>

/* минимальный раннер без фреймворков: CHECK считает провалы, TEST_DONE печатает итог и код выхода */
static int failed;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failed++;                                              \
        }                                                          \
    } while (0)

#define TEST_DONE(name)                         \
    do {                                        \
        if (failed) {                           \
            printf("%s: %d failed\n", name, failed); \
            return 1;                           \
        }                                       \
        printf("%s: all tests passed\n", name); \
        return 0;                               \
    } while (0)

#endif
