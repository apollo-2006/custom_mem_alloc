#ifndef MY_ALLOCATOR_H
#define MY_ALLOCATOR_H

#include <stddef.h>
#include <stdbool.h>

// Represents the hidden metadata tracking layer at the start of each memory block
typedef struct BlockHeader {
    size_t size;               // Size of the usable payload block
    bool is_free;              // Availability flag
    struct BlockHeader* next;  // Intrusive linked-list pointer to the next block
} BlockHeader;

#define HEADER_SIZE sizeof(BlockHeader)
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

// Primary memory interface functions
void* my_malloc(size_t size);
void  my_free(void* ptr);
void* my_calloc(size_t num, size_t size);

// Debugging utility: dumps every block in the arena with its size and state
void  print_heap_metadata(void);

#endif // MY_ALLOCATOR_H