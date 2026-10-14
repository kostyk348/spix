CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -pedantic -std=c99
LDLIBS  ?= -lm

.PHONY: all clean test bench demo generate

all: examples/demo

examples/demo: examples/demo.c spix.h
	$(CC) $(CFLAGS) -o $@ examples/demo.c $(LDLIBS)

tests/test_spix: tests/test_spix.c spix.h
	$(CC) $(CFLAGS) -o $@ tests/test_spix.c $(LDLIBS)

bench/bench_spix: bench/bench_spix.c spix.h
	$(CC) $(CFLAGS) -o $@ bench/bench_spix.c $(LDLIBS)

# correctness tests (exit code 0 = every check passed) — this is what CI runs
test: tests/test_spix
	./tests/test_spix

# throughput benchmark (timing, so deliberately not part of CI)
bench: bench/bench_spix
	./bench/bench_spix

generate: examples/demo
	./examples/demo generate

# the original demo pipeline: generate a test pattern, then filter it
demo: examples/demo generate
	./examples/demo

clean:
	rm -f examples/demo examples/demo.exe tests/test_spix tests/test_spix.exe \
	      bench/bench_spix bench/bench_spix.exe *.ppm spix_test_* spix_bench_*
