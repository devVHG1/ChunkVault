#ifndef CV_SHA256_H
#define CV_SHA256_H

#include <stddef.h>
#include <stdint.h>

enum { CV_SHA256_SIZE = 32 };

struct cv_sha256 {
    uint32_t state[8];
    uint64_t bytes;
    unsigned char block[64];
    size_t used;
};

void cv_sha256_init(struct cv_sha256 *hash);
void cv_sha256_update(struct cv_sha256 *hash, const void *data, size_t size);
void cv_sha256_finish(struct cv_sha256 *hash, unsigned char digest[CV_SHA256_SIZE]);

#endif
