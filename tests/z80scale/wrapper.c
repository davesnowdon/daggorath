/* wrapper.c - z80-side test wrapper for the Covox volume-scale routine,
 * compiled with zsdcc for the z88dk "+test" target (org 0x8000, run
 * under z88dk-ticks -mz80n).
 *
 * Exercises the REAL spectrum-next/snd_scale.asm: an LFSR-filled
 * source buffer is scaled through the volume LUT at 0x5B00 (built with
 * the same C formula as sound_next.c's build_lut - so the zsdcc
 * codegen of the LUT builder is under test too) for every tier 0-7,
 * each call with a different source offset and length, plus one
 * len==0 call that must write nothing.  host_check.c replicates all
 * of it with gcc and compares the RAM dump byte-for-byte.
 *
 * Exit protocol mirrors tests/z80draw: the destination area is
 * prefilled with a marker (so untouched bytes are observable), a
 * completion sentinel is written at 0x5C00/0x5C01, then test_done
 * (done.asm) is reached; run.sh points z88dk-ticks -end there and
 * collects the RAM image via -output.
 */
#include <stdint.h>

extern void snd_scale_chunk(const uint8_t *src, uint8_t *dst, uint16_t len);
extern void test_done(void);

#define SRC      ((uint8_t *)0x4000)
#define DST      ((uint8_t *)0xA000)          /* 8 x 2K regions */
#define LUT      ((uint8_t *)0x5B00)          /* snd_scale.asm SND_LUT_PG */
#define SENTINEL ((volatile uint8_t *)0x5C00)
#define SRC_LEN  2048u
#define DST_STEP 2048u
#define MARKER   0x77u

/* identical to sound_next.c build_lut (minus the tier cache) */
static void build_lut(uint8_t tier)
{
    uint16_t i;
    int16_t m = (int16_t)((uint16_t)tier << 5);

    for (i = 0; i != 256u; ++i) {
        LUT[i] = (uint8_t)(128 + ((((int16_t)i - 128) * m) >> 8));
    }
}

int main(void)
{
    uint16_t i, lfsr = 0xACE1u;
    uint8_t t;

    for (i = 0; i != SRC_LEN; ++i) {
        lfsr = (uint16_t)((lfsr >> 1) ^ ((lfsr & 1u) ? 0xB400u : 0u));
        SRC[i] = (uint8_t)lfsr;
    }
    for (i = 0; i != 8u * DST_STEP; ++i) {
        DST[i] = MARKER;
    }

    for (t = 0; t != 8u; ++t) {
        uint16_t soff = (uint16_t)(t * 37u);
        uint16_t len = (t == 3u) ? 0u
                                 : (uint16_t)(SRC_LEN - soff - t);
        SENTINEL[2] = t;              /* progress, for partial runs */
        build_lut(t);
        snd_scale_chunk(SRC + soff, DST + (uint16_t)(t * DST_STEP), len);
    }

    SENTINEL[0] = 0xC5u;
    SENTINEL[1] = 0x5Cu;
    test_done();
    return 0;
}
