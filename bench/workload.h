// A random allocate/free workload, shared by the native benchmark and the web
// demo so both measure the same thing. Header-only on purpose: each program
// includes it with its own ALLOC/FREE pair.
#ifndef WORKLOAD_H
#define WORKLOAD_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define WORKLOAD_SLOTS 256

typedef struct {
    long allocs;
    long frees;
    long failed;     // allocation returned NULL (arena full or too fragmented)
    long checksum;   // bytes written and verified, so a broken allocator cannot pass
    int corrupt;     // set if a block did not hold the pattern written into it
} WorkloadResult;

static uint32_t workload_rand(uint32_t* s) {
    *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5;
    return *s;
}

// Each op either frees a live slot or allocates into an empty one. Sizes are
// 1..max_size bytes, skewed small the way real programs are. Every block is
// filled with a byte derived from its slot and checked before it is freed.
static WorkloadResult run_workload(void* (*alloc)(size_t), void (*release)(void*),
                                   long ops, size_t max_size, uint32_t seed) {
    void* ptr[WORKLOAD_SLOTS] = {0};
    size_t len[WORKLOAD_SLOTS] = {0};
    WorkloadResult r = {0};
    uint32_t s = seed ? seed : 1;

    for (long i = 0; i < ops; i++) {
        int slot = (int)(workload_rand(&s) % WORKLOAD_SLOTS);
        if (ptr[slot]) {
            unsigned char* p = (unsigned char*)ptr[slot];
            for (size_t k = 0; k < len[slot]; k++) {
                if (p[k] != (unsigned char)slot) { r.corrupt = 1; break; }
            }
            r.checksum += (long)len[slot];
            release(ptr[slot]);
            ptr[slot] = NULL;
            r.frees++;
        } else {
            uint32_t roll = workload_rand(&s);
            size_t size = (roll & 3) ? 1 + roll % 64 : 1 + roll % max_size;
            void* p = alloc(size);
            if (!p) { r.failed++; continue; }
            memset(p, slot, size);
            ptr[slot] = p;
            len[slot] = size;
            r.allocs++;
        }
    }
    for (int i = 0; i < WORKLOAD_SLOTS; i++) {
        if (ptr[i]) { release(ptr[i]); r.frees++; }
    }
    return r;
}

#endif
