#include "repo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include "fs.h"
#include <windows.h>
#include <shellapi.h>
#endif

static int run(int argc, char *argv[])
{
    const char *usage = "Usage: cv [--help | --version]\n"
                        "       cv init REPO\n"
                        "       cv backup REPO SOURCE [SNAPSHOT]\n"
                        "       cv list REPO\n"
                        "       cv verify REPO [SNAPSHOT]\n"
                        "       cv restore REPO SNAPSHOT DESTINATION\n"
                        "       cv recover REPO [--remove]\n";

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        fputs(usage, stdout);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        puts("ChunkVault " CV_VERSION);
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "init") == 0) {
        return !cv_repo_init(argv[2]);
    }
    if (argc == 3 && strcmp(argv[1], "list") == 0) {
        return !cv_repo_list(argv[2]);
    }
    if ((argc == 4 || argc == 5) && strcmp(argv[1], "backup") == 0) {
        return !cv_repo_backup(argv[2], argv[3], argc == 5 ? argv[4] : NULL);
    }
    if (argc == 3 && strcmp(argv[1], "verify") == 0) {
        return !cv_repo_verify(argv[2]);
    }
    if (argc == 4 && strcmp(argv[1], "verify") == 0) {
        return !cv_repo_read(argv[2], argv[3], NULL);
    }
    if ((argc == 3 || (argc == 4 && strcmp(argv[3], "--remove") == 0))
        && strcmp(argv[1], "recover") == 0) {
        return !cv_repo_recover(argv[2], argc == 4);
    }
    if (argc == 5 && strcmp(argv[1], "restore") == 0) {
        return !cv_repo_read(argv[2], argv[3], argv[4]);
    }

    if (argc > 1) {
        fputs("cv: invalid arguments\n", stderr);
    }
    fputs(usage, stderr);
    return 2;
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    (void)argv;
    wchar_t **wide = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wide) {
        return 1;
    }
    char **utf8 = calloc((size_t)argc + 1, sizeof *utf8);
    int result = 1;
    if (utf8) {
        int i = 0;
        while (i < argc && (utf8[i] = cv_utf8(wide[i])) != NULL) {
            ++i;
        }
        if (i == argc) {
            result = run(argc, utf8);
        }
        while (i > 0) {
            free(utf8[--i]);
        }
    }
    free(utf8);
    LocalFree(wide);
    return result;
#else
    return run(argc, argv);
#endif
}
