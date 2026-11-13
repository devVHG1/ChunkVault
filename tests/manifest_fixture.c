#include <stdio.h>

static int write_fixture(const char *name, const unsigned char *bytes, size_t size)
{
    FILE *file = NULL;
#ifdef _WIN32
    (void)fopen_s(&file, name, "wb");
#else
    file = fopen(name, "wb");
#endif
    if (!file) {
        return 0;
    }
    int ok = fwrite(bytes, 1, size, file) == size;
    return fclose(file) == 0 && ok;
}

int main(void)
{
    static const unsigned char traversal[] = "CVDRAFT2D\x07\0\0\0../evilE";
    static const unsigned char duplicate[] = "CVDRAFT2D\x01\0\0\0xD\x01\0\0\0xE";
    static const unsigned char truncated[] = "CVDRAFT2F\x01\0\0\0x\x01\0";
    static const unsigned char valid[] = {
        0x43, 0x56, 0x44, 0x52, 0x41, 0x46, 0x54, 0x32, 0x46, 0x01, 0x00, 0x00,
        0x00, 0x78, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
        0x00, 0x00, 0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41,
        0x40, 0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
        0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad, 0x45
    };

    for (size_t length = 0; length < sizeof valid; ++length) {
        char name[32];
        snprintf(name, sizeof name, "mut-cut-%zu.cvm", length);
        if (!write_fixture(name, valid, length)) {
            return 1;
        }
    }
    static const struct {
        const char *name;
        const unsigned char *bytes;
        size_t size;
    } mutations[] = {
#define MUTATION(name, bytes) {"mut-" name ".cvm", (const unsigned char *)(bytes), sizeof(bytes) - 1}
        MUTATION("huge-path", "CVDRAFT2D\xff\xff\xff\xff"),
        MUTATION("absolute", "CVDRAFT2D\x02\0\0\0/xE"),
        MUTATION("drive", "CVDRAFT2D\x04\0\0\0" "C:\\xE"),
        MUTATION("nested-traversal", "CVDRAFT2D\x09\0\0\0" "a/../../bE"),
        MUTATION("nul", "CVDRAFT2D\x03\0\0\0x\0yE"),
        MUTATION("zero-chunk", "CVDRAFT2F\x01\0\0\0x\x01\0\0\0\0\0\0\0\0\0\0\0"),
        MUTATION("huge-chunk", "CVDRAFT2F\x01\0\0\0x\x01\0\0\0\0\0\0\0\xff\xff\xff\x7f")
#undef MUTATION
    };
    for (size_t i = 0; i < sizeof mutations / sizeof mutations[0]; ++i) {
        if (!write_fixture(mutations[i].name, mutations[i].bytes, mutations[i].size)) {
            return 1;
        }
    }
    return !(write_fixture("traversal.cvm", traversal, sizeof traversal - 1)
             && write_fixture("duplicate.cvm", duplicate, sizeof duplicate - 1)
             && write_fixture("truncated.cvm", truncated, sizeof truncated - 1));
}
