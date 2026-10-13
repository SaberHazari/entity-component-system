#ifndef ECS_H
#define ECS_H

#include "arena.h"

#define MAX_COMPONENTS 256
#define SIG_WORDS ((MAX_COMPONENTS + 63) / 64)

typedef struct Mask {
    u64 words[SIG_WORDS];
} Mask;

static inline void mask_set(Mask *mask, u32 comp_id) { 
    mask->words[comp_id >> 6] |= (1ull << (comp_id & 63));
}

static inline void mask_clear(Mask *mask, u32 comp_id) {
    mask->words[comp_id >> 6] &= ~(1ull << (comp_id & 63));
}

#endif