#include "chunk.h"

#include <string.h>

#ifdef CV_TEST_LINK_FALLBACK
#include <errno.h>

static int fail_cleanup;
static int fail_flush;

int __wrap_renameat2(int source_dir, const char *source, int destination_dir,
                     const char *destination, unsigned int flags)
{
    (void)source_dir;
    (void)source;
    (void)destination_dir;
    (void)destination;
    (void)flags;
    errno = EOPNOTSUPP;
    return -1;
}

int __real_fsync(int fd);

int __wrap_fsync(int fd)
{
    if (fail_flush) {
        errno = EIO;
        return -1;
    }
    return __real_fsync(fd);
}

int __real_unlink(const char *path);

int __wrap_unlink(const char *path)
{
    if (fail_cleanup) {
        errno = EACCES;
        return -1;
    }
    return __real_unlink(path);
}
#endif

#ifdef _WIN32
#define PATH_LITERAL(value) L##value
#else
#define PATH_LITERAL(value) value
#endif

int main(int argc, char *argv[])
{
    if (argc != 2) {
        return 3;
    }

#ifdef CV_TEST_LINK_FALLBACK
    fail_cleanup = strcmp(argv[1], "4") == 0 || strcmp(argv[1], "8") == 0;
    fail_flush = strcmp(argv[1], "7") == 0 || strcmp(argv[1], "8") == 0;
#endif

    if (strcmp(argv[1], "store") == 0 || strcmp(argv[1], "store-other") == 0) {
        static const unsigned char bytes[] = {0, 10, 255};
        const cv_path_char *temporary = strcmp(argv[1], "store") == 0
                                        ? PATH_LITERAL("object.tmp")
                                        : PATH_LITERAL("object-other.tmp");
        int cleanup_pending;
        enum cv_chunk_store_result stored = cv_chunk_store(
            temporary, PATH_LITERAL("object"),
            bytes, sizeof bytes, &cleanup_pending);
        return cleanup_pending ? 5 : (int)stored;
    }

    if (argv[1][0] == '5' || argv[1][0] == '7' || argv[1][0] == '8') {
        static const unsigned char bytes[] = {0, 10, 255};
        return cv_stage(PATH_LITERAL("stage.tmp"), bytes, sizeof bytes);
    }

    const cv_path_char *source = PATH_LITERAL("first.tmp");
    if (argv[1][0] == '2') {
        source = PATH_LITERAL("second.tmp");
    } else if (argv[1][0] == '3') {
        source = PATH_LITERAL("missing.tmp");
    } else if (argv[1][0] == '6') {
        source = PATH_LITERAL("stage.tmp");
    }
    switch (cv_publish(source, PATH_LITERAL("final"))) {
    case CV_PUBLISH_OK:
        return 0;
    case CV_PUBLISH_TEMP_REMAINS:
        return 4;
    case CV_PUBLISH_EXISTS:
        return 1;
    case CV_PUBLISH_ERROR:
        return 2;
    }
    return 3;
}
