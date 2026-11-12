#include "sha256.h"

#include <stdio.h>
#include <string.h>

static int check(const char *data, size_t size, size_t step, const char *expected)
{
    struct cv_sha256 hash;
    unsigned char digest[CV_SHA256_SIZE];
    char actual[CV_SHA256_SIZE * 2 + 1];
    const char hex[] = "0123456789abcdef";

    cv_sha256_init(&hash);
    for (size_t offset = 0; offset < size;) {
        size_t remaining = size - offset;
        size_t count = remaining < step ? remaining : step;
        cv_sha256_update(&hash, data + offset, count);
        offset += count;
    }
    cv_sha256_finish(&hash, digest);
    for (size_t i = 0; i < sizeof digest; ++i) {
        actual[i * 2] = hex[digest[i] >> 4];
        actual[i * 2 + 1] = hex[digest[i] & 15];
    }
    actual[sizeof actual - 1] = '\0';
    if (strcmp(actual, expected) == 0) {
        return 0;
    }
    fprintf(stderr, "SHA-256 mismatch for %zu bytes: %s\n", size, actual);
    return 1;
}

int main(void)
{
    static char million[1000000];
    const char long_vector[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    memset(million, 'a', sizeof million);

    return check("", 0, 1, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")
        || check("abc", 3, 1, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
        || check(long_vector, sizeof long_vector - 1, 17,
                 "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1")
        || check(million, sizeof million, 137,
                 "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}
