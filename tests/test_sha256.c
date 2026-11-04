#include <stdlib.h>
#include <string.h>

#include "sha256.h"
#include "test.h"

static int digest_is(const void *data, size_t len, const char *want)
{
    unsigned char d[32];
    char hex[65];

    if (ff_sha256(data, len, d) != 0) {
        return 0;
    }
    ff_sha256_hex(d, hex);
    return strcmp(hex, want) == 0;
}

int main(void)
{
    /* векторы из FIPS 180-4: пустая строка, один блок, два блока с хвостом 56 байт, миллион «a» */
    CHECK(digest_is("", 0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    CHECK(digest_is("abc", 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK(digest_is("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
                    "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    {
        char *big = malloc(1000000);

        CHECK(big != NULL);
        if (big) {
            memset(big, 'a', 1000000);
            CHECK(digest_is(big, 1000000,
                            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
            free(big);
        }
    }
    TEST_DONE("sha256");
}
