#ifndef ECS_H
#define ECS_H

#include "arena.h"

#define MAX_COMPONENTS 512
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



#endif