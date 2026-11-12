#ifndef _WIN32
#define _XOPEN_SOURCE 700
#endif

#include "fs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

cv_path_char *cv_native(const char *path)
{
#ifdef _WIN32
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (count <= 0) {
        return NULL;
    }
    wchar_t *result = malloc((size_t)count * sizeof *result);
    if (result && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, result, count)) {
        free(result);
        return NULL;
    }
    return result;
#else
    return strdup(path);
#endif
}

#ifdef _WIN32
char *cv_utf8(const wchar_t *path)
{
    int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path, -1, NULL, 0, NULL, NULL);
    if (count <= 0) {
        return NULL;
    }
    char *result = malloc((size_t)count);
    if (result && !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path, -1, result, count, NULL, NULL)) {
        free(result);
        return NULL;
    }
    return result;
}
#endif

char *cv_join(const char *directory, const char *name)
{
    size_t a = strlen(directory);
    size_t b = strlen(name);
    if (b > (size_t)-1 - a || a + b > (size_t)-3) {
        return NULL;
    }
    char *path = malloc(a + b + 2);
    if (path) {
        memcpy(path, directory, a);
        path[a] = '/';
        memcpy(path + a + 1, name, b + 1);
    }
    return path;
}

FILE *cv_open(const char *path, int write)
{
#ifdef _WIN32
    cv_path_char *native = cv_native(path);
    FILE *file = NULL;
    if (native) {
        (void)_wfopen_s(&file, native, write ? L"wb" : L"rb");
        free(native);
    }
    return file;
#else
    return fopen(path, write ? "wb" : "rb");
#endif
}

static int source_stamp(FILE *file, struct cv_source_stamp *stamp)
{
#ifdef _WIN32
    intptr_t descriptor = _get_osfhandle(_fileno(file));
    BY_HANDLE_FILE_INFORMATION info;
    if (descriptor == -1 || !GetFileInformationByHandle((HANDLE)descriptor, &info)
        || (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
        return 0;
    }
    stamp->size = ((uint64_t)info.nFileSizeHigh << 32) | info.nFileSizeLow;
    stamp->changed[0] = ((uint64_t)info.ftLastWriteTime.dwHighDateTime << 32)
                        | info.ftLastWriteTime.dwLowDateTime;
    stamp->changed[1] = ((uint64_t)info.nFileIndexHigh << 32) | info.nFileIndexLow;
    stamp->changed[2] = info.dwVolumeSerialNumber;
    stamp->changed[3] = 0;
#else
    struct stat info;
    if (fstat(fileno(file), &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0) {
        return 0;
    }
    stamp->size = (uint64_t)info.st_size;
    stamp->changed[0] = (uint64_t)info.st_mtim.tv_sec;
    stamp->changed[1] = (uint64_t)info.st_mtim.tv_nsec;
    stamp->changed[2] = (uint64_t)info.st_ctim.tv_sec;
    stamp->changed[3] = (uint64_t)info.st_ctim.tv_nsec;
#endif
    return 1;
}

FILE *cv_open_native_source(const cv_path_char *path, struct cv_source_stamp *stamp,
                            int *missing)
{
    *missing = 0;
#ifdef _WIN32
    HANDLE handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        *missing = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
        return NULL;
    }
    int fd = _open_osfhandle((intptr_t)handle, _O_RDONLY | _O_BINARY | _O_NOINHERIT);
    if (fd < 0) {
        CloseHandle(handle);
        return NULL;
    }
    FILE *file = _fdopen(fd, "rb");
    if (!file) {
        _close(fd);
        return NULL;
    }
#else
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) {
        *missing = errno == ENOENT;
        return NULL;
    }
    FILE *file = fdopen(fd, "rb");
    if (!file) {
        close(fd);
        return NULL;
    }
#endif
    if (!source_stamp(file, stamp)) {
        fclose(file);
        return NULL;
    }
    return file;
}

FILE *cv_open_source(const char *path, struct cv_source_stamp *stamp)
{
    cv_path_char *native = cv_native(path);
    int missing;
    FILE *file = native ? cv_open_native_source(native, stamp, &missing) : NULL;
    free(native);
    return file;
}

int cv_source_unchanged(FILE *file, const struct cv_source_stamp *before)
{
    struct cv_source_stamp after;
    return source_stamp(file, &after) && before->size == after.size
           && memcmp(before->changed, after.changed, sizeof before->changed) == 0;
}

int cv_sync(FILE *file)
{
    if (fflush(file) != 0) {
        return 0;
    }
#ifdef _WIN32
    return _commit(_fileno(file)) == 0;
#else
    return fsync(fileno(file)) == 0;
#endif
}

int cv_sync_directory(const char *path)
{
#ifdef _WIN32
    (void)path;
    return 1;
#else
    int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) {
        return 0;
    }
    int ok = fsync(fd) == 0;
    return close(fd) == 0 && ok;
#endif
}

int cv_sync_parent(const char *path)
{
#ifdef _WIN32
    (void)path;
    return 1;
#else
    char *parent = strdup(path);
    if (!parent) {
        return 0;
    }
    size_t length = strlen(parent);
    while (length > 1 && parent[length - 1] == '/') {
        parent[--length] = '\0';
    }
    char *slash = strrchr(parent, '/');
    if (slash == parent) {
        slash[1] = '\0';
    } else if (slash) {
        *slash = '\0';
    }
    int ok = cv_sync_directory(slash ? parent : ".");
    free(parent);
    return ok;
#endif
}

int cv_mkdir(const char *path)
{
#ifdef _WIN32
    cv_path_char *native = cv_native(path);
    int result = native && CreateDirectoryW(native, NULL);
    free(native);
    return result;
#else
    return mkdir(path, 0700) == 0;
#endif
}

int cv_rmdir(const char *path)
{
#ifdef _WIN32
    cv_path_char *native = cv_native(path);
    int result = native && RemoveDirectoryW(native);
    free(native);
    return result;
#else
    return rmdir(path) == 0;
#endif
}

enum cv_file_kind cv_kind(const char *path)
{
#ifdef _WIN32
    cv_path_char *native = cv_native(path);
    if (!native) {
        return CV_FILE_ERROR;
    }
    DWORD attr = GetFileAttributesW(native);
    DWORD error = GetLastError();
    free(native);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
               ? CV_FILE_MISSING : CV_FILE_ERROR;
    }
    if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
        return CV_FILE_OTHER;
    }
    return attr & FILE_ATTRIBUTE_DIRECTORY ? CV_FILE_DIRECTORY : CV_FILE_REGULAR;
#else
    struct stat info;
    if (lstat(path, &info) != 0) {
        return errno == ENOENT || errno == ENOTDIR ? CV_FILE_MISSING : CV_FILE_ERROR;
    }
    if (S_ISDIR(info.st_mode)) {
        return CV_FILE_DIRECTORY;
    }
    return S_ISREG(info.st_mode) ? CV_FILE_REGULAR : CV_FILE_OTHER;
#endif
}

int cv_visit(const char *path, cv_visit_fn visit, void *context)
{
#ifdef _WIN32
    if (cv_kind(path) != CV_FILE_DIRECTORY) {
        return 0;
    }
    char *pattern = cv_join(path, "*");
    cv_path_char *native = pattern ? cv_native(pattern) : NULL;
    free(pattern);
    if (!native) {
        return 0;
    }
    WIN32_FIND_DATAW item;
    HANDLE search = FindFirstFileW(native, &item);
    DWORD error = GetLastError();
    free(native);
    if (search == INVALID_HANDLE_VALUE) {
        return error == ERROR_FILE_NOT_FOUND;
    }
    int ok = 1;
    do {
        char *name = cv_utf8(item.cFileName);
        ok = name && (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 || visit(name, context));
        free(name);
    } while (ok && FindNextFileW(search, &item));
    ok = ok && GetLastError() == ERROR_NO_MORE_FILES;
    FindClose(search);
    return ok;
#else
    int fd = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        return 0;
    }
    DIR *directory = fdopendir(fd);
    if (!directory) {
        close(fd);
        return 0;
    }
    int ok = 1;
    while (ok) {
        errno = 0;
        struct dirent *item = readdir(directory);
        if (!item) {
            ok = errno == 0;
            break;
        }
        if (strcmp(item->d_name, ".") != 0 && strcmp(item->d_name, "..") != 0) {
            ok = visit(item->d_name, context);
        }
    }
    return closedir(directory) == 0 && ok;
#endif
}

char *cv_absolute(const char *path)
{
#ifdef _WIN32
    cv_path_char *native = cv_native(path);
    if (!native) {
        return NULL;
    }
    HANDLE directory = CreateFileW(native, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    free(native);
    if (directory == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    DWORD count = GetFinalPathNameByHandleW(directory, NULL, 0, FILE_NAME_NORMALIZED);
    wchar_t *resolved = count ? malloc((size_t)count * sizeof *resolved) : NULL;
    DWORD length = resolved ? GetFinalPathNameByHandleW(directory, resolved, count, FILE_NAME_NORMALIZED) : 0;
    CloseHandle(directory);
    char *result = length && length < count ? cv_utf8(resolved) : NULL;
    free(resolved);
    return result;
#else
    return realpath(path, NULL);
#endif
}

unsigned long cv_process_id(void)
{
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

int cv_lock_file(const char *path, int exclusive, struct cv_lock *lock)
{
#ifdef _WIN32
    cv_path_char *native = cv_native(path);
    if (!native) {
        return 0;
    }
    HANDLE file = CreateFileW(native, GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                              FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    free(native);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    BY_HANDLE_FILE_INFORMATION info;
    OVERLAPPED region = {0};
    DWORD flags = LOCKFILE_FAIL_IMMEDIATELY | (exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0);
    if (!GetFileInformationByHandle(file, &info)
        || (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))
        || !LockFileEx(file, flags, 0, 1, 0, &region)) {
        CloseHandle(file);
        return 0;
    }
    lock->handle = file;
#else
    int fd = open(path, O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK, 0600);
    if (fd < 0) {
        return 0;
    }
    struct stat info;
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)
        || flock(fd, (exclusive ? LOCK_EX : LOCK_SH) | LOCK_NB) != 0) {
        close(fd);
        return 0;
    }
    lock->fd = fd;
#endif
    return 1;
}

void cv_unlock_file(struct cv_lock *lock)
{
#ifdef _WIN32
    OVERLAPPED region = {0};
    UnlockFileEx(lock->handle, 0, 1, 0, &region);
    CloseHandle(lock->handle);
#else
    close(lock->fd);
#endif
}
