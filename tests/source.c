#include "fs.h"

int main(void)
{
    const char *path = "source-stamp.test";
    FILE *writer = cv_open(path, 1);
    if (!writer) {
        return 1;
    }
    int wrote = fputs("abc", writer) != EOF;
    if (fclose(writer) != 0 || !wrote) {
        return 1;
    }

    struct cv_source_stamp stamp;
    FILE *reader = cv_open_source(path, &stamp);
    if (!reader) {
        return 1;
    }
    int ok = stamp.size == 3 && cv_source_unchanged(reader, &stamp);
    writer = cv_open(path, 1);
#ifdef _WIN32
    ok = ok && writer == NULL && cv_source_unchanged(reader, &stamp);
    if (writer) {
        fclose(writer);
    }
#else
    if (writer) {
        int closed = fclose(writer) == 0;
        ok = ok && closed && !cv_source_unchanged(reader, &stamp);
    } else {
        ok = 0;
    }
#endif
    return !(fclose(reader) == 0 && ok);
}
