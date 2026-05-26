#include "include/my_allocator.h"
#include <stdio.h>

int main(void) {
    printf("Booting Custom Allocator Verification Routine...\n");

    // Allocation pass
    int* array = (int*)my_malloc(5 * sizeof(int));
    char* string = (char*)my_malloc(24 * sizeof(char));

    if (array && string) {
        for (int i = 0; i < 5; i++) {
            array[i] = i * 10;
        }
        snprintf(string, 24, "Kernel Architecture");

        printf("Allocated Array Values: %d, %d, %d\n", array[0], array[1], array[2]);
        printf("Allocated String Value: %s\n", string);
    }

    print_heap_metadata();

    // Free items to verify heap structure compaction and merging
    printf("\nReleasing allocated blocks back to the virtual layer...\n");
    my_free(array);
    my_free(string);

    print_heap_metadata();

    return 0;
}