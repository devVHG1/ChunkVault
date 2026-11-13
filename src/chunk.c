#include "chunk.h"

#include "fs.h"
#include <string.h>

enum cv_chunk_result cv_chunk_read(FILE *input, unsigned char *buffer,
                                   size_t capacity, size_t *size,
                                   unsigned char digest[CV_SHA256_SIZE])
{
    *size = 0;
    if (capacity == 0 || ferror(input)) {
        return CV_CHUNK_ERROR;
    }
    size_t count = fread(buffer, 1, capacity, input);
    if (ferror(input)) {
        return CV_CHUNK_ERROR;
    }
    if (count == 0) {
        return CV_CHUNK_END;
    }

    struct cv_sha256 hash;
    cv_sha256_init(&hash);
    cv_sha256_update(&hash, buffer, count);
    cv_sha256_finish(&hash, digest);
    *size = count;
    return CV_CHUNK_OK;
}

enum cv_chunk_verify_result cv_chunk_verify(FILE *input, uint64_t expected_size,
                                            const unsigned char expected_digest[CV_SHA256_SIZE])
{
    unsigned char buffer[8192];
    struct cv_sha256 hash;
    cv_sha256_init(&hash);
    while (expected_size > 0) {
        size_t wanted = expected_size < sizeof buffer ? (size_t)expected_size : sizeof buffer;
        size_t count = fread(buffer, 1, wanted, input);
        if (count != wanted) {
            return ferror(input) ? CV_CHUNK_READ_ERROR : CV_CHUNK_MISMATCH;
        }
        cv_sha256_update(&hash, buffer, count);
        expected_size -= count;
    }

    int trailing = fgetc(input);
    if (ferror(input)) {
        return CV_CHUNK_READ_ERROR;
    }
    if (trailing != EOF) {
        return CV_CHUNK_MISMATCH;
    }

    unsigned char actual[CV_SHA256_SIZE];
    cv_sha256_finish(&hash, actual);
    return memcmp(actual, expected_digest, sizeof actual) == 0
           ? CV_CHUNK_VALID : CV_CHUNK_MISMATCH;
}

static enum cv_chunk_store_result check_existing(const cv_path_char *destination,
                                                  const void *data, size_t size,
                                                  int *missing)
{
    struct cv_source_stamp stamp;
    FILE *file = cv_open_native_source(destination, &stamp, missing);
    if (!file) {
        return CV_CHUNK_STORE_ERROR;
    }

    struct cv_sha256 hash;
    unsigned char digest[CV_SHA256_SIZE];
    cv_sha256_init(&hash);
    cv_sha256_update(&hash, data, size);
    cv_sha256_finish(&hash, digest);

    enum cv_chunk_verify_result verified = cv_chunk_verify(file, size, digest);
    /* Removing the staging hard link can change ctime after publication.
     * Reuse depends on the complete byte count and digest. */
    if (fclose(file) != 0 || verified == CV_CHUNK_READ_ERROR) {
        return CV_CHUNK_STORE_ERROR;
    }
    return verified == CV_CHUNK_VALID ? CV_CHUNK_REUSED : CV_CHUNK_CONFLICT;
}

enum cv_chunk_store_result cv_chunk_store(const cv_path_char *temporary,
                                           const cv_path_char *destination,
                                           const void *data, size_t size,
                                           int *cleanup_pending)
{
    *cleanup_pending = 0;
    int missing;
    enum cv_chunk_store_result existing = check_existing(destination, data, size, &missing);
    if (!missing) {
        return existing;
    }

    enum cv_stage_result staged = cv_stage(temporary, data, size);
    if (staged != CV_STAGE_OK) {
        *cleanup_pending = staged == CV_STAGE_TEMP_REMAINS;
        return CV_CHUNK_STORE_ERROR;
    }

    enum cv_publish_result published = cv_publish(temporary, destination);
    if (published == CV_PUBLISH_OK) {
        return CV_CHUNK_STORED;
    }
    *cleanup_pending = !cv_remove_temp(temporary);
    if (published == CV_PUBLISH_TEMP_REMAINS) {
        return CV_CHUNK_STORED;
    }
    return check_existing(destination, data, size, &missing);
}
