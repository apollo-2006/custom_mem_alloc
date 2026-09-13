// Correctness checks for the allocator. `make test`.
//
// The misuse cases (double free, foreign pointer, interior pointer) must abort,
// so each runs in a forked child and the parent checks it died with SIGABRT.
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../include/my_allocator.h"
#include "../bench/workload.h"

#define ARENA (1024 * 1024)
static int failures = 0;

static void check(int ok, const char* name) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    if (!ok) failures++;
}

static void* worker(void* arg) {
    WorkloadResult r = run_workload(my_malloc, my_free, 200000, 700, (uint32_t)(uintptr_t)arg);
    return (void*)(uintptr_t)(r.corrupt || r.failed);
}

static int aborts(void (*fn)(void)) {
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stderr);
        fn();
        _exit(0);
    }
    int status;
    waitpid(pid, &status, 0);
    return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

static void double_free(void) { char* p = my_malloc(16); my_malloc(16); my_free(p); my_free(p); }
static void foreign_free(void) { int x; my_free(&x); }
static void interior_free(void) { char* p = my_malloc(64); my_malloc(8); my_free(p + 16); }

int main(void) {
    check(my_malloc(0) == NULL, "malloc(0) returns NULL");
    check(my_malloc(SIZE_MAX) == NULL, "malloc(SIZE_MAX) returns NULL, no wraparound");
    check(my_malloc(SIZE_MAX - 3) == NULL, "malloc(SIZE_MAX - 3) returns NULL");
    check(my_malloc(2 * ARENA) == NULL, "request larger than the arena returns NULL");
    check(my_calloc(SIZE_MAX / 2, 4) == NULL, "calloc overflow returns NULL");

    int* z = my_calloc(100, sizeof(int));
    int zero = z != NULL;
    for (int i = 0; zero && i < 100; i++) zero = z[i] == 0;
    check(zero, "calloc memory is zeroed");
    my_free(z);

    int aligned = 1;
    void* ptrs[64];
    for (int i = 0; i < 64; i++) { ptrs[i] = my_malloc(1 + i * 3); aligned &= ((uintptr_t)ptrs[i] % 8) == 0; }
    check(aligned, "every pointer is 8-byte aligned");
    for (int i = 0; i < 64; i += 2) my_free(ptrs[i]);
    for (int i = 1; i < 64; i += 2) my_free(ptrs[i]);

    pthread_t t[8];
    for (int i = 0; i < 8; i++) pthread_create(&t[i], NULL, worker, (void*)(uintptr_t)(i + 1));
    int thread_ok = 1;
    for (int i = 0; i < 8; i++) { void* bad; pthread_join(t[i], &bad); thread_ok &= bad == NULL; }
    check(thread_ok, "8 threads x 200k ops, no corruption and no failed allocations");

    void* whole = my_malloc(ARENA - 24);
    check(whole != NULL, "after everything is freed the arena coalesces back into one block");
    my_free(whole);

    check(aborts(double_free), "double free aborts");
    check(aborts(foreign_free), "freeing a pointer from outside the arena aborts");
    check(aborts(interior_free), "freeing a pointer into the middle of a block aborts");

    printf("%s\n", failures ? "FAILED" : "all passed");
    return failures != 0;
}
