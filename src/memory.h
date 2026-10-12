#ifndef MEMORY_H
#define MEMORY_H

#include "arena.h"

typedef struct Memory {
    Arena permanent;
    Arena transient;
} Memory;

typedef struct TempMemory {
    Arena *arena;
    ArenaMark mark;
} TempMemory;

static inline void memory_init(Memory *memory, ArenaAllocFn alloc, ArenaFreeFn free, 
    void *user, usize permanent_min_block_size, usize transient_min_block_size) {
    arena_init(&memory->permanent, "Permanent", permanent_min_block_size, alloc, free, user);
    arena_init(&memory->transient, "Transient", transient_min_block_size, alloc, free, user);
}

static inline void memory_init_default(Memory *memory, usize permanent_min_block_size, 
    usize transient_min_block_size) {
    memory_init(memory, arena_default_alloc, arena_default_free, NULL, 
        permanent_min_block_size, transient_min_block_size);
}

static inline void memory_frame_reset(Memory *memory) {
    arena_clear(&memory->transient);
}

static inline void memory_destroy(Memory *memory) {
    arena_destroy(&memory->permanent);
    arena_destroy(&memory->transient);
}

static inline TempMemory temp_memory_begin(Arena *arena) {
    return (TempMemory){ arena, arena_mark(arena) };
}

static inline void temp_memory_end(TempMemory temp) {
    arena_pop_to(temp.arena, temp.mark);
}

#endif