#ifndef ECS_H
#define ECS_H

#include "arena.h"

#define MAX_COMPONENTS 1024
#define SIG_WORDS ((MAX_COMPONENTS + 63) / 64)

#define component_add_m(w, e, T, cid) ((T *)component_add((w), (e), (cid)))
#define component_get_m(w, e, T, cid) ((T *)component_get((w), (e), (cid)))

typedef struct Mask {
    u64 words[SIG_WORDS];
} Mask;

typedef struct Entity {
    u32 index;
    u32 generation;
} Entity;

typedef struct Archetype {
    Mask sig;
    void *columns[MAX_COMPONENTS];
    u32 *entities;
    u32 count;
    u32 capacity;
} Archetype;

typedef struct EntityLocation {
    u32 archetype;
    u32 row;
} EntityLocation;

typedef struct World {
    Arena *arena;
    
    Archetype *archetypes;
    u32 archetype_count;
    u32 archetype_capacity;
    u32 empty_archetype;
    
    usize component_sizes[MAX_COMPONENTS];
    bool component_registered[MAX_COMPONENTS];
    
    EntityLocations *locations;
    u32 *generations;
    u8 *alive;
    u32 *free_list;
    u32 free_count;
    u32 capacity;
} World;

typedef struct QueryIter {
    World *world;
    Mask query;
    u32 component_ids[MAX_COMPONENTS];
    u32 component_count;
    u32 archetype_index;
    u32 row;
} QueryIter;

static inline void mask_set(Mask *mask, u32 comp_id) { 
    mask->words[comp_id >> 6] |= (1ull << (comp_id & 63));
}

static inline void mask_clear(Mask *mask, u32 comp_id) {
    mask->words[comp_id >> 6] &= ~(1ull << (comp_id & 63));
}

static inline bool mask_has(const Mask *mask, u32 comp_id) {
    return (mask->words[comp_id >> 6] >> (comp_id & 63)) & 1ull;
}

static inline bool mask_subset(const Mask *super, const Mask *sub) {
    for(u32 i = 0; i < SIG_WORDS; ++i) {
        if((super->words[i] & sub->words[i]) != super->words[i]) {
            return false;
        }
    }
    return true;
}

static inline bool mask_equal(const Mask *x, const Mask *y) {
    for(u32 i = 0; i < SIG_WORDS; ++i) {
        if(x->words[i] != y->words[i]) {
            return false;
        }
    }
    return true;
}

bool world_init(World *world, Arena *arena);
u32 component_register(World *world, usize elem_size);
bool entity_alive(const World *world, Entity ent);
Entity entity_create(World *world);
void entity_destroy(World *world, Entity ent);
void *component_get(World *world, Entity ent, u32 comp_id);
void *component_add(World *world, Entity ent, u32 comp_id);
void component_remove(World *world, Entity ent, u32 comp_id);
QueryIter world_query(World *world, const u32 *comp_ids, u32 count);
bool query_next(QueryIter *iter, Entity *out_ent, void **out_cols);

#endif