#include "ecs.h"
#include <assert.h>

static u32 archetype__find_or_create(World *world, Mask sig) {
    for(u32 i = 0; i < world.archetype_count; ++i) {
        if(mask_equal(&world->archetypes[i].sig, &sig)) {
            return i;
        }
    }
    
    if(world->archetype_count == world->archetype_capacity) {
        u32 cap = world->archetype_capacity ? 
            world->archetype_capacity * 2 : 16;
        Archetype *archs = arena_array_m(world->arena, Archetype, cap);
        if(!archs) { return UINT32_MAX; }
        if(world->archetype_count) {
            memcpy(archs, world->archetypes, 
                world->archetype_count * sizeof(Archetype));
        }
        world->archetypes = archs;
        world->archetype_capacity = cap;
    }
    
    Archetype *new_arch = &world->archetypes[world->archetype_count];
    memset(new_arch, 0, sizeof(*new_arch));
    new_arch->sig = sig;
    return world->archetype_count++;
}

static u32 archetype__append(World *world, Archetype *arch, u32 ent_id) {
    if(arch->count == arch->capacity) {
        u32 cap = arch->capacity ? arch->capacity * 2 : 16;
        for(u32 comp_id = 0; comp_id < MAX_COMPONENTS; ++comp_id) {
            if(!mask_has(&arch->sig, comp_id)) { continue; }
            usize size = world->component_sizes[comp_id];
            void *comps = arena_push(world->arena, cap * size, ARENA_MIN_ALIGN);
            if(arch->count) {
                memcpy(comps, arch->columns[comp_id], arch->count * size);
            }
            arch->columns[comp_id] = comps;
        }
        u32 *ents = arena_array_m(world->arena, u32, cap);
        if(arch->count) {
            memcpy(ents, arch->entities, arch->count * sizeof(u32));
        }
        arch->entities = ents;
        arch->capacity = cap;
    }
    
    u32 row = arch->count++;
    for(u32 comp_id = 0; comp_id < MAX_COMPONENTS; ++comp_id) {
        if(!mask_has(&arch->sig, comp_id)) { continue; }
        memset((u8 *)arch->columns[comp_id] + row * world->component_sizes[comp_id], 
            0, world->component_sizes[comp_id]);
    }
    arch->entities[row] = ent_id;
    return row;
}

static void archetype__swap_remove(World *world, Archetype *arch, u32 row) {
    u32 last = arch->count - 1;
    if(row != last) {
        for(u32 comp_id = 0; comp_id < MAX_COMPONENTS; ++comp_id) {
            if(!mask_has(&arch->sig, comp_id)) { continue; }
            usize size = world->component_sizes[comp_id];
            memcpy((u8 *)arch->columns[comp_id] + row * size, 
            (u8 *)arch->columns[comp_id] + last * size, size);
        }
        u32 moved = arch->entities[last];
        arch->entities[row] = moved;
        world->locations[moved].row = row;
    }
    arch->count--;
}

static bool world__grow_entities(World *world) {
    u32 cap = world->capacity ? world->capacity * 2 : 64;
    
    u32 *gens = arena_array_m(world->arena, u32, cap);
    u32 *frees = arena_array_m(world->arena, u32, cap);
    u8 *alives = arena_array_m(world->arena, u8, cap);
    EntityLocation *locs = arena_array_m(world->arena, EntityLocation, cap);
    if(!gens || !frees || !alives || !locs) {
        return false;
    }
    
    if(world->capacity) {
        memcpy(gens, world->generations, world->capacity * sizeof(u32));
        memcpy(alives, world->alive, world->capacity * sizeof(u8));
        memcpy(locs, world->locations, world->capacity * sizeof(EntityLocation));
        memcpy(frees, world->free_list, world->free_count * sizeof(u32));
    }
    
    for(u32 i = world->capacity; i < cap; ++i) {
        gens[i] = 1;
        alives[i] = 0;
        frees[world->free_count++] = i;
    }
    
    world->generations = gens;
    world->alive = alives;
    world->locations = locs;
    world->free_list = frees;
    world->capacity = cap;
    
    return true;
}

bool world_init(World *world, Arena *arena) {
    memset(world, 0, sizeof(*world));
    world->arena = arena;
    if(!world__grow_entities(world)) { return false; }
    
    Mask empty = {{0}};
    u32 id = archetype__find_or_create(world, empty);
    if(id == UINT32_MAX) { return false; }
    world->empty_archetype = id;
    return true;
}

u32 component_register(World *world, usize elem_size) {
    assert(elem_size > 0);
    for(u32 comp_id = 0; comp_id < MAX_COMPONENTS; ++comp_id) {
        if(!world->component_registered[comp_id]) {
            world->component_registered[comp_id] = true;
            world->component_sizes[comp_id] = elem_size;
            return comp_id;
        }
    }
    return UINT32_MAX;
}

bool entity_alive(const World *world, Entity ent) {
    return ent->index < world->capacity && 
        world->alive[ent.index] && 
        world->generations[ent.index] == ent.generation;
}

Entity entity_create(World *world) {
    if(world->free_count == 0 && !world__grow_entities(world)) {
        return (Entity){0};
    }
    u32 id = world->free_list[--world->free_count];
    world->alive[id] = 1;
    
    Archetype *empty = &world->archetypes[world->empty_archetype];
    u32 row = archetype__append(world, empty, id);
    world->locations[id] = (EntityLocation){ world->empty_archetype, row };
    return (Entity){ id, world->generations[id] };
}

void entity_destroy(World *world, Entity ent) {
    if(!entity_alive(world, ent)) { return; }
    EntityLocation loc = world->locations[ent.index];
    archetype__swap_remove(world, &world->archetypes[loc.archetype], loc.row);
    world->alive[ent.index] = 0;
    world->generations[ent.index]++;
    world->free_list[world->free_count++] = ent.index;
}

void *component_get(World *world, Entity ent, u32 comp_id) {
    if(!entity_alive(world, ent)) { return NULL; }
    EntityLocation loc = world->locations[ent.index];
    Archetype *arch = &world->archetypes[loc.archetype];
    if(!mask_has(&arch->sig, comp_id)) { return NULL; }
    return (u8 *)arch->columns[comp_id] + 
        loc.row * world->component_sizes[comp_id];
}

void *component_add(World *world, Entity ent, u32 comp_id) {
    if(!entity_alive(world, ent)) { return NULL; }

    EntityLocation *loc = &world->locations[ent.index];
    u32 old_id = loc->archetype;
    u32 old_row = loc->row;

    if(mask_has(&world->archetypes[old_id].sig, comp_id)) {
        return (u8 *)world->archetypes[old_id].columns[comp_id]
             + old_row * world->component_sizes[comp_id];
    }
    Mask new_sig = world->archetypes[old_id].sig;
    mask_set(&new_sig, comp_id);

    u32 new_id = archetype__find_or_create(world, new_sig);
    if(new_id == UINT32_MAX) { return NULL; }

    Archetype *old_arch = &world->archetypes[old_id];
    Archetype *new_arch = &world->archetypes[new_id];

    u32 new_row = archetype__append(world, new_arch, ent.index);

    for(u32 i = 0; i < MAX_COMPONENTS; ++i) {
        if(i == comp_id) { continue; }
        if(!mask_has(&old_arch->sig, i)) { continue; }
        usize size = world->component_sizes[i];
        memcpy((u8 *)new_arch->columns[i] + new_row * size,
               (u8 *)old_arch->columns[i] + old_row * size, size);
    }

    archetype__swap_remove(world, old_arch, old_row);
    loc->archetype = new_id;
    loc->row = new_row;

    return (u8 *)new_arch->columns[comp_id] + 
        new_row * world->component_sizes[comp_id];
}

void component_remove(World *world, Entity ent, u32 comp_id) {
    if(!entity_alive(world, ent)) { return; }

    EntityLocation *loc = &world->locations[ent.index];
    u32 old_id = loc->archetype;
    u32 old_row = loc->row;
    if(!mask_has(&world->archetypes[old_id].sig, comp_id)) { return; }

    Mask new_sig = world->archetypes[old_id].sig;
    mask_clear(&new_sig, comp_id);

    u32 new_id = archetype__find_or_create(world, new_sig);
    if(new_id == UINT32_MAX) { return; }

    Archetype *old_arch = &world->archetypes[old_id];
    Archetype *new_arch = &world->archetypes[new_id];

    u32 new_row = archetype__append(world, new_arch, ent.index);

    for(u32 i = 0; i < MAX_COMPONENTS; ++i) {
        if(i == comp_id) { continue; }
        if(!mask_has(&old_arch->sig, i)) { continue; }
        usize size = world->component_sizes[i];
        memcpy((u8 *)new_arch->columns[i] + new_row * size,
               (u8 *)old_arch->columns[i] + old_row * size, size);
    }

    archetype__swap_remove(world, old_arch, old_row);
    loc->archetype = new_id;
    loc->row = new_row;
}

QueryIter world_query(World *world, const u32 *comp_ids, u32 count) {
    QueryIter iter;
    memset(&iter, 0, sizeof(iter));
    iter.world = world;
    iter.component_count = count;
    for(u32 i = 0; i < count; ++i) {
        iter.component_ids[i] = comp_ids[i];
        mask_set(&iter.query, comp_ids[i]);
    }
    return iter;
}

bool query_next(QueryIter *iter, Entity *out_ent, void **out_cols) {
    World *world = iter->world;
    while(iter->arch_index < world->archetype_count) {
        Archetype *arch = &world->archetypes[iter->arch_index];

        if(mask_subset(&iter->query, &arch->sig) && iter->row < arch->count) {
            u32 row = iter->row++;
            u32 ent_id = arch->entities[row];
            *out_ent = (Entity){ ent_id, world->generations[ent_id] };
            for(u32 i = 0; i < iter->component_count; ++i) {
                u32 comp_id = iter->component_ids[i];
                out_cols[i] = (u8 *)arch->columns[comp_id] + row * world->component_sizes[comp_id];
            }
            return true;
        }
        iter->arch_index++;
        iter->row = 0;
    }
    return false;
}