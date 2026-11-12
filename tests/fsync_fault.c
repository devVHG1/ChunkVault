#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

int __real_fsync(int fd);

int __wrap_fsync(int fd)
{
    static unsigned long directory_calls;
    const char *fail_at = getenv("CV_TEST_FAIL_DIR_SYNC_AT");
    struct stat info;
    if (fail_at && fstat(fd, &info) == 0 && S_ISDIR(info.st_mode)
        && ++directory_calls == strtoul(fail_at, NULL, 10)) {
        errno = EIO;
        return -1;
    }
    return __real_fsync(fd);
}
