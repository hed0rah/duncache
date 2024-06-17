#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <string.h>
#include <errno.h>

/*
 * duncache.c
 * Remove files/directories from the Linux page cache with POSIX_FADV_DONTNEED and fts().
 * http://github.com/jrelo/duncache
 */

void duncache_file(const char *filepath);
void duncache_directory(const char *dirpath);
int compare(const FTSENT **, const FTSENT **);

int main(int argc, char *const argv[]) {
    if (argc < 2) {
        printf("Usage: %s <path>\n", argv[0]);
        exit(255);
    }

    for (int i = 1; i < argc; i++) {
        struct stat sb;
        if (stat(argv[i], &sb) == 0 && S_ISDIR(sb.st_mode)) {
            duncache_directory(argv[i]);
        } else {
            duncache_file(argv[i]);
        }
    }

    return 0;
}

void duncache_file(const char *filepath) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return;
    }

    if (fdatasync(fd) == -1) {
        perror("fdatasync");
        close(fd);
        return;
    }

    if (posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED) == -1) {
        perror("posix_fadvise");
        close(fd);
        return;
    }

    printf("%s -> POSIX_FADV_DONTNEED\n", filepath);
    close(fd);
}

void duncache_directory(const char *dirpath) {
    char *paths[] = {(char *)dirpath, NULL};
    FTS *fs = fts_open(paths, FTS_COMFOLLOW | FTS_NOCHDIR, &compare);
    if (NULL == fs) {
        perror("fts_open");
        return;
    }

    FTSENT *child = NULL;
    FTSENT *parent = NULL;

    while ((parent = fts_read(fs)) != NULL) {
        child = fts_children(fs, 0);

        if (errno != 0) {
            perror("fts_children");
        }

        while ((NULL != child)) {
            duncache_file(child->fts_accpath);
            child = child->fts_link;
        }
    }
    fts_close(fs);
}

int compare(const FTSENT **one, const FTSENT **two) {
    return (strcmp((*one)->fts_name, (*two)->fts_name));
}
