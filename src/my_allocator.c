#include "../include/my_allocator.h"
#include <sys/mman.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define ARENA_SIZE (1024 * 1024) // Pre-allocate a 1MB virtual memory pool

static void* global_arena_start = NULL;
static BlockHeader* free_list_head = NULL;
static pthread_mutex_t allocator_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initializes our virtual memory arena safely from the kernel via mmap
static bool init_arena(void) {
    global_arena_start = mmap(NULL, ARENA_SIZE, PROT_READ | PROT_WRITE,
                              MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (global_arena_start == MAP_FAILED) {
        return false;
    }

    // Set up the initial master free block covering the entire arena space
    free_list_head = (BlockHeader*)global_arena_start;
    free_list_head->size = ARENA_SIZE - HEADER_SIZE;
    free_list_head->is_free = true;
    free_list_head->next = NULL;

    return true;
}

// Scans the intrusive linked list for an available free block
static BlockHeader* find_free_block(size_t size) {
    BlockHeader* current = free_list_head;
    while (current) {
        if (current->is_free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Splits a large block into the requested size and a remaining smaller free block
static void split_block(BlockHeader* block, size_t size) {
    // Only split if the remainder can hold a header and at least 8 bytes of payload
    if (block->size >= size + HEADER_SIZE + ALIGNMENT) {
        BlockHeader* new_block = (BlockHeader*)((char*)block + HEADER_SIZE + size);
        new_block->size = block->size - size - HEADER_SIZE;
        new_block->is_free = true;
        new_block->next = block->next;

        block->size = size;
        block->next = new_block;
    }
}

void* my_malloc(size_t size) {
    if (size == 0) return NULL;

    size_t aligned_size = ALIGN(size);

    pthread_mutex_lock(&allocator_mutex);

    // Lazy initialization of the memory arena on first run
    if (!global_arena_start && !init_arena()) {
        pthread_mutex_unlock(&allocator_mutex);
        return NULL;
    }

    BlockHeader* block = find_free_block(aligned_size);

    if (block) {
        split_block(block, aligned_size);
        block->is_free = false;
        pthread_mutex_unlock(&allocator_mutex);
        return (void*)(block + 1); // Return pointer shifts forward past metadata header
    }

    pthread_mutex_unlock(&allocator_mutex);
    return NULL; // Out of memory pool space
}

void my_free(void* ptr) {
    if (!ptr) return;

    pthread_mutex_lock(&allocator_mutex);

    // Shift pointer backward to locate the corresponding metadata block
    BlockHeader* block = (BlockHeader*)ptr - 1;
    block->is_free = true;

    // Coalescing phase: Merge contiguous free blocks to prevent fragmentation
    BlockHeader* current = free_list_head;
    while (current && current->next) {
        if (current->is_free && current->next->is_free) {
            current->size += HEADER_SIZE + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }

    pthread_mutex_unlock(&allocator_mutex);
}

void* my_calloc(size_t num, size_t size) {
    // Reject requests whose size would wrap around, which would otherwise hand
    // back a block far smaller than the caller asked for
    if (num != 0 && size > SIZE_MAX / num) return NULL;

    size_t total = num * size;
    void* ptr = my_malloc(total);
    if (ptr) {
        memset(ptr, 0, total); // Securely zero out memory payload
    }
    return ptr;
}

void print_heap_metadata(void) {
    pthread_mutex_lock(&allocator_mutex);
    BlockHeader* current = free_list_head;
    printf("\n=== Current Memory Pool State ===\n");
    while (current) {
        printf("Block Address: %p | Size: %zu Bytes | Status: %s\n",
               (void*)current, current->size, current->is_free ? "FREE" : "ALLOCATED");
        current = current->next;
    }
    printf("=================================\n");
    pthread_mutex_unlock(&allocator_mutex);
}