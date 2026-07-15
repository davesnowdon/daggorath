/* host_check.c - gcc-side referee for the z80 volume-scale identity
 * test.  Replicates wrapper.c exactly (LFSR source, marker prefill,
 * per-tier LUT build + copy, the len==0 no-op at tier 3) and compares
 * the z88dk-ticks RAM dump byte-for-byte over the source buffer, the
 * final LUT (tier 7's - checks the zsdcc-compiled builder as well)
 * and all 8 destination regions.  Prints "Z80SCALE IDENTICAL" on
 * success, else a diff summary.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define SRC_BASE  0x4000u
#define LUT_BASE  0x5B00u
#define SENT_ADDR 0x5C00u
#define DST_BASE  0xA000u
#define SRC_LEN   2048u
#define DST_STEP  2048u
#define MARKER    0x77u

static uint8_t expect[65536];

static void build_lut(uint8_t *lut, uint8_t tier)
{
    unsigned i;
    int m = tier * 32;

    for (i = 0; i != 256u; ++i) {
        lut[i] = (uint8_t)(128 + (((int)i - 128) * m >> 8));
    }
}

static unsigned diff_range(const uint8_t *ram, unsigned base, unsigned len,
                           const char *what)
{
    unsigned i, n = 0, shown = 0;

    for (i = 0; i != len; ++i) {
        if (ram[base + i] != expect[base + i]) {
            if (shown < 8) {
                fprintf(stderr,
                        "  %s +%u (0x%04X): got %02X want %02X\n",
                        what, i, base + i, ram[base + i], expect[base + i]);
                ++shown;
            }
            ++n;
        }
    }
    return n;
}

int main(int argc, char **argv)
{
    static uint8_t ram[65536];
    uint16_t lfsr = 0xACE1u;
    unsigned i, t, ndiff;
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
    if (got < DST_BASE + 8u * DST_STEP) {
        fprintf(stderr, "host_check: dump too short (%zu bytes)\n", got);
        return 2;
    }
    if (ram[SENT_ADDR] != 0xC5u || ram[SENT_ADDR + 1] != 0x5Cu) {
        fprintf(stderr,
                "host_check: completion sentinel missing (%02X %02X) - "
                "wrapper did not finish (progress tier %u)\n",
                ram[SENT_ADDR], ram[SENT_ADDR + 1], ram[SENT_ADDR + 2]);
        return 1;
    }

    for (i = 0; i != SRC_LEN; ++i) {
        lfsr = (uint16_t)((lfsr >> 1) ^ ((lfsr & 1u) ? 0xB400u : 0u));
        expect[SRC_BASE + i] = (uint8_t)lfsr;
    }
    memset(expect + DST_BASE, MARKER, 8u * DST_STEP);
    for (t = 0; t != 8u; ++t) {
        unsigned soff = t * 37u;
        unsigned len = (t == 3u) ? 0u : SRC_LEN - soff - t;

        build_lut(expect + LUT_BASE, (uint8_t)t);
        for (i = 0; i != len; ++i) {
            expect[DST_BASE + t * DST_STEP + i] =
                expect[LUT_BASE + expect[SRC_BASE + soff + i]];
        }
    }

    ndiff = diff_range(ram, SRC_BASE, SRC_LEN, "src");
    ndiff += diff_range(ram, LUT_BASE, 256u, "lut");
    ndiff += diff_range(ram, DST_BASE, 8u * DST_STEP, "dst");
    if (ndiff != 0) {
        fprintf(stderr, "Z80SCALE MISMATCH: %u bytes differ\n", ndiff);
        return 1;
    }
    printf("Z80SCALE IDENTICAL\n");
    return 0;
}
