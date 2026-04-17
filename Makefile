CC     ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99 -O2

TARGETS = duncache incache

all: $(TARGETS)

duncache: duncache.c
	$(CC) $(CFLAGS) -o $@ $<

incache: incache.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean
