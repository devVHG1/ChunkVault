#include "chunk.h"

#include <string.h>

static int check_chunks(FILE *file)
{
    unsigned char buffer[3];
    unsigned char digest[CV_SHA256_SIZE];
    unsigned char first_digest[CV_SHA256_SIZE];
    size_t size;

    if (cv_chunk_read(file, buffer, 0, &size, digest) != CV_CHUNK_ERROR || size != 0
        || cv_chunk_read(file, buffer, sizeof buffer, &size, first_digest) != CV_CHUNK_OK
        || size != 3 || memcmp(buffer, "abc", size) != 0
        || cv_chunk_read(file, buffer, sizeof buffer, &size, digest) != CV_CHUNK_OK
        || size != 3 || memcmp(buffer, "abc", size) != 0
        || memcmp(first_digest, digest, sizeof digest) != 0
        || cv_chunk_read(file, buffer, sizeof buffer, &size, digest) != CV_CHUNK_OK
        || size != 2 || buffer[0] != 0 || buffer[1] != 255
        || memcmp(first_digest, digest, sizeof digest) == 0
        || cv_chunk_read(file, buffer, sizeof buffer, &size, digest) != CV_CHUNK_END
        || size != 0) {
        fputs("Unexpected chunk data, digest, or end of stream\n", stderr);
        return 1;
    }
    return 0;
}

static int check_verification(FILE *file, const unsigned char *data, size_t size)
{
    struct cv_sha256 hash;
    unsigned char digest[CV_SHA256_SIZE];
    cv_sha256_init(&hash);
    cv_sha256_update(&hash, data, size);
    cv_sha256_finish(&hash, digest);

    if (fseek(file, 0, SEEK_SET) != 0 || cv_chunk_verify(file, size, digest) != CV_CHUNK_VALID
        || fseek(file, 0, SEEK_SET) != 0
        || cv_chunk_verify(file, size - 1, digest) != CV_CHUNK_MISMATCH
        || fseek(file, 0, SEEK_SET) != 0
        || cv_chunk_verify(file, size + 1, digest) != CV_CHUNK_MISMATCH) {
        fputs("Unexpected result verifying chunk size\n", stderr);
        return 1;
    }

    if (fseek(file, 0, SEEK_SET) != 0 || fputc(data[0] ^ 1, file) == EOF
        || fseek(file, 0, SEEK_SET) != 0
        || cv_chunk_verify(file, size, digest) != CV_CHUNK_MISMATCH) {
        fputs("Unexpected result verifying modified chunk\n", stderr);
        return 1;
    }
    return 0;
}

int main(void)
{
    const unsigned char bytes[] = {'a', 'b', 'c', 'a', 'b', 'c', 0, 255};
#ifdef _WIN32
    FILE *file = NULL;
    if (tmpfile_s(&file) != 0) {
        return 1;
    }
#else
    FILE *file = tmpfile();
    if (file == NULL) {
        return 1;
    }
#endif

    int result = 1;
    if (fwrite(bytes, 1, sizeof bytes, file) == sizeof bytes
        && fseek(file, 0, SEEK_SET) == 0) {
        result = check_chunks(file) || check_verification(file, bytes, sizeof bytes);
    }
    if (fclose(file) != 0) {
        return 1;
    }
    return result;
}
