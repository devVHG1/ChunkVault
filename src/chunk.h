#ifndef CV_CHUNK_H
#define CV_CHUNK_H

#include "publish.h"
#include "sha256.h"

#include <stdio.h>

enum cv_chunk_result {
    CV_CHUNK_OK,
    CV_CHUNK_END,
    CV_CHUNK_ERROR
};

/* Reads up to capacity bytes using fixed-size boundaries. A successful
 * read returns a nonempty chunk and its digest. The caller owns the stream. */
enum cv_chunk_result cv_chunk_read(FILE *input, unsigned char *buffer,
                                   size_t capacity, size_t *size,
                                   unsigned char digest[CV_SHA256_SIZE]);

enum cv_chunk_verify_result {
    CV_CHUNK_VALID,
    CV_CHUNK_MISMATCH,
    CV_CHUNK_READ_ERROR
};

/* Verifies the bytes from the current stream position through EOF. */
enum cv_chunk_verify_result cv_chunk_verify(FILE *input, uint64_t expected_size,
                                            const unsigned char expected_digest[CV_SHA256_SIZE]);

enum cv_chunk_store_result {
    CV_CHUNK_STORED,
    CV_CHUNK_REUSED,
    CV_CHUNK_CONFLICT,
    CV_CHUNK_STORE_ERROR
};

/* The caller supplies the object path derived from the digest and a distinct
 * temporary path in the same directory. A pending cleanup is reported
 * independently of the object result. */
enum cv_chunk_store_result cv_chunk_store(const cv_path_char *temporary,
                                           const cv_path_char *destination,
                                           const void *data, size_t size,
                                           int *cleanup_pending);

#endif
