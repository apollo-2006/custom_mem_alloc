CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
LDLIBS = -lpthread

all: custom_allocator

custom_allocator: main.c src/my_allocator.c include/my_allocator.h
	$(CC) $(CFLAGS) main.c src/my_allocator.c -o $@ $(LDLIBS)

test: tests/test_alloc.c src/my_allocator.c include/my_allocator.h bench/workload.h
	$(CC) $(CFLAGS) -g tests/test_alloc.c src/my_allocator.c -o test_alloc $(LDLIBS)
	./test_alloc

bench: bench/bench.c src/my_allocator.c include/my_allocator.h bench/workload.h
	$(CC) $(CFLAGS) bench/bench.c src/my_allocator.c -o bench_alloc $(LDLIBS)
	./bench_alloc

clean:
	rm -f custom_allocator test_alloc bench_alloc

.PHONY: all test bench clean
