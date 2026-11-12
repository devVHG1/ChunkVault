#include "cdc.h"

#include <stdlib.h>
#include <string.h>

enum { SAMPLE_SIZE = 2 * 1024 * 1024, MAX_CHUNKS = SAMPLE_SIZE / CV_CDC_MIN + 2 };

struct sample_chunk {
    size_t size;
    unsigned char digest[CV_SHA256_SIZE];
};

static int collect(const unsigned char *input, size_t size,
                   struct sample_chunk chunks[MAX_CHUNKS], size_t *count)
{
#ifdef _WIN32
    FILE *file = NULL;
    if (tmpfile_s(&file) != 0) {
        return 0;
    }
#else
    FILE *file = tmpfile();
    if (!file) {
        return 0;
    }
#endif

    struct cv_cdc *reader = calloc(1, sizeof *reader);
    int ok = reader && fwrite(input, 1, size, file) == size && fseek(file, 0, SEEK_SET) == 0;
    size_t offset = 0;
    *count = 0;
    while (ok && *count < MAX_CHUNKS) {
        const unsigned char *bytes;
        struct sample_chunk *chunk = &chunks[*count];
        enum cv_chunk_result result = cv_cdc_read(reader, file, &bytes, &chunk->size,
                                                  chunk->digest);
        if (result == CV_CHUNK_END) {
            break;
        }
        ok = result == CV_CHUNK_OK && chunk->size > 0 && chunk->size <= CV_CDC_MAX
             && chunk->size <= size - offset
             && (chunk->size >= CV_CDC_MIN || chunk->size == size - offset)
             && memcmp(bytes, input + offset, chunk->size) == 0;
        if (ok) {
            offset += chunk->size;
            ++*count;
        }
    }
    free(reader);
    return fclose(file) == 0 && ok && offset == size && *count < MAX_CHUNKS;
}

int main(void)
{
    unsigned char *data = malloc(SAMPLE_SIZE + 1);
    if (!data) {
        return 1;
    }
    uint32_t random = 123456789;
    data[0] = 0xff;
    for (size_t i = 1; i <= SAMPLE_SIZE; ++i) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        data[i] = (unsigned char)random;
    }

    struct sample_chunk original[MAX_CHUNKS], shifted[MAX_CHUNKS];
    size_t first_count, second_count;
    int ok = collect(data + 1, SAMPLE_SIZE, original, &first_count)
             && collect(data, SAMPLE_SIZE + 1, shifted, &second_count);
    free(data);
    if (!ok) {
        fputs("Chunking did not preserve the byte stream or size bounds\n", stderr);
        return 1;
    }

    size_t reused = 0;
    for (size_t i = 0; i < second_count; ++i) {
        for (size_t j = 0; j < first_count; ++j) {
            if (shifted[i].size == original[j].size
                && memcmp(shifted[i].digest, original[j].digest, CV_SHA256_SIZE) == 0) {
                reused += shifted[i].size;
                break;
            }
        }
    }
    if (reused < SAMPLE_SIZE * 85 / 100) {
        fprintf(stderr, "Insertion retained only %zu of %d bytes in existing chunks\n",
                reused, SAMPLE_SIZE);
        return 1;
    }
    return 0;
}
