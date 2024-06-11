# Page Cache Utilities

This repository contains utilities to manage and check the Linux page cache.

## Tools

- `duncache`: Remove files in a directory from the Linux page cache using `POSIX_FADV_DONTNEED`.
- `incache`: Check if files in a directory are in the Linux page cache.

## Compilation

```sh
gcc -o duncache duncache.c
gcc -o incache incache.c

Example Usage:

# Create a 1MB test file with random data
dd if=/dev/urandom of=testfile bs=1M count=1

# Output:
1+0 records in
1+0 records out
1048576 bytes (1.0 MB, 1.0 MiB) copied, 0.0279166 s, 37.6 MB/s

# Read the file to ensure it is loaded into the page cache
cat testfile > /dev/null

# Check if the file is in the page cache
./incache ./testfile

# Output:
./testfile is in the page cache.

# Remove the file from the page cache
./duncache ./testfile

# Output:
./testfile -> POSIX_FADV_DONTNEED

# Check if the file is in the page cache again
./incache ./testfile

# Output:
./testfile is NOT in the page cache.
