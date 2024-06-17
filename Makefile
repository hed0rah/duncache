# Makefile for compiling buffer examples

CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGETS = duncache incache 

all: $(TARGETS)

linear_buffer: duncache.c
	$(CC) $(CFLAGS) -o duncache duncache.c

ring_buffer: incache.c
	$(CC) $(CFLAGS) -o incache incache.c

clean:
	rm -f $(TARGETS)

.PHONY: all clean
