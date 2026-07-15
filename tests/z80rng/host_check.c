/* host_check.c - gcc-side referee for the z80 RNG identity test.
 * Replays wrapper.c's corpus (same edge seeds, same LFSR-derived
 * seeds) through the NORMATIVE core/rng.c rng_RANDOM and compares the
 * z88dk-ticks RAM dump byte-for-byte: 256 return values + the four
 * state bytes (SEED[0..2], carry) per seed.  Prints "Z80RNG IDENTICAL"
 * on success, else a diff summary.
 */
#include <stdio.h>
#include <stdint.h>
#include "rng.h"

#define OUT_BASE  0xA000u
#define SENT_ADDR 0x9F00u
#define NSEEDS    20u
#define NCALLS    256u
#define BLOCK     (NCALLS + 4u)

static const dodBYTE edge_seeds[4][3] = {
    { 0x00, 0x00, 0x00 },
    { 0xFF, 0xFF, 0xFF },
    { 0x01, 0x00, 0x00 },
    { 0x00, 0x00, 0xE1 },
};

int main(int argc, char **argv)
{
    static uint8_t ram[65536];
    static uint8_t expect[NSEEDS * BLOCK];
    uint16_t lfsr = 0x1D87u;
    unsigned s, i, n = 0, ndiff = 0, shown = 0;
    FILE *fp;
    size_t got;

    if (argc != 2) {
        fprintf(stderr, "usage: host_check <ticks-ram-dump>\n");
        return 2;
    }
    fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "host_check: cannot open %s\n", argv[1]);
        return 2;
    }
    got = fread(ram, 1, sizeof ram, fp);
    fclose(fp);
    if (got < OUT_BASE + sizeof expect) {
        fprintf(stderr, "host_check: dump too short (%zu bytes)\n", got);
        return 2;
    }
    if (ram[SENT_ADDR] != 0xC5u || ram[SENT_ADDR + 1] != 0x5Cu) {
        fprintf(stderr,
                "host_check: completion sentinel missing (%02X %02X) - "
                "wrapper did not finish (progress seed %u)\n",
                ram[SENT_ADDR], ram[SENT_ADDR + 1], ram[SENT_ADDR + 2]);
        return 1;
    }

    for (s = 0; s != NSEEDS; ++s) {
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
        for (i = 0; i != NCALLS; ++i) {
            expect[n++] = rng_RANDOM();
        }
        expect[n++] = rng.SEED[0];
        expect[n++] = rng.SEED[1];
        expect[n++] = rng.SEED[2];
        expect[n++] = rng.carry;
    }

    for (i = 0; i != n; ++i) {
        if (ram[OUT_BASE + i] != expect[i]) {
            if (shown < 8) {
                fprintf(stderr,
                        "  seed %u %s %u: got %02X want %02X\n",
                        i / BLOCK,
                        (i % BLOCK) < NCALLS ? "call" : "state byte",
                        (i % BLOCK) < NCALLS ? (i % BLOCK)
                                             : (i % BLOCK) - NCALLS,
                        ram[OUT_BASE + i], expect[i]);
                ++shown;
            }
            ++ndiff;
        }
    }
    if (ndiff != 0) {
        fprintf(stderr, "Z80RNG MISMATCH: %u bytes differ\n", ndiff);
        return 1;
    }
    printf("Z80RNG IDENTICAL\n");
    return 0;
}
