#ifndef CV_CDC_H
#define CV_CDC_H

#include "chunk.h"

enum { CV_CDC_MIN = 16384, CV_CDC_TARGET = 65536, CV_CDC_MAX = 262144 };

struct cv_cdc {
    unsigned char bytes[CV_CDC_MAX];
    size_t used;
    size_t consumed;
};

/* Reset used and consumed between files. The returned bytes are valid until
 * the next call. CV_CHUNK_OK always returns a nonempty chunk. */
enum cv_chunk_result cv_cdc_read(struct cv_cdc *reader, FILE *input,
                                 const unsigned char **bytes, size_t *size,
                                 unsigned char digest[CV_SHA256_SIZE]);

#endif
