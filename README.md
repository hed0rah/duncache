# Page Cache Utilities

Small Linux utilities for managing the page cache. Useful for cold-cache
benchmarking, I/O investigation, and anything that needs to know or
control what the kernel has cached from disk.

## Tools

- `duncache`: evict files or directory trees from the page cache using
  `posix_fadvise(..., POSIX_FADV_DONTNEED)`.
- `incache`: report how many pages of a file (or tree) are resident in
  the page cache, using `mincore()`.

## Build

```sh
make
```

## Usage

```
duncache [-q] [--no-sync] <path>...
incache  [-q] [-j|--json]  <path>...
```

### duncache

```
-q, --quiet      no output unless an error occurs
    --no-sync    skip fdatasync() before eviction
```

Exits 0 if every path was evicted, 1 if any failed.

### incache

```
-q, --quiet      no output; exit code only
-j, --json       one json object per path
```

Reports `cached/total pages (percent)`. Exits 0 if every path is fully
cached, 1 if any path is partially or not cached, 2 on error.

## Example

```sh
# create a 1 MiB test file
dd if=/dev/urandom of=testfile bs=1M count=1

# read it so the kernel caches it
cat testfile > /dev/null

# check residency
./incache testfile
# testfile	256/256 pages (100.0%)

# evict
./duncache testfile
# testfile -> POSIX_FADV_DONTNEED

# check again
./incache testfile
# testfile	0/256 pages (0.0%)

# scriptable use
if ./incache -q testfile; then
    echo "fully cached"
else
    echo "not (fully) cached"
fi
```
