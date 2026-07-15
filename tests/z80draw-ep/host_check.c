/* host_check.c - gcc-side referee for the EP z80 draw identity test.
 *
 * Replays the same corpus (corpus_data.h) through the NORMATIVE
 * reference core/draw_ref.c into a byte-per-pixel map whose initial
 * contents unpack the wrapper's deterministic prefill, packs the
 * result into the EP's LINEAR layout (32-byte stride, bit 7 leftmost -
 * identical to plat_ep.c's row_addr/bit_mask), and compares
 * byte-for-byte against the 6144-byte bitmap slice [0x4000,0x5800) of
 * the z88dk-ticks RAM dump.  Prints "Z80DRAW-EP IDENTICAL" on success,
 * else a diff summary.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "draw_ref.h"
#include "corpus_data.h"

#define FB_BYTES   6144u
#define FB_BASE    0x4000u
#define SENT_ADDR  0x5B00u

static uint16_t rowoff(unsigned y)
{
    return (uint16_t)(y * 32u);           /* linear: the whole point */
}

static uint8_t prefill(unsigned i)
{
    return (uint8_t)((i & 0xFFu) ^ ((i >> 8) & 0xFFu) ^ 0x5Au);
}

int main(int argc, char **argv)
{
    static uint8_t ram[65536];
    static uint8_t map[FB_HEIGHT * FB_WIDTH];   /* byte per pixel */
    static uint8_t expect[FB_BYTES];
    unsigned x, y, i, ndiff, shown;
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
    if (got < FB_BASE + FB_BYTES) {
        fprintf(stderr, "host_check: dump too short (%zu bytes)\n", got);
        return 2;
    }

    if (ram[SENT_ADDR] != 0xC5u || ram[SENT_ADDR + 1] != 0x5Cu) {
        fprintf(stderr,
                "host_check: completion sentinel missing (%02X %02X) - "
                "the z80 wrapper did not run to completion "
                "(progress index: %u)\n",
                ram[SENT_ADDR], ram[SENT_ADDR + 1],
                (unsigned)(ram[SENT_ADDR + 2] | (ram[SENT_ADDR + 3] << 8)));
        return 2;
    }

    /* unpack the wrapper's prefill into the byte-per-pixel map */
    for (y = 0; y < FB_HEIGHT; ++y) {
        for (x = 0; x < FB_WIDTH; ++x) {
            unsigned off = rowoff(y) + (x >> 3);
            map[y * FB_WIDTH + x] =
                (uint8_t)((prefill(off) >> (7u - (x & 7u))) & 1u);
        }
    }

    /* replay through the normative reference.  CORPUS_START/CORPUS_END
     * (defaults: all) mirror the wrapper's subset support, for bisecting
     * a mismatch: build BOTH sides with the same -D values.  (With a
     * nonzero START the diff is only meaningful when the skipped prefix
     * would not have touched the compared pixels - true for isolating a
     * single suspect record.) */
#ifndef CORPUS_START
#define CORPUS_START 0u
#endif
#ifndef CORPUS_END
#define CORPUS_END CORPUS_COUNT
#endif
    for (i = CORPUS_START; i < CORPUS_END; ++i) {
        const corpus_rec *r = &corpus[i];
        draw_line_ref(map, r->x0, r->y0, r->x1, r->y1,
                      r->vctfad, (uint8_t)(r->flags & 0x01u),
                      0, (uint8_t)FB_HEIGHT);
    }

    /* pack to the linear layout */
    memset(expect, 0, sizeof expect);
    for (y = 0; y < FB_HEIGHT; ++y) {
        for (x = 0; x < FB_WIDTH; ++x) {
            if (map[y * FB_WIDTH + x]) {
                expect[rowoff(y) + (x >> 3)] |=
                    (uint8_t)(0x80u >> (x & 7u));
            }
        }
    }

    /* compare */
    ndiff = 0;
    shown = 0;
    for (i = 0; i < FB_BYTES; ++i) {
        if (expect[i] != ram[FB_BASE + i]) {
            if (shown < 16) {
                fprintf(stderr,
                        "  diff @0x%04X (y=%u xbyte=%u): "
                        "expect %02X got %02X\n",
                        FB_BASE + i, i / 32u, i & 31u,
                        expect[i], ram[FB_BASE + i]);
                ++shown;
            }
            ++ndiff;
        }
    }

    if (ndiff == 0) {
        printf("Z80DRAW-EP IDENTICAL (%u lines, %u framebuffer bytes)\n",
               (unsigned)CORPUS_COUNT, FB_BYTES);
        return 0;
    }
    fprintf(stderr, "Z80DRAW-EP MISMATCH: %u differing bytes\n", ndiff);
    return 1;
}
