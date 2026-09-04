# custom_mem_alloc

A thread-safe memory allocator written from scratch in C. It requests one large
contiguous region of virtual memory from the kernel up front via `mmap`, then hands
slices of that region to the program itself, so no call after the first one goes back
to the operating system.

Written to find out what `malloc` is actually doing underneath.

## How it works

* **One arena, mapped once.** A single 1 MB `mmap` region is created lazily on the
  first allocation. Every later request is served out of that region.
* **Intrusive block headers.** Each block carries a `BlockHeader` immediately before
  its payload, holding the payload size, a free flag, and a pointer to the next block.
  The list is threaded through the arena itself rather than stored beside it.
* **First-fit with splitting.** `my_malloc` walks the block list for the first free
  block large enough, then splits off the remainder as a new free block when the
  leftover can hold a header plus at least 8 bytes of payload.
* **Coalescing on free.** `my_free` marks the block free and then merges neighbouring
  free blocks, which keeps the arena from fragmenting into unusable slivers.
* **8-byte alignment.** Every payload is rounded up to an 8-byte boundary so returned
  pointers are suitably aligned for any primitive type.
* **Thread safety.** A single `pthread_mutex` guards the arena, so the allocator can be
  called from multiple threads without racing on the block list.

## API

```c
void* my_malloc(size_t size);
void  my_free(void* ptr);
void* my_calloc(size_t num, size_t size);
void  print_heap_metadata(void);   // dumps every block: address, size, free/allocated
```

## Build & run

```bash
git clone https://github.com/apollo-2006/custom_mem_alloc.git
cd custom_mem_alloc

gcc -Wall -Wextra -O2 -Iinclude main.c src/my_allocator.c -o custom_allocator -lpthread
./custom_allocator
```

`main.c` allocates an array and a string, prints the arena layout, frees both, and
prints the layout again so you can watch the two blocks merge back together.

## Known limits

These are deliberate; the point was the mechanism, not a production allocator.

* The arena is a fixed 1 MB and never grows. Once it is full, `my_malloc` returns `NULL`.
* Memory is never returned to the operating system; the mapping lives for the life of
  the process.
* Coalescing runs a full pass over the block list on every free, so `my_free` is O(n).
* Blocks are found by first fit, which is simple but fragments more than a best-fit or
  size-segregated scheme would.

## Author

**Abir Deol**
