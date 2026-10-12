#include "memory.h"
#include <sys/mman.h>
#include <assert.h>

static void *platform_arena_alloc(usize size, void *user);
static void platform_arena_free(void *ptr, usize size, void *user);

int main(void) {
    Memory memory;
    memory_init(&memory, platform_arena_alloc, platform_arena_free, 
        NULL, 64 * 1024 * 1024, 10 * 1024 * 1024);
    
    
    
    return 0;
}

static void *platform_arena_alloc(usize size, void *user) {
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

static void platform_arena_free(void *ptr, usize size, void *user) {
    (void)user;
    munmap(ptr, size);
}