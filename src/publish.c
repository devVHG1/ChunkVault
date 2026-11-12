#ifndef _WIN32
#define _GNU_SOURCE
#endif

#include "publish.h"

#include <errno.h>
#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

int cv_remove_temp(const cv_path_char *temporary)
{
#ifdef _WIN32
    return DeleteFileW(temporary) || GetLastError() == ERROR_FILE_NOT_FOUND;
#else
    return unlink(temporary) == 0 || errno == ENOENT;
#endif
}

enum cv_stage_result cv_stage(const cv_path_char *temporary,
                              const void *data, size_t size)
{
#ifdef _WIN32
    FILE *file = NULL;
    int open_error = _wfopen_s(&file, temporary, L"wbx");
    if (open_error != 0) {
        return open_error == EEXIST ? CV_STAGE_EXISTS : CV_STAGE_ERROR;
    }
#else
    int fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0) {
        return errno == EEXIST ? CV_STAGE_EXISTS : CV_STAGE_ERROR;
    }
    FILE *file = fdopen(fd, "wb");
    if (file == NULL) {
        close(fd);
        return cv_remove_temp(temporary) ? CV_STAGE_ERROR : CV_STAGE_TEMP_REMAINS;
    }
#endif

    int written = (size == 0 || fwrite(data, 1, size, file) == size)
                  && fflush(file) == 0;
#ifdef _WIN32
    int flushed = written && _commit(_fileno(file)) == 0;
#else
    int flushed = written && fsync(fileno(file)) == 0;
#endif
    int closed = fclose(file) == 0;
    if (flushed && closed) {
        return CV_STAGE_OK;
    }

    return cv_remove_temp(temporary) ? CV_STAGE_ERROR : CV_STAGE_TEMP_REMAINS;
}

enum cv_publish_result cv_publish(const cv_path_char *temporary,
                                  const cv_path_char *destination)
{
#ifdef _WIN32
    if (MoveFileExW(temporary, destination, 0)) {
        return CV_PUBLISH_OK;
    }
    DWORD error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS) {
        return CV_PUBLISH_EXISTS;
    }
#else
    if (renameat2(AT_FDCWD, temporary, AT_FDCWD, destination, RENAME_NOREPLACE) == 0) {
        return CV_PUBLISH_OK;
    }
    int error = errno;
    if (error == EEXIST) {
        return CV_PUBLISH_EXISTS;
    }
    if (error == ENOSYS || error == EINVAL || error == EOPNOTSUPP) {
        if (link(temporary, destination) == 0) {
            return unlink(temporary) == 0 ? CV_PUBLISH_OK : CV_PUBLISH_TEMP_REMAINS;
        }
        if (errno == EEXIST) {
            return CV_PUBLISH_EXISTS;
        }
    }
#endif
    return CV_PUBLISH_ERROR;
}
