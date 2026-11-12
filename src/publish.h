#ifndef CV_PUBLISH_H
#define CV_PUBLISH_H

#include <stddef.h>

#ifdef _WIN32
typedef wchar_t cv_path_char;
#else
typedef char cv_path_char;
#endif

enum cv_stage_result {
    CV_STAGE_OK,
    CV_STAGE_EXISTS,
    CV_STAGE_ERROR,
    CV_STAGE_TEMP_REMAINS
};

/* Creates an exclusive temporary file, writes and flushes the bytes, and
 * closes the file. TEMP_REMAINS means the write failed and cleanup is pending. */
enum cv_stage_result cv_stage(const cv_path_char *temporary,
                              const void *data, size_t size);

enum cv_publish_result {
    CV_PUBLISH_OK,
    CV_PUBLISH_TEMP_REMAINS,
    CV_PUBLISH_EXISTS,
    CV_PUBLISH_ERROR
};

/* Paths must be distinct siblings. The caller owns the temporary entry,
 * and closes and flushes regular files before publication.
 * On EXISTS, ERROR, or TEMP_REMAINS the caller must
 * clean up the temporary path. TEMP_REMAINS means publication succeeded.
 * Directories require native no-replace rename support. */
enum cv_publish_result cv_publish(const cv_path_char *temporary,
                                  const cv_path_char *destination);

/* Returns nonzero if the owned temporary path is absent after cleanup. */
int cv_remove_temp(const cv_path_char *temporary);

#endif
