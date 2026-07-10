CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -pedantic -std=c99 -lm

.PHONY: all clean test generate

all: examples/demo

examples/demo: examples/demo.c spix.h
	$(CC) $(CFLAGS) -o $@ examples/demo.c -lm

generate: examples/demo
	./examples/demo generate

test: examples/demo generate
	./examples/demo

clean:
	rm -f examples/demo examples/demo.exe *.ppm
