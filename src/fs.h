#ifndef CV_FS_H
#define CV_FS_H

#include "publish.h"

#include <stdint.h>
#include <stdio.h>

enum cv_file_kind {
    CV_FILE_ERROR = -1,
    CV_FILE_MISSING,
    CV_FILE_REGULAR,
    CV_FILE_DIRECTORY,
    CV_FILE_OTHER
};

typedef int (*cv_visit_fn)(const char *name, void *context);

cv_path_char *cv_native(const char *path);
#ifdef _WIN32
char *cv_utf8(const wchar_t *path);
#endif
char *cv_join(const char *directory, const char *name);
FILE *cv_open(const char *path, int write);
struct cv_source_stamp {
    uint64_t size;
    uint64_t changed[4];
};

FILE *cv_open_native_source(const cv_path_char *path, struct cv_source_stamp *stamp,
                            int *missing);
FILE *cv_open_source(const char *path, struct cv_source_stamp *stamp);
int cv_source_unchanged(FILE *file, const struct cv_source_stamp *before);
int cv_sync(FILE *file);
int cv_sync_directory(const char *path);
int cv_sync_parent(const char *path);
int cv_mkdir(const char *path);
int cv_rmdir(const char *path);
enum cv_file_kind cv_kind(const char *path);
int cv_visit(const char *path, cv_visit_fn visit, void *context);
char *cv_absolute(const char *path);
unsigned long cv_process_id(void);

struct cv_lock {
#ifdef _WIN32
    void *handle;
#else
    int fd;
#endif
};

int cv_lock_file(const char *path, int exclusive, struct cv_lock *lock);
void cv_unlock_file(struct cv_lock *lock);

#endif
