# custom_mem_alloc

[![demo](https://github.com/apollo-2006/custom_mem_alloc/actions/workflows/pages.yml/badge.svg)](https://github.com/apollo-2006/custom_mem_alloc/actions/workflows/pages.yml)
[![license: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A thread-safe memory allocator written from scratch in C. It requests one large
contiguous region of virtual memory from the kernel up front via `mmap`, then hands
slices of that region to the program itself, so no call after the first one goes back
to the operating system.

**[Watch it allocate in your browser →](https://apollo-2006.github.io/custom_mem_alloc/)**
The allocator is compiled to WebAssembly and its block list is drawn after every call:
split blocks, coalescing, fragmentation that makes a 4 KB request fail with half the
arena free, and the aborts on a double free or an interior pointer.

Written to find out what `malloc` is actually doing underneath. The bug that taught me
the most is written up in the [post-mortem](https://abirdeol.tech/research/allocator).

## How it works

* **One arena, mapped once.** A single 1 MB `mmap` region is created lazily on the
  first allocation. Every later request is served out of that region.
* **Intrusive block headers.** Each block carries a `BlockHeader` immediately before
  its payload, holding the payload size, a free flag, and a pointer to the next block.
  The list is threaded through the arena itself rather than stored beside it, in address
  order. The header is 24 bytes on 64-bit targets and 12 on wasm32.
* **First-fit with splitting.** `my_malloc` walks the block list for the first free
  block large enough, then splits off the remainder as a new free block when the
  leftover can hold a header plus at least 8 bytes of payload.
* **Coalescing on free, neighbours only.** Every free merges the block with its free
  neighbours, so two adjacent free blocks never exist. That invariant is what lets
  `my_free` touch only the block before and after instead of sweeping the whole list,
  and it is why the arena returns to a single block once everything is freed.
* **Misuse aborts instead of corrupting.** Freeing a pointer from outside the arena, a
  pointer into the middle of a block, or a block that is already free prints a message and
  calls `abort()`, the way glibc does. Requests larger than the arena are refused before
  alignment rounding, which is where `my_malloc(SIZE_MAX)` used to wrap around to 0 and
  return a live pointer.
* **8-byte alignment.** Every payload is rounded up to an 8-byte boundary, which covers
  integers, pointers and `double`. It does not cover `long double` or SIMD types, which
  want 16 bytes on x86-64 (`alignof(max_align_t)`), so this is not a drop-in `malloc`.
* **Thread safety.** A single `pthread_mutex` guards the arena, so the allocator can be
  called from multiple threads without racing on the block list.

## API

```c
void* my_malloc(size_t size);
void  my_free(void* ptr);
void* my_calloc(size_t num, size_t size);
void  print_heap_metadata(void);   // dumps every block: address, size, free/allocated
```

## Build, test, benchmark

```bash
git clone https://github.com/apollo-2006/custom_mem_alloc.git
cd custom_mem_alloc

make && ./custom_allocator   # allocate, print the arena, free, print it again
make test                    # correctness, including the abort cases
make bench                   # against the platform malloc
```

`make test` checks the size edge cases (`0`, `SIZE_MAX`, larger than the arena, `calloc`
overflow), zeroing, alignment, eight threads running 200k verified operations each, full
coalescing back to one block, and that the three misuse cases really abort (each in a
forked child).

## Performance

`bench/workload.h` is a random allocate/free workload over 256 live slots, sized the
way programs usually allocate (three requests in four under 64 bytes). Every block is
filled and checked before it is freed, so an allocator that hands out overlapping memory
fails instead of looking fast. Ryzen 9 5900XT, gcc `-O2`, 2M operations, best of five:

| sizes | my_malloc | glibc malloc |
|---|---|---|
| 1 to 256 bytes | 107 ns/op (9.3M ops/s) | 18 ns/op (55M ops/s) |
| 1 to 1024 bytes | 120 ns/op (8.3M ops/s) | 30 ns/op (34M ops/s) |
| 1 to 4096 bytes | 166 ns/op (6.0M ops/s) | 77 ns/op (13M ops/s) |

It is 2 to 6 times slower than glibc, and the gap is the design: first fit is a linear
walk of every block, where glibc goes straight to a size-segregated bin, and one global
mutex is taken on every call. The benchmark did pay for itself once already. The original
`my_free` re-scanned the entire block list on every call to find mergeable neighbours;
merging only with the adjacent blocks took the three rows above from 161, 178 and
223 ns/op to 107, 120 and 166.

The demo page runs the same workload inside WebAssembly against Emscripten's malloc.

## Web demo

`web/build.sh` compiles `src/my_allocator.c` unmodified with Emscripten, plus
`web/alloc_web.c` for the in-browser benchmark. GitHub Actions runs `make test`, builds
the demo, and publishes it to Pages on every push to `main`.

```bash
web/build.sh                          # needs emcc on PATH
python3 -m http.server -d web/dist    # then open http://localhost:8000
```

## Known limits

These are deliberate; the point was the mechanism, not a production allocator.

* The arena is a fixed 1 MB and never grows. Once it is full, `my_malloc` returns `NULL`.
* Memory is never returned to the operating system; the mapping lives for the life of
  the process.
* First fit is O(blocks) per allocation, and `my_free` walks the list up to the block
  being freed to find its predecessor. A size-class free list, or a boundary tag in each
  block footer, would make both constant time.
* A double free is only detected while the block has not been handed out again.

## License

MIT. See [LICENSE](LICENSE).

## Author

**Abir Deol** · [abirdeol.tech](https://abirdeol.tech)
