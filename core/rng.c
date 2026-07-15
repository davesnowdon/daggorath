/* rng.c - RANDOM.ASM: 24-bit shift register, taps SEED[2] & 0xE1.
 * Structure follows the 6809 rotate loop exactly (see the annotated
 * disassembly); this is what makes every dungeon identical to 1982. */
#include "rng.h"

rng_state rng;

void rng_reset(void)
{
    rng.SEED[0] = 0;
    rng.SEED[1] = 0;
    rng.SEED[2] = 0;
    rng.carry = 0;
}

void rng_set_seed(dodBYTE s0, dodBYTE s1, dodBYTE s2)
{
    rng.SEED[0] = s0;
    rng.SEED[1] = s1;
    rng.SEED[2] = s2;
}

/* A backend may provide rng_RANDOM in assembly by defining DOD_RNG_ASM
 * (the Spectrum Next does: spectrum-next/rng_z80.asm - the per-bit
 * helper calls below cost ~90 calls per random byte under zsdcc, which
 * made level generation take ~30 wall seconds).  This C version stays
 * the normative reference: any replacement must be proven byte-exact
 * against it, including the final rng.carry (it is part of the dumped/
 * saved state) - see tests/z80rng/run.sh. */
#ifndef DOD_RNG_ASM

static dodBYTE lsl(dodBYTE c)
{
    rng.carry = (dodBYTE)((c & 0x80u) ? 1 : 0);
    return (dodBYTE)(c << 1);
}

static dodBYTE lsr(dodBYTE c)
{
    rng.carry = (dodBYTE)(c & 0x01u);
    return (dodBYTE)(c >> 1);
}

static dodBYTE rol(dodBYTE c)
{
    dodBYTE cry = (dodBYTE)((c & 0x80u) ? 1 : 0);
    c = (dodBYTE)((dodBYTE)(c << 1) + rng.carry);
    rng.carry = cry;
    return c;
}

dodBYTE rng_RANDOM(void)
{
    uint8_t x, y;
    dodBYTE a, b;
    rng.carry = 0;
    for (x = 8; x != 0; --x) {
        b = 0;
        a = (dodBYTE)(rng.SEED[2] & 0xE1u);
        for (y = 8; y != 0; --y) {
            a = lsl(a);
            if (rng.carry != 0) {
                ++b;
            }
        }
        b = lsr(b);
        rng.SEED[0] = rol(rng.SEED[0]);
        rng.SEED[1] = rol(rng.SEED[1]);
        rng.SEED[2] = rol(rng.SEED[2]);
    }
    return rng.SEED[0];
}

#endif /* DOD_RNG_ASM */
