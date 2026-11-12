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
    return !(write_fixture("traversal.cvm", traversal, sizeof traversal - 1)
             && write_fixture("duplicate.cvm", duplicate, sizeof duplicate - 1)
             && write_fixture("truncated.cvm", truncated, sizeof truncated - 1));
}
