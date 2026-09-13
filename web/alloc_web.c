// Browser bindings. my_malloc, my_free, my_calloc and print_heap_metadata are
// exported straight from src/my_allocator.c; this file only adds the benchmark,
// which has to run inside WebAssembly so JS call overhead is not what gets timed.
#include <stdlib.h>
#include <emscripten/emscripten.h>
#include "../include/my_allocator.h"
#include "../bench/workload.h"

static double last_ns_per_op;
static long last_failed;
static int last_corrupt;

EMSCRIPTEN_KEEPALIVE double bench_run(int use_mine, int ops, int max_size) {
    double t0 = emscripten_get_now();
    WorkloadResult r = use_mine ? run_workload(my_malloc, my_free, ops, (size_t)max_size, 42)
                                : run_workload(malloc, free, ops, (size_t)max_size, 42);
    double ms = emscripten_get_now() - t0;
    last_ns_per_op = ms * 1e6 / ops;
    last_failed = r.failed;
    last_corrupt = r.corrupt;
    return last_ns_per_op;
}

EMSCRIPTEN_KEEPALIVE int bench_failed(void) { return (int)last_failed; }
EMSCRIPTEN_KEEPALIVE int bench_corrupt(void) { return last_corrupt; }
