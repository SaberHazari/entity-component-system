#define ARENA_IMPLEMENTATION
#include "arena.h"
#include <assert.h>

static inline usize arena__align_up(usize n, usize align) {
    return (n + (align - 1)) & ~(align - 1);
}

static inline usize arena__block_header_size(void) {
    return arena__align_up(sizeof(ArenaBlock), ARENA_MIN_ALIGN);
}

static inline u8 *arena__block_data(ArenaBlock *block) {
    return (u8 *)block + arena__block_header_size();
}

static inline ArenaBlock *arena__new_block(Arena *arena, usize min_data_size) {
    usize data_size = arena->minimum_block_size;
    if(data_size < min_data_size) { data_size = min_data_size; }
    data_size = arena__align_up(data_size, ARENA_MIN_ALIGN);
    
    usize total = arena__block_header_size() + data_size;
    u8 *raw = (u8 *)arena->alloc(total, arena->alloc_user);
    if(!raw) {
        arena__fail(arena, data_size, ARENA_MIN_ALIGN);
        return NULL;
    }
    assert(((uptr)raw & (ARENA_MIN_ALIGN - 1)) == 0 && 
    "allocator must return ARENA_MIN_ALIGN-aligned memory");
    
    ArenaBlock *block = (ArenaBlock *)raw;
    block->prev = arena->current_block;
    block->next = NULL;
    block->size = data_size;
    block->used = 0;
    
    if(arena->current_block) { arena->current_block->next = block; }
    else { arena->first_block = block; }
    
    arena->current_block = block;
    arena->total_size += data_size;
    
    return block;
}

void arena_init(Arena *arena, const char *name, usize minimum_block_size, 
    ArenaAllocFn alloc, ArenaFreeFn free, void *user) {
    assert(arena && alloc && free && minimum_block_size > 0u);
    assert((minimum_block_size & (ARENA_MIN_ALIGN - 1u)) == 0u && 
        "minimum block size must be ARENA_MIN_ALIGN-aligned");
    
    arena->name = name;
    arena->first_block = NULL;
    arena->current_block = NULL;
    arena->minimum_block_size = minimum_block_size;
    arena->total_size = 0;
    arena->total_used = 0;
    arena->peak = 0;
    arena->alloc = alloc;
    arena->free = free;
    arena->alloc_user = user;
}

void *arena_push(Arena *arena, usize size, usize align) {
    assert(arena);
    assert(align && (align & (align - 1)) == 0 && 
        "align must be a power of two");
    if(size == 0) { return NULL; }
    
    ArenaBlock *block = arena->current_block;
    if(block) {
        uptr base = (uptr)arena__block_data(block);
        uptr addr = base + block->used;
        uptr aligned = (addr + (align - 1)) & ~(uptr)(align - 1);
        usize off = (usize)(aligned - base);
        
        if(off + size <= block->size) {
            void *result = (u8 *)aligned;
            usize new_used = off + size;
            arena->total_used += new_used - block->used;
            if(arena->total_used > arena->peak) { arena->peak = arena->total_used; }
            block->used = new_used;
            arena_unpoison_m(result, size);
            return result;
        }
    }
    
    ArenaBlock *fresh = arena__new_block(arena, size + align);
    if(!fresh) { return NULL; }
    
    uptr base = (uptr)arena__block_data(fresh);
    uptr aligned = (base + (align - 1)) & ~(uptr)(align - 1);
    usize off = (usize)(aligned - base);
    
    void *result = (u8 *)aligned;
    usize new_used = off + size;
    arena->total_used += new_used - fresh->used;
    if(arena->total_used > arena->peak) { arena->peak = arena->total_used; }
    fresh->used = new_used;
    arena_unpoison_m(result, size);
    return result;
}

void arena_pop_to(Arena *arena, ArenaMark mark) {
    assert(arena);
    
    ArenaBlock *block = arena->current_block;
    while(block != mark.block) {
        assert(block && "mark doesn't belong to this arena");
        ArenaBlock *prev = block->prev;
        usize total = arena__block_header_size() + block->size;
        arena_unpoison_m(arena__block_data(block), block->size);
        arena->free(block, total, arena->alloc_user);
        arena->total_size -= block->size;
        arena->total_used -= block->used;
        block = prev;
    }
    
    arena->current_block = mark.block;
    
    if(arena->current_block) {
        arena->current_block->next = NULL;
        usize old_used = arena->current_block->used;
        assert(mark.used <= old_used && "mark is newer than current usage");
        if(mark.used < old_used) {
            arena_poison_m(arena__block_data(arena->current_block) + mark.used, old_used - mark.used);
            arena->total_used -= (old_used - mark.used);
        }
        arena->current_block->used = mark.used;
    } else {
        arena->first_block = NULL;
    }
}

void arena_clear(Arena *arena) {
    assert(arena);
    if(!arena->first_block) { return; }
    
    ArenaBlock *first = arena->first_block;
    ArenaBlock *block = arena->current_block;
    while(block != first) {
        ArenaBlock *prev = block->prev;
        usize total = arena__block_header_size() + block->size;
        arena_unpoison_m(arena__block_data(block), block->size);
        arena->free(block, total, arena->alloc_user);
        arena->total_size -= block->size;
        block = prev;
    }
    
    first->next = NULL;
    if(first->used) {
        arena_poison_m(arena__block_data(first), first->used);
    }
    first->used = 0;
    
    arena->current_block = first;
    arena->total_used = 0;
}

void arena_destroy(Arena *arena) {
    assert(arena);
    ArenaBlock *block = arena->first_block;
    while(block) {
        ArenaBlock *next = block->next;
        usize total = arena__block_header_size() + block->size;
        arena_unpoison_m(arena__block_data(block), block->size);
        arena->free(block, total, arena->alloc_user);
        block = next;
    }
    
    arena->first_block = NULL;
    arena->current_block = NULL;
    arena->total_size = 0;
    arena->total_used = 0;
}