// my_malloc/my_free against the platform allocator on the same random workload.
//
//   make bench
//
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../include/my_allocator.h"
#include "workload.h"

static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

static void report(const char* name, void* (*a)(size_t), void (*f)(void*), long ops, size_t max) {
    double best = 1e300;
    WorkloadResult r = {0};
    for (int run = 0; run < 5; run++) {
        double t0 = now_ns();
        r = run_workload(a, f, ops, max, 42);
        double dt = now_ns() - t0;
        if (dt < best) best = dt;
    }
    printf("  %-8s %8.1f ns/op  %6.2f M ops/s  allocs %ld  failed %ld%s\n",
           name, best / ops, ops / best * 1e3, r.allocs, r.failed, r.corrupt ? "  CORRUPT" : "");
}

int main(void) {
    const long ops = 2000000;
    printf("%ld ops, 256 live slots, best of 5\n", ops);
    for (size_t max = 256; max <= 4096; max *= 4) {
        printf("sizes 1..%zu bytes (3 in 4 under 64):\n", max);
        report("my_malloc", my_malloc, my_free, ops, max);
        report("malloc", malloc, free, ops, max);
    }
    return 0;
}
