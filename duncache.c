#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

/*
 * duncache.c
 * remove files/directories from the linux page cache with
 * POSIX_FADV_DONTNEED and fts().
 */

static int quiet = 0;
static int no_sync = 0;
static int any_failed = 0;

static int duncache_file(const char *filepath);
static int duncache_directory(const char *dirpath);
static int compare(const FTSENT **, const FTSENT **);

static void usage(FILE *f, const char *prog) {
    fprintf(f,
        "usage: %s [options] <path>...\n"
        "\n"
        "  -q, --quiet      no output unless an error occurs\n"
        "      --no-sync    skip fdatasync() before eviction (faster,\n"
        "                   safe only when you know no writes are pending)\n"
        "  -h, --help       show this help\n"
        "\n"
        "exits 0 if every path was evicted, 1 if any path failed.\n",
        prog);
}

int main(int argc, char *const argv[]) {
    int first = 1;
    for (; first < argc; ++first) {
        const char *a = argv[first];
        if (!strcmp(a, "-q") || !strcmp(a, "--quiet"))   { quiet = 1; continue; }
        if (!strcmp(a, "--no-sync"))                     { no_sync = 1; continue; }
        if (!strcmp(a, "-h") || !strcmp(a, "--help"))    { usage(stdout, argv[0]); return 0; }
        if (!strcmp(a, "--"))                            { first++; break; }
        if (a[0] == '-' && a[1] != '\0')                 { usage(stderr, argv[0]); return 2; }
        break;
    }

    if (first >= argc) {
        usage(stderr, argv[0]);
        return 2;
    }

    for (int i = first; i < argc; i++) {
        struct stat sb;
        if (stat(argv[i], &sb) == 0 && S_ISDIR(sb.st_mode)) {
            duncache_directory(argv[i]);
        } else {
            duncache_file(argv[i]);
        }
    }

    return any_failed ? 1 : 0;
}

static int duncache_file(const char *filepath) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "open %s: %s\n", filepath, strerror(errno));
        any_failed = 1;
        return -1;
    }

    if (!no_sync && fdatasync(fd) == -1) {
        fprintf(stderr, "fdatasync %s: %s\n", filepath, strerror(errno));
        close(fd);
        any_failed = 1;
        return -1;
    }

    if (posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED) == -1) {
        fprintf(stderr, "posix_fadvise %s: %s\n", filepath, strerror(errno));
        close(fd);
        any_failed = 1;
        return -1;
    }

    if (!quiet) {
        printf("%s -> POSIX_FADV_DONTNEED\n", filepath);
    }
    close(fd);
    return 0;
}

static int duncache_directory(const char *dirpath) {
    char *paths[] = {(char *)dirpath, NULL};
    FTS *fs = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR, &compare);
    if (NULL == fs) {
        fprintf(stderr, "fts_open %s: %s\n", dirpath, strerror(errno));
        any_failed = 1;
        return -1;
    }

    FTSENT *ent;
    while ((ent = fts_read(fs)) != NULL) {
        if (ent->fts_info == FTS_F) {
            duncache_file(ent->fts_accpath);
        } else if (ent->fts_info == FTS_ERR || ent->fts_info == FTS_DNR) {
            fprintf(stderr, "%s: %s\n", ent->fts_path, strerror(ent->fts_errno));
            any_failed = 1;
        }
    }
    fts_close(fs);
    return 0;
}

static int compare(const FTSENT **one, const FTSENT **two) {
    return strcmp((*one)->fts_name, (*two)->fts_name);
}
