# duncache

duncache is a utility to remove files in a directory from the Linux page cache using `POSIX_FADV_DONTNEED` and `fts()` functions.

## Usage

```bash
duncache <path-spec>
