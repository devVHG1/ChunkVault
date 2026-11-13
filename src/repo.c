#include "failpoint.h"
#include "repo.h"

#include "cdc.h"
#include "fs.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { CHUNK_SIZE = 65536, PATH_LIMIT = 4096, DEPTH_LIMIT = 128 };
static const char marker_v1[] = "ChunkVault draft-1\n";
static const char marker_v2[] = "ChunkVault draft-2\n";
static const char magic_v1[] = "CVDRAFT1";
static const char magic_v2[] = "CVDRAFT2";

static const char *manifest_magic(int version)
{
    return version == 1 ? magic_v1 : magic_v2;
}

static int close_file(FILE *file, int ok)
{
    return fclose(file) == 0 && ok;
}

static int put_number(FILE *file, uint64_t value, size_t width)
{
    while (width-- > 0) {
        if (fputc((unsigned char)value, file) == EOF) {
            return 0;
        }
        value >>= 8;
    }
    return 1;
}

static int get_number(FILE *file, uint64_t *value, size_t width)
{
    *value = 0;
    for (size_t i = 0; i < width; ++i) {
        int byte = fgetc(file);
        if (byte == EOF) {
            return 0;
        }
        *value |= (uint64_t)byte << (i * 8);
    }
    return 1;
}

static int alnum_ascii(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static size_t utf8_width(const unsigned char *bytes, size_t available)
{
    unsigned char first = bytes[0];
    size_t width;
    if (first >= 0xc2 && first <= 0xdf) {
        width = 2;
    } else if (first >= 0xe0 && first <= 0xef) {
        width = 3;
    } else if (first >= 0xf0 && first <= 0xf4) {
        width = 4;
    } else {
        return 0;
    }
    if (available < width) {
        return 0;
    }
    for (size_t i = 1; i < width; ++i) {
        if (bytes[i] < 0x80 || bytes[i] > 0xbf) {
            return 0;
        }
    }
    if ((first == 0xe0 && bytes[1] < 0xa0) || (first == 0xed && bytes[1] >= 0xa0)
        || (first == 0xf0 && bytes[1] < 0x90) || (first == 0xf4 && bytes[1] >= 0x90)) {
        return 0;
    }
    return width;
}

static int valid_component(const char *part, size_t length)
{
    if (length == 0 || length > 255 || part[length - 1] == '.' || part[length - 1] == ' ') {
        return 0;
    }
    const char *dot = memchr(part, '.', length);
    if (dot && dot != part && dot[-1] == ' ') {
        return 0;
    }
    char stem[8] = {0};
    size_t stem_length = dot ? (size_t)(dot - part) : length;
    if (stem_length < sizeof stem) {
        for (size_t i = 0; i < stem_length; ++i) {
            unsigned char c = (unsigned char)part[i];
            stem[i] = c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : (char)c;
        }
        if (strcmp(stem, "CON") == 0 || strcmp(stem, "PRN") == 0
            || strcmp(stem, "AUX") == 0 || strcmp(stem, "NUL") == 0
            || strcmp(stem, "CONIN$") == 0 || strcmp(stem, "CONOUT$") == 0) {
            return 0;
        }
        if (strncmp(stem, "COM", 3) == 0 || strncmp(stem, "LPT", 3) == 0) {
            unsigned char digit = (unsigned char)stem[4];
            if ((stem_length == 4 && stem[3] >= '0' && stem[3] <= '9')
                || (stem_length == 5 && (unsigned char)stem[3] == 0xc2
                    && (digit == 0xb9 || digit == 0xb2 || digit == 0xb3))) {
                return 0;
            }
        }
    }
    for (size_t i = 0; i < length;) {
        unsigned char c = (unsigned char)part[i];
        if (c < 0x80) {
            if (c < 0x20 || c == 0x7f || strchr("<>:\"/\\|?*", c)) {
                return 0;
            }
            ++i;
        } else {
            size_t width = utf8_width((const unsigned char *)part + i, length - i);
            if (width == 0) {
                return 0;
            }
            i += width;
        }
    }
    return 1;
}

static int valid_relative(const char *path)
{
    if (strlen(path) >= PATH_LIMIT) {
        return 0;
    }
    for (unsigned depth = 0; depth < DEPTH_LIMIT; ++depth) {
        const char *slash = strchr(path, '/');
        if (!valid_component(path, slash ? (size_t)(slash - path) : strlen(path))) {
            return 0;
        }
        if (!slash) {
            return 1;
        }
        path = slash + 1;
    }
    return 0;
}

static int valid_snapshot(const char *id)
{
    size_t length = strlen(id);
    if (length == 0 || length > 64) {
        return 0;
    }
    for (size_t i = 0; i < length; ++i) {
        if (!alnum_ascii((unsigned char)id[i]) && id[i] != '-' && id[i] != '_') {
            return 0;
        }
    }
    return valid_component(id, length);
}

static char *snapshot_path(const char *directory, const char *id)
{
    char name[70];
    snprintf(name, sizeof name, "%s.cvm", id);
    return cv_join(directory, name);
}

static int repository_version(const char *root)
{
    char *path = cv_join(root, "format");
    char *objects = cv_join(root, "objects");
    char *snapshots = cv_join(root, "snapshots");
    FILE *file = path && cv_kind(path) == CV_FILE_REGULAR ? cv_open(path, 0) : NULL;
    char bytes[sizeof marker_v1];
    int version = 0;
    if (file && fread(bytes, 1, sizeof bytes, file) == sizeof marker_v1 - 1
        && !ferror(file) && objects && cv_kind(objects) == CV_FILE_DIRECTORY
        && snapshots && cv_kind(snapshots) == CV_FILE_DIRECTORY) {
        if (memcmp(bytes, marker_v1, sizeof marker_v1 - 1) == 0) {
            version = 1;
        } else if (memcmp(bytes, marker_v2, sizeof marker_v2 - 1) == 0) {
            version = 2;
        }
    }
    if (file && fclose(file) != 0) {
        version = 0;
    }
    free(snapshots);
    free(objects);
    free(path);
    return version;
}

int cv_repo_init(const char *root)
{
    char *objects = cv_join(root, "objects");
    char *snapshots = cv_join(root, "snapshots");
    char *pending = cv_join(root, ".format.tmp");
    char *format = cv_join(root, "format");
    cv_path_char *temp_native = pending ? cv_native(pending) : NULL;
    cv_path_char *format_native = format ? cv_native(format) : NULL;

    int made_root = objects && snapshots && temp_native && format_native && cv_mkdir(root);
    int made_objects = made_root && cv_mkdir(objects);
    int made_snapshots = made_objects && cv_mkdir(snapshots);
    enum cv_stage_result staged = made_snapshots
        ? cv_stage(temp_native, marker_v2, sizeof marker_v2 - 1) : CV_STAGE_ERROR;
    enum cv_publish_result published = staged == CV_STAGE_OK && cv_sync_directory(objects)
        && cv_sync_directory(snapshots) && cv_sync_directory(root)
        ? cv_publish(temp_native, format_native) : CV_PUBLISH_ERROR;
    int ok = published == CV_PUBLISH_OK || published == CV_PUBLISH_TEMP_REMAINS;
    if (staged == CV_STAGE_TEMP_REMAINS
        || (staged == CV_STAGE_OK && published != CV_PUBLISH_OK)) {
        if (!cv_remove_temp(temp_native)) {
            fputs("cv: format temporary file requires cleanup\n", stderr);
        }
    }
    if (ok && (!cv_sync_directory(root) || !cv_sync_parent(root))) {
        fputs("cv: repository is visible but directory synchronization failed\n", stderr);
        ok = 0;
    } else if (!ok) {
        if (made_snapshots) {
            cv_rmdir(snapshots);
        }
        if (made_objects) {
            cv_rmdir(objects);
        }
        if (made_root) {
            cv_rmdir(root);
        }
        fputs("cv: cannot initialize repository (destination must be new)\n", stderr);
    }

    free(format_native);
    free(temp_native);
    free(format);
    free(pending);
    free(snapshots);
    free(objects);
    return ok;
}

static char *object_path(const char *directory, const unsigned char digest[CV_SHA256_SIZE])
{
    const char alphabet[] = "0123456789abcdef";
    char name[CV_SHA256_SIZE * 2 + 1];
    for (size_t i = 0; i < CV_SHA256_SIZE; ++i) {
        name[2 * i] = alphabet[digest[i] >> 4];
        name[2 * i + 1] = alphabet[digest[i] & 15];
    }
    name[sizeof name - 1] = '\0';
    return cv_join(directory, name);
}

struct backup {
    FILE *manifest;
    const char *objects;
    const cv_path_char *object_temp;
    int version;
    struct cv_cdc reader;
};

static int backup_file(struct backup *backup, const char *source)
{
    struct cv_source_stamp before;
    FILE *input = cv_open_source(source, &before);
    if (!input) {
        return 0;
    }
    backup->reader.used = 0;
    backup->reader.consumed = 0;
    fpos_t size_position;
    uint64_t total = 0;
    int ok = fgetpos(backup->manifest, &size_position) == 0 && put_number(backup->manifest, 0, 8);
    while (ok) {
        size_t size;
        unsigned char digest[CV_SHA256_SIZE];
        const unsigned char *bytes = backup->reader.bytes;
        enum cv_chunk_result read = backup->version == 1
            ? cv_chunk_read(input, backup->reader.bytes, CHUNK_SIZE, &size, digest)
            : cv_cdc_read(&backup->reader, input, &bytes, &size, digest);
        if (read == CV_CHUNK_END) {
            break;
        }
        if (read != CV_CHUNK_OK || size > before.size - total) {
            ok = 0;
            break;
        }
        char *path = object_path(backup->objects, digest);
        cv_path_char *native = path ? cv_native(path) : NULL;
        int cleanup = 0;
        enum cv_chunk_store_result stored = native
            ? cv_chunk_store(backup->object_temp, native, bytes, size, &cleanup)
            : CV_CHUNK_STORE_ERROR;
        free(native);
        free(path);
        ok = !cleanup && (stored == CV_CHUNK_STORED || stored == CV_CHUNK_REUSED)
             && (backup->version == 1 || put_number(backup->manifest, size, 4))
             && fwrite(digest, 1, sizeof digest, backup->manifest) == sizeof digest;
        total += size;
    }
    fpos_t end;
    if (ok && (total != before.size || !cv_source_unchanged(input, &before))) {
        fprintf(stderr, "cv: source changed during backup: %s\n", source);
        ok = 0;
    }
    ok = ok && fgetpos(backup->manifest, &end) == 0
         && fsetpos(backup->manifest, &size_position) == 0
         && put_number(backup->manifest, total, 8) && fsetpos(backup->manifest, &end) == 0;
    return close_file(input, ok);
}

struct walk {
    struct backup *backup;
    const char *source;
    const char *relative;
};

static int backup_entry(const char *name, void *context)
{
    const struct walk *parent = context;
    char *path = cv_join(parent->source, name);
    char *relative = *parent->relative ? cv_join(parent->relative, name) : cv_join("", name);
    const char *record = relative && *parent->relative == '\0' ? relative + 1 : relative;
    int ok = path && record && valid_relative(record);
    if (ok) {
        enum cv_file_kind kind = cv_kind(path);
        size_t length = strlen(record);
        FILE *file = parent->backup->manifest;
        ok = (kind == CV_FILE_DIRECTORY || kind == CV_FILE_REGULAR)
             && fputc(kind == CV_FILE_DIRECTORY ? 'D' : 'F', file) != EOF
             && put_number(file, length, 4) && fwrite(record, 1, length, file) == length;
        if (ok && kind == CV_FILE_DIRECTORY) {
            struct walk child = {parent->backup, path, record};
            ok = cv_visit(path, backup_entry, &child);
        } else if (ok) {
            ok = backup_file(parent->backup, path);
        }
    }
    if (!ok) {
        fprintf(stderr, "cv: cannot back up %s\n", path ? path : name);
    }
    free(relative);
    free(path);
    return ok;
}

static int root_inside_source(const char *root, const char *source)
{
    char *a = cv_absolute(root);
    char *b = cv_absolute(source);
    int inside = 1;
    if (a && b) {
        size_t length = strlen(b);
        inside = 1;
        for (size_t i = 0; i < length; ++i) {
#ifdef _WIN32
            if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) {
#else
            if (a[i] != b[i]) {
#endif
                inside = 0;
                break;
            }
        }
        if (inside && length > 0) {
            char last = b[length - 1];
            inside = a[length] == '\0' || a[length] == '/' || a[length] == '\\'
                     || last == '/' || last == '\\';
        }
    }
    free(b);
    free(a);
    return inside;
}

static int repository_lock(const char *root, int exclusive, struct cv_lock *lock)
{
    char *path = cv_join(root, ".lock");
    int ok = path && cv_lock_file(path, exclusive, lock);
    free(path);
    if (!ok) {
        fputs("cv: repository busy or lock unavailable\n", stderr);
    }
    return ok;
}

int cv_repo_backup(const char *root, const char *source, const char *snapshot)
{
    char generated[64];
    if (!snapshot) {
        snprintf(generated, sizeof generated, "%lld-%lu", (long long)time(NULL), cv_process_id());
        snapshot = generated;
    }
    int version = repository_version(root);
    if (!valid_snapshot(snapshot) || !version || cv_kind(source) != CV_FILE_DIRECTORY
        || root_inside_source(root, source) || root_inside_source(source, root)) {
        fputs("cv: invalid snapshot ID, repository, or source directory\n", stderr);
        return 0;
    }

    struct cv_lock lock;
    if (!repository_lock(root, 0, &lock)) {
        return 0;
    }
    char temp_name[64];
    snprintf(temp_name, sizeof temp_name, ".%lld-%lu.tmp", (long long)time(NULL), cv_process_id());
    char *objects = cv_join(root, "objects");
    char *snapshots = cv_join(root, "snapshots");
    char *object_temp = objects ? cv_join(objects, temp_name) : NULL;
    char *temp = snapshots ? cv_join(snapshots, temp_name) : NULL;
    char *destination = snapshots ? snapshot_path(snapshots, snapshot) : NULL;
    cv_path_char *object_native = object_temp ? cv_native(object_temp) : NULL;
    cv_path_char *temp_native = temp ? cv_native(temp) : NULL;
    cv_path_char *destination_native = destination ? cv_native(destination) : NULL;
    enum cv_stage_result staged = object_native && temp_native && destination_native
                                  && cv_kind(destination) == CV_FILE_MISSING
        ? cv_stage(temp_native, NULL, 0) : CV_STAGE_ERROR;
    FILE *file = staged == CV_STAGE_OK ? cv_open(temp, 1) : NULL;

    int ok = 0;
    if (file) {
        struct backup *backup = calloc(1, sizeof *backup);
        if (backup) {
            backup->manifest = file;
            backup->objects = objects;
            backup->object_temp = object_native;
            backup->version = version;
            struct walk walk = {backup, source, ""};
            ok = fwrite(manifest_magic(version), 1, sizeof magic_v1 - 1, file) == sizeof magic_v1 - 1
                 && cv_visit(source, backup_entry, &walk) && fputc('E', file) != EOF && cv_sync(file);
            free(backup);
        }
        ok = close_file(file, ok);
    }

    if (ok) {
        cv_crash_at("manifest-ready");
    }
    enum cv_publish_result published = ok && cv_sync_directory(objects)
        ? cv_publish(temp_native, destination_native) : CV_PUBLISH_ERROR;
    ok = published == CV_PUBLISH_OK || published == CV_PUBLISH_TEMP_REMAINS;
    if (staged == CV_STAGE_TEMP_REMAINS
        || (staged == CV_STAGE_OK && published != CV_PUBLISH_OK)) {
        if (!cv_remove_temp(temp_native)) {
            fputs("cv: manifest temporary file requires cleanup\n", stderr);
        }
    }
    if (ok) {
        cv_crash_at("manifest-visible");
    }
    if (ok && !cv_sync_directory(snapshots)) {
        fputs("cv: snapshot is visible but directory synchronization failed\n", stderr);
        ok = 0;
    } else if (ok) {
        puts(snapshot);
    } else {
        fputs("cv: backup failed or snapshot already exists\n", stderr);
    }

    cv_unlock_file(&lock);
    free(destination_native);
    free(temp_native);
    free(object_native);
    free(destination);
    free(temp);
    free(object_temp);
    free(snapshots);
    free(objects);
    return ok;
}

struct snapshot_scan {
    const char *root;
    int verify;
    int valid;
    size_t count;
    size_t capacity;
    char (*ids)[65];
};

static int scan_snapshot(const char *name, void *context)
{
    struct snapshot_scan *scan = context;
    size_t length = strlen(name);
    if (length <= 4 || length > 68 || strcmp(name + length - 4, ".cvm") != 0) {
        return 1;
    }
    char id[65];
    memcpy(id, name, length - 4);
    id[length - 4] = '\0';
    if (!valid_snapshot(id)) {
        return 1;
    }
    if (scan->verify) {
        if (!cv_repo_read(scan->root, id, NULL)) {
            fprintf(stderr, "cv: invalid snapshot %s\n", id);
            scan->valid = 0;
        }
        return 1;
    }
    if (scan->count == scan->capacity) {
        if (scan->capacity > SIZE_MAX / sizeof *scan->ids / 2) {
            return 0;
        }
        size_t capacity = scan->capacity ? scan->capacity * 2 : 32;
        void *ids = realloc(scan->ids, capacity * sizeof *scan->ids);
        if (!ids) {
            return 0;
        }
        scan->ids = ids;
        scan->capacity = capacity;
    }
    memcpy(scan->ids[scan->count++], id, length - 3);
    return 1;
}

static int compare_snapshot_ids(const void *a, const void *b)
{
    return strcmp(a, b);
}

static int scan_snapshots(const char *root, int verify)
{
    char *path = cv_join(root, "snapshots");
    struct snapshot_scan scan = {.root = root, .verify = verify, .valid = 1};
    int ok = path && repository_version(root) && cv_visit(path, scan_snapshot, &scan);
    free(path);
    if (ok && scan.count > 0) {
        qsort(scan.ids, scan.count, sizeof *scan.ids, compare_snapshot_ids);
        for (size_t i = 0; ok && i < scan.count; ++i) {
            ok = puts(scan.ids[i]) != EOF;
        }
        ok = fflush(stdout) == 0 && ok;
    }
    free(scan.ids);
    if (!ok) {
        fputs(verify ? "cv: cannot scan repository\n" : "cv: cannot list snapshots\n", stderr);
    }
    return ok && scan.valid;
}

int cv_repo_list(const char *root)
{
    return scan_snapshots(root, 0);
}

int cv_repo_verify(const char *root)
{
    return scan_snapshots(root, 1);
}

static int read_file(FILE *manifest, const char *objects, FILE *output,
                     unsigned char buffer[CV_CDC_MAX], int version)
{
    uint64_t remaining;
    if (!get_number(manifest, &remaining, 8)) {
        return 0;
    }
    while (remaining > 0) {
        uint64_t wanted = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;
        if (version == 2 && (!get_number(manifest, &wanted, 4)
            || wanted == 0 || wanted > remaining || wanted > CV_CDC_MAX
            || (wanted < CV_CDC_MIN && wanted != remaining))) {
            return 0;
        }
        unsigned char expected[CV_SHA256_SIZE], actual[CV_SHA256_SIZE];
        if (fread(expected, 1, sizeof expected, manifest) != sizeof expected) {
            return 0;
        }
        char *path = object_path(objects, expected);
        struct cv_source_stamp stamp = {0};
        FILE *object = path ? cv_open_source(path, &stamp) : NULL;
        if (!object) {
            fprintf(stderr, "cv: missing or unreadable chunk: %s\n", path ? path : "(path unavailable)");
            free(path);
            return 0;
        }
        if (stamp.size != wanted) {
            fprintf(stderr, "cv: incorrect chunk size: %s\n", path);
            free(path);
            fclose(object);
            return 0;
        }
        size_t count;
        int ok = cv_chunk_read(object, buffer, (size_t)wanted, &count, actual) == CV_CHUNK_OK
                 && count == wanted && memcmp(expected, actual, sizeof expected) == 0
                 && fgetc(object) == EOF && !ferror(object);
        ok = close_file(object, ok);
        if (!ok) {
            fprintf(stderr, "cv: chunk integrity check failed: %s\n", path);
        }
        free(path);
        if (!ok || (output && fwrite(buffer, 1, count, output) != count)) {
            return 0;
        }
        remaining -= count;
    }
    return 1;
}

struct manifest_path {
    char *name;
    int type;
};

struct manifest_paths {
    struct manifest_path *entries;
    size_t capacity;
    size_t used;
};

static struct manifest_path *path_slot(struct manifest_paths *paths, const char *name)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char *p = (const unsigned char *)name; *p; ++p) {
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    }
    size_t index = (size_t)hash & (paths->capacity - 1);
    while (paths->entries[index].name && strcmp(paths->entries[index].name, name) != 0) {
        index = (index + 1) & (paths->capacity - 1);
    }
    return &paths->entries[index];
}

static int add_manifest_path(struct manifest_paths *paths, char *name, int type)
{
    if (paths->used == paths->capacity / 2) {
        if (paths->capacity > SIZE_MAX / sizeof *paths->entries / 2) {
            return 0;
        }
        struct manifest_paths grown = {.capacity = paths->capacity ? paths->capacity * 2 : 32,
                                       .used = paths->used};
        grown.entries = calloc(grown.capacity, sizeof *grown.entries);
        if (!grown.entries) {
            return 0;
        }
        for (size_t i = 0; i < paths->capacity; ++i) {
            if (paths->entries[i].name) {
                *path_slot(&grown, paths->entries[i].name) = paths->entries[i];
            }
        }
        free(paths->entries);
        *paths = grown;
    }

    char *slash = strrchr(name, '/');
    if (slash) {
        *slash = '\0';
        int parent_type = path_slot(paths, name)->type;
        *slash = '/';
        if (parent_type != 'D') {
            return 0;
        }
    }

    struct manifest_path *slot = path_slot(paths, name);
    if (slot->name) {
        return 0;
    }
    size_t length = strlen(name) + 1;
    slot->name = malloc(length);
    if (!slot->name) {
        return 0;
    }
    memcpy(slot->name, name, length);
    slot->type = type;
    ++paths->used;
    return 1;
}

static int read_records(FILE *manifest, const char *objects, const char *output,
                        struct manifest_paths *paths, int version)
{
    char header[sizeof magic_v1 - 1];
    unsigned char buffer[CV_CDC_MAX];
    if (fread(header, 1, sizeof header, manifest) != sizeof header
        || memcmp(header, manifest_magic(version), sizeof header) != 0) {
        return 0;
    }
    for (;;) {
        int type = fgetc(manifest);
        if (type == 'E') {
            return fgetc(manifest) == EOF && !ferror(manifest);
        }
        uint64_t length;
        char relative[PATH_LIMIT];
        if ((type != 'D' && type != 'F') || !get_number(manifest, &length, 4)
            || length == 0 || length >= sizeof relative
            || fread(relative, 1, (size_t)length, manifest) != length
            || memchr(relative, '\0', (size_t)length)) {
            return 0;
        }
        relative[length] = '\0';
        if (!valid_relative(relative) || !add_manifest_path(paths, relative, type)) {
            return 0;
        }
        char *path = output ? cv_join(output, relative) : NULL;
        if (output && !path) {
            return 0;
        }

        int ok;
        if (type == 'D') {
            ok = !output || cv_mkdir(path);
        } else {
            cv_path_char *native = path ? cv_native(path) : NULL;
            enum cv_stage_result staged = native ? cv_stage(native, NULL, 0) : CV_STAGE_ERROR;
            FILE *file = staged == CV_STAGE_OK ? cv_open(path, 1) : NULL;
            ok = (!output || file) && read_file(manifest, objects, file, buffer, version);
            if (file) {
                ok = close_file(file, ok && cv_sync(file));
            }
            if (!ok && (staged == CV_STAGE_OK || staged == CV_STAGE_TEMP_REMAINS)) {
                cv_remove_temp(native);
            }
            free(native);
        }
        free(path);
        if (!ok) {
            return 0;
        }
    }
}

static int read_manifest(FILE *manifest, const char *objects, const char *output, int version)
{
    struct manifest_paths paths = {0};
    int ok = read_records(manifest, objects, output, &paths, version);
    for (size_t i = 0; i < paths.capacity; ++i) {
        free(paths.entries[i].name);
    }
    free(paths.entries);
    return ok;
}

static int remove_restore_entry(const char *name, void *context);

static int remove_restore_directory(const char *path)
{
    return cv_visit(path, remove_restore_entry, (void *)path) && cv_rmdir(path);
}

static int remove_restore_entry(const char *name, void *context)
{
    char *path = cv_join(context, name);
    if (!path) {
        return 0;
    }
    int ok;
    if (cv_kind(path) == CV_FILE_DIRECTORY) {
        ok = remove_restore_directory(path);
    } else {
        cv_path_char *native = cv_native(path);
        ok = native && cv_remove_temp(native);
        free(native);
    }
    free(path);
    return ok;
}

#ifndef _WIN32
static int sync_restored_directory(const char *path);

static int sync_restored_entry(const char *name, void *context)
{
    char *path = cv_join(context, name);
    if (!path) {
        return 0;
    }
    enum cv_file_kind kind = cv_kind(path);
    int ok = kind == CV_FILE_REGULAR
             || (kind == CV_FILE_DIRECTORY && sync_restored_directory(path));
    free(path);
    return ok;
}

static int sync_restored_directory(const char *path)
{
    return cv_visit(path, sync_restored_entry, (void *)path) && cv_sync_directory(path);
}
#endif

static int restore(FILE *manifest, const struct cv_source_stamp *stamp,
                   const char *objects, const char *output, int version)
{
    size_t length = strlen(output);
    if (length == 0 || output[length - 1] == '/' || output[length - 1] == '\\'
        || length > (size_t)-1 - 48) {
        fputs("cv: invalid restore destination path\n", stderr);
        return 0;
    }
    if (cv_kind(output) != CV_FILE_MISSING) {
        fprintf(stderr, "cv: restore destination exists or is inaccessible: %s\n", output);
        return 0;
    }
    char *pending = malloc(length + 48);
    if (!pending) {
        return 0;
    }
    snprintf(pending, length + 48, "%s.cv-%lu.tmp", output, cv_process_id());
    cv_path_char *from = cv_native(pending);
    cv_path_char *to = cv_native(output);
    int created = from && to && cv_mkdir(pending);
    int ready = created && read_manifest(manifest, objects, pending, version)
                && cv_source_unchanged(manifest, stamp);
#ifndef _WIN32
    ready = ready && sync_restored_directory(pending);
#endif
    enum cv_publish_result result = ready ? cv_publish(from, to) : CV_PUBLISH_ERROR;
    int published = result == CV_PUBLISH_OK;
    if (ready && !published) {
        fprintf(stderr, "cv: cannot publish restore without replacing the destination: %s\n", output);
    }
    int ok = published && cv_sync_parent(output);
    if (published && !ok) {
        fputs("cv: restore is visible but directory synchronization failed\n", stderr);
    }
    if (created && !published && !remove_restore_directory(pending)) {
        fprintf(stderr, "cv: restore temporary directory requires cleanup: %s\n", pending);
    }
    free(to);
    free(from);
    free(pending);
    return ok;
}

int cv_repo_read(const char *root, const char *snapshot, const char *output)
{
    char *snapshots = cv_join(root, "snapshots");
    char *objects = cv_join(root, "objects");
    char *path = snapshots && valid_snapshot(snapshot) ? snapshot_path(snapshots, snapshot) : NULL;
    int version = repository_version(root);
    struct cv_source_stamp stamp;
    FILE *file = path && objects && version ? cv_open_source(path, &stamp) : NULL;
    int ok = file && read_manifest(file, objects, NULL, version)
             && cv_source_unchanged(file, &stamp);
    if (ok && output) {
        ok = fseek(file, 0, SEEK_SET) == 0
             && restore(file, &stamp, objects, output, version);
    }
    if (file) {
        ok = close_file(file, ok);
    }
    if (!ok) {
        fputs(output ? "cv: restore failed\n"
                     : "cv: snapshot verification failed\n", stderr);
    }
    free(path);
    free(objects);
    free(snapshots);
    return ok;
}

struct recovery {
    const char *directory;
    int remove;
};

static int temp_name(const char *name)
{
    if (*name++ != '.') {
        return 0;
    }
    for (int field = 0; field < 2; ++field) {
        const char *start = name;
        while (*name >= '0' && *name <= '9') {
            ++name;
        }
        if (name == start) {
            return 0;
        }
        if (strcmp(name, ".tmp") == 0) {
            return 1;
        }
        if (*name++ != '-') {
            return 0;
        }
    }
    return 0;
}

static int recover_temp(const char *name, void *context)
{
    const struct recovery *recovery = context;
    if (!temp_name(name) && strcmp(name, ".format.tmp") != 0) {
        return 1;
    }
    char *path = cv_join(recovery->directory, name);
    if (!path) {
        return 0;
    }
    int ok = 1;
    if (cv_kind(path) == CV_FILE_REGULAR) {
        cv_path_char *native = recovery->remove ? cv_native(path) : NULL;
        ok = (!recovery->remove || (native && cv_remove_temp(native))) && puts(path) != EOF;
        free(native);
    }
    free(path);
    return ok;
}

int cv_repo_recover(const char *root, int remove)
{
    if (!repository_version(root)) {
        fputs("cv: cannot recover an invalid repository\n", stderr);
        return 0;
    }

    struct cv_lock lock;
    if (!repository_lock(root, 1, &lock)) {
        return 0;
    }
    char *objects = cv_join(root, "objects");
    char *snapshots = cv_join(root, "snapshots");
    struct recovery recovery = {root, remove};
    int ok = objects && snapshots && recover_temp(".format.tmp", &recovery);
    recovery.directory = objects;
    ok = ok && cv_visit(objects, recover_temp, &recovery);
    recovery.directory = snapshots;
    ok = ok && cv_visit(snapshots, recover_temp, &recovery);
    if (ok && remove) {
        ok = cv_sync_directory(objects) && cv_sync_directory(snapshots) && cv_sync_directory(root);
    }
    free(snapshots);
    free(objects);
    cv_unlock_file(&lock);
    if (!ok) {
        fputs("cv: recovery could not finish\n", stderr);
    }
    return ok;
}
