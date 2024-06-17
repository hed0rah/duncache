#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <fts.h>
#include <string.h>

/*
 * incache.c
 * Check if files are in the Linux page cache.
 * http://github.com/jrelo/duncache
 */

int is_file_in_page_cache(const char *filepath);
void check_directory(const char *dirpath);

int main(int argc, char* const argv[]) {
    if (argc < 2) {
        printf("Usage: %s <path>\n", argv[0]);
        exit(255);
    }

    for (int i = 1; i < argc; i++) {
        struct stat sb;
        if (stat(argv[i], &sb) == 0 && S_ISDIR(sb.st_mode)) {
            check_directory(argv[i]);
        } else {
            if (is_file_in_page_cache(argv[i])) {
                printf("%s is in the page cache.\n", argv[i]);
            } else {
                printf("%s is NOT in the page cache.\n", argv[i]);
            }
        }
    }

    return 0;
}

void check_directory(const char *dirpath) {
    char *paths[] = { (char *)dirpath, NULL };
    FTS *ftsp = fts_open(paths, FTS_COMFOLLOW | FTS_NOCHDIR, NULL);
    if (!ftsp) {
        perror("fts_open");
        return;
    }

    FTSENT *entry;
    while ((entry = fts_read(ftsp)) != NULL) {
        if (entry->fts_info & FTS_F) {
            if (is_file_in_page_cache(entry->fts_path)) {
                printf("%s is in the page cache.\n", entry->fts_path);
            } else {
                printf("%s is NOT in the page cache.\n", entry->fts_path);
            }
        }
    }

    fts_close(ftsp);
}

int is_file_in_page_cache(const char *filepath) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 0;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        close(fd);
        return 0;
    }

    unsigned long pagesize = getpagesize();
    unsigned long pagecount = (sb.st_size + pagesize - 1) / pagesize;
    unsigned char *vec = malloc(pagecount);

    if (vec == NULL) {
        perror("malloc");
        close(fd);
        return 0;
    }

    void *mapped = mmap(NULL, sb.st_size, PROT_NONE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        perror("mmap");
        free(vec);
        close(fd);
        return 0;
    }

    if (mincore(mapped, sb.st_size, vec) == -1) {
        perror("mincore");
        munmap(mapped, sb.st_size);
        free(vec);
        close(fd);
        return 0;
    }

    int in_cache = 1;
    for (unsigned long i = 0; i < pagecount; i++) {
        if ((vec[i] & 1) == 0) {
            in_cache = 0;
            break;
        }
    }

    munmap(mapped, sb.st_size);
    free(vec);
    close(fd);
    return in_cache;
}
