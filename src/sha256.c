#include "sha256.h"

#include <string.h>

static const uint32_t round_constants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static uint32_t rotate_right(uint32_t word, unsigned int bits)
{
    return (word >> bits) | (word << (32 - bits));
}

static void compress(struct cv_sha256 *hash, const unsigned char block[64])
{
    uint32_t words[64];
    for (size_t i = 0; i < 16; ++i) {
        const unsigned char *p = block + i * 4;
        words[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
                   | ((uint32_t)p[2] << 8) | p[3];
    }
    for (size_t i = 16; i < 64; ++i) {
        uint32_t x = words[i - 15];
        uint32_t y = words[i - 2];
        uint32_t s0 = rotate_right(x, 7) ^ rotate_right(x, 18) ^ (x >> 3);
        uint32_t s1 = rotate_right(y, 17) ^ rotate_right(y, 19) ^ (y >> 10);
        words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }

    uint32_t a = hash->state[0], b = hash->state[1], c = hash->state[2], d = hash->state[3];
    uint32_t e = hash->state[4], f = hash->state[5], g = hash->state[6], h = hash->state[7];
    for (size_t i = 0; i < 64; ++i) {
        uint32_t s1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        uint32_t choose = (e & f) ^ (~e & g);
        uint32_t t1 = h + s1 + choose + round_constants[i] + words[i];
        uint32_t s0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + s0 + majority;
    }
    const uint32_t result[] = {a, b, c, d, e, f, g, h};
    for (size_t i = 0; i < 8; ++i) {
        hash->state[i] += result[i];
    }
}

void cv_sha256_init(struct cv_sha256 *hash)
{
    *hash = (struct cv_sha256){
        .state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                  0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19}
    };
}

void cv_sha256_update(struct cv_sha256 *hash, const void *data, size_t size)
{
    const unsigned char *input = data;
    hash->bytes += size;
    while (size > 0) {
        size_t space = sizeof hash->block - hash->used;
        size_t count = size < space ? size : space;
        memcpy(hash->block + hash->used, input, count);
        input += count;
        size -= count;
        hash->used += count;
        if (hash->used == sizeof hash->block) {
            compress(hash, hash->block);
            hash->used = 0;
        }
    }
}

void cv_sha256_finish(struct cv_sha256 *hash, unsigned char digest[CV_SHA256_SIZE])
{
    uint64_t bits = hash->bytes * 8;
    hash->block[hash->used++] = 0x80;
    if (hash->used > 56) {
        memset(hash->block + hash->used, 0, sizeof hash->block - hash->used);
        compress(hash, hash->block);
        hash->used = 0;
    }
    memset(hash->block + hash->used, 0, 56 - hash->used);
    for (size_t i = 0; i < 8; ++i) {
        hash->block[63 - i] = (unsigned char)(bits >> (8 * i));
    }
    compress(hash, hash->block);

    for (size_t i = 0; i < CV_SHA256_SIZE; ++i) {
        digest[i] = (unsigned char)(hash->state[i / 4] >> (24 - 8 * (i % 4)));
    }
}
