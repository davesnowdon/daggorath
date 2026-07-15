/* wrapper.c - z80-side test wrapper for the asm rng_RANDOM, compiled
 * with zsdcc for the z88dk "+test" target (org 0x8000, run under
 * z88dk-ticks -mz80n).
 *
 * Links the REAL core/rng.c compiled with -DDOD_RNG_ASM (so rng state
 * and rng_set_seed come from the C module, exactly as in the shipping
 * build) plus the REAL spectrum-next/rng_z80.asm providing rng_RANDOM.
 * For each of 20 seed triples (deterministic LFSR-derived, plus edge
 * cases including the degenerate all-zero seed) it records 256
 * consecutive rng_RANDOM return values followed by the four state
 * bytes (SEED[0..2] and carry - the carry byte is part of saved game
 * state and must match too).  host_check.c replays the identical
 * corpus through the normative C rng_RANDOM with gcc and compares the
 * RAM dump byte-for-byte.
 *
 * Exit protocol mirrors tests/z80draw: completion sentinel at
 * 0x9F00/0x9F01, then test_done (done.asm); run.sh points z88dk-ticks
 * -end there and collects the RAM image via -output.
 */
#include <stdint.h>
#include "rng.h"

extern void test_done(void);

#define OUT      ((uint8_t *)0xA000)   /* 20 x 260 = 5200 bytes */
#define SENTINEL ((volatile uint8_t *)0x9F00)
#define NSEEDS   20u
#define NCALLS   256u

static const dodBYTE edge_seeds[4][3] = {
    { 0x00, 0x00, 0x00 },   /* degenerate: LFSR stays stuck  */
    { 0xFF, 0xFF, 0xFF },
    { 0x01, 0x00, 0x00 },
    { 0x00, 0x00, 0xE1 },   /* exactly the tap bits */
};

int main(void)
{
    uint16_t lfsr = 0x1D87u;
    uint8_t s, i;
    uint8_t *p = OUT;

    for (s = 0; s != NSEEDS; ++s) {
        SENTINEL[2] = s;               /* progress, for partial runs */
        if (s < 4u) {
            rng_set_seed(edge_seeds[s][0], edge_seeds[s][1],
                         edge_seeds[s][2]);
        } else {
            dodBYTE t[3];
            for (i = 0; i != 3u; ++i) {
                lfsr = (uint16_t)((lfsr >> 1)
                                  ^ ((lfsr & 1u) ? 0xB400u : 0u));
                t[i] = (uint8_t)lfsr;
            }
            rng_set_seed(t[0], t[1], t[2]);
        }
        i = 0;
        do {
            *p++ = rng_RANDOM();
        } while (++i != 0u);           /* 256 calls */
        *p++ = rng.SEED[0];
        *p++ = rng.SEED[1];
        *p++ = rng.SEED[2];
        *p++ = rng.carry;
    }

    SENTINEL[0] = 0xC5u;
    SENTINEL[1] = 0x5Cu;
    test_done();
    return 0;
}
