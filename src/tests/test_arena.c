#include "test_arena.h"
#include "test_harness.h"
#include "core/arena.h"
#include "core/memory.h"
#include <sys/mman.h>

static void *platform__arena_alloc(usize size, void *user) {
    (void)user;
    void *mem = mmap(0, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(mem == MAP_FAILED) {
        return NULL;
    }
    if(mprotect(mem, size, PROT_READ | PROT_WRITE) == -1) {
        munmap(mem, size);
        return NULL;
    }
    return mem;
}

static void platform__arena_free(void *ptr, usize size, void *user) {
    (void)user;
    munmap(ptr, size);
}

void test_arena_run_all(void) {
    printf("Arena:\n");
}