/* rng.h - the original 24-bit polynomial RNG (RANDOM.ASM).
 * Byte-exact with the 6809: dungeons generate identically to 1982. */
#ifndef DOD_RNG_H
#define DOD_RNG_H

#include "dod_types.h"

typedef struct {
    dodBYTE SEED[3];
    dodBYTE carry;
} rng_state;

extern rng_state rng;

void    rng_reset(void);
void    rng_set_seed(dodBYTE s0, dodBYTE s1, dodBYTE s2);
dodBYTE rng_RANDOM(void);

#endif /* DOD_RNG_H */
