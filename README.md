# custom_mem_alloc
the allocator requests a large contiguous chunk of virtual memory from the operating system up front using mmap, slices this memory down for the user program with minimal overhead, incorporates free-list , aligns alloc to 8-byte boundaries, and has thread-safe mutex lock to prevent race conditions in multithreaded envs
