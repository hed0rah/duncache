#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fts.h>

/*
 * incache.c
 * check if files are in the linux page cache. reports pages resident
 * out of total pages using mincore().
 */

static int quiet = 0;
static int json  = 0;
static int any_not_fully_cached = 0;
static int any_error = 0;
static int first_json = 1;

typedef struct {
    off_t size;
    unsigned long pages;
    unsigned long cached;
} cache_info;

static int probe(const char *path, cache_info *out);
static void report(const char *path, const cache_info *ci);
static int walk_directory(const char *dirpath);

static void usage(FILE *f, const char *prog) {
    fprintf(f,
        "usage: %s [options] <path>...\n"
        "\n"
        "  -q, --quiet      no output; exit code only\n"
        "  -j, --json       emit one json object per path\n"
        "  -h, --help       show this help\n"
        "\n"
        "exits 0 if every path is fully cached, 1 if any path is partially\n"
        "or not cached, 2 on error (open/mmap/mincore failure).\n",
        prog);
}

int main(int argc, char* const argv[]) {
    int first = 1;
    for (; first < argc; ++first) {
        const char *a = argv[first];
        if (!strcmp(a, "-q") || !strcmp(a, "--quiet")) { quiet = 1; continue; }
        if (!strcmp(a, "-j") || !strcmp(a, "--json"))  { json  = 1; continue; }
        if (!strcmp(a, "-h") || !strcmp(a, "--help"))  { usage(stdout, argv[0]); return 0; }
        if (!strcmp(a, "--"))                          { first++; break; }
        if (a[0] == '-' && a[1] != '\0')               { usage(stderr, argv[0]); return 2; }
        break;
    }
    if (first >= argc) { usage(stderr, argv[0]); return 2; }

    if (json && !quiet) fputs("[", stdout);

    for (int i = first; i < argc; i++) {
        struct stat sb;
        if (stat(argv[i], &sb) == 0 && S_ISDIR(sb.st_mode)) {
            walk_directory(argv[i]);
        } else {
            cache_info ci = {0};
            if (probe(argv[i], &ci) == 0) report(argv[i], &ci);
        }
    }

    if (json && !quiet) fputs("]\n", stdout);

    if (any_error) return 2;
    if (any_not_fully_cached) return 1;
    return 0;
}

static int walk_directory(const char *dirpath) {
    char *paths[] = { (char *)dirpath, NULL };
    FTS *ftsp = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
    if (!ftsp) {
        fprintf(stderr, "fts_open %s: %s\n", dirpath, strerror(errno));
        any_error = 1;
        return -1;
    }
    FTSENT *ent;
    while ((ent = fts_read(ftsp)) != NULL) {
        if (ent->fts_info == FTS_F) {
            cache_info ci = {0};
            if (probe(ent->fts_accpath, &ci) == 0) report(ent->fts_path, &ci);
        } else if (ent->fts_info == FTS_ERR || ent->fts_info == FTS_DNR) {
            fprintf(stderr, "%s: %s\n", ent->fts_path, strerror(ent->fts_errno));
            any_error = 1;
        }
    }
    fts_close(ftsp);
    return 0;
}

static int probe(const char *filepath, cache_info *out) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "open %s: %s\n", filepath, strerror(errno));
        any_error = 1;
        return -1;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        fprintf(stderr, "fstat %s: %s\n", filepath, strerror(errno));
        close(fd);
        any_error = 1;
        return -1;
    }

    out->size = sb.st_size;
    if (sb.st_size == 0) {
        out->pages = 0;
        out->cached = 0;
        close(fd);
        return 0;  /* empty file: trivially "fully cached" */
    }

    unsigned long pagesize = (unsigned long)sysconf(_SC_PAGESIZE);
    unsigned long pagecount = ((unsigned long)sb.st_size + pagesize - 1) / pagesize;
    unsigned char *vec = malloc(pagecount);
    if (!vec) {
        fprintf(stderr, "malloc: %s\n", strerror(errno));
        close(fd);
        any_error = 1;
        return -1;
    }

    void *mapped = mmap(NULL, sb.st_size, PROT_NONE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        fprintf(stderr, "mmap %s: %s\n", filepath, strerror(errno));
        free(vec);
        close(fd);
        any_error = 1;
        return -1;
    }

    if (mincore(mapped, sb.st_size, vec) == -1) {
        fprintf(stderr, "mincore %s: %s\n", filepath, strerror(errno));
        munmap(mapped, sb.st_size);
        free(vec);
        close(fd);
        any_error = 1;
        return -1;
    }

    unsigned long cached = 0;
    for (unsigned long i = 0; i < pagecount; i++) {
        if (vec[i] & 1) cached++;
    }

    out->pages = pagecount;
    out->cached = cached;

    munmap(mapped, sb.st_size);
    free(vec);
    close(fd);
    return 0;
}

static void report(const char *path, const cache_info *ci) {
    if (ci->pages > 0 && ci->cached < ci->pages) any_not_fully_cached = 1;

    if (quiet) return;

    double pct = ci->pages ? (100.0 * (double)ci->cached / (double)ci->pages) : 100.0;

    if (json) {
        if (!first_json) fputs(",", stdout);
        first_json = 0;
        printf("{\"path\":\"%s\",\"size\":%lld,\"pages\":%lu,\"cached\":%lu,\"pct\":%.2f}",
               path, (long long)ci->size, ci->pages, ci->cached, pct);
    } else {
        printf("%s\t%lu/%lu pages (%.1f%%)\n",
               path, ci->cached, ci->pages, pct);
    }
}
