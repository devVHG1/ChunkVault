#ifndef CV_FAILPOINT_H
#define CV_FAILPOINT_H

#ifdef CV_TEST_FAILPOINTS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void cv_crash_at(const char *point)
{
    const char *selected = getenv("CV_TEST_FAILPOINT");
    if (selected && strcmp(selected, point) == 0) {
        fprintf(stderr, "injected interruption: %s\n", point);
        _Exit(86);
    }
}
#else
#define cv_crash_at(point) ((void)0)
#endif

#endif
