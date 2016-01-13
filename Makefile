CFLAGS=-g -Wall -std=c99

all: h2-cache-digest-00

h2-cache-digest-00: h2-cache-digest-00.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f h2-cache-digest-00

.PHONY: clean
