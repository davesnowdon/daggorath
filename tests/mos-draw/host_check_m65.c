/* host_check_m65.c - normative reference for the mos-draw identity
 * test: run the same corpus through core/draw_ref.c (gcc, byte-per-
 * pixel map), pack into the MEGA65 backend's cell-interleaved 6144-byte
 * layout (rowbase[y] = (y>>3)*256 + (y&7); byte = base + (x & 0xF8);
 * bit 7-(x&7)), and byte-compare against the mos-sim hex dump. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "draw_ref.h"
#include "corpus_data.h"

/* draw_ref.c plots into this byte-per-pixel map */
static uint8_t map[FB_WIDTH * FB_HEIGHT];

int main(int argc, char **argv)
{
    static uint8_t m65[6144];
    static uint8_t got[6144];
    FILE *f;
    char line[80];
    size_t n = 0, i;
    unsigned x, y, ndiff = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: host_check_m65 <mos-sim-hex-dump>\n");
        return 2;
    }
    for (i = 0; i < sizeof corpus / sizeof corpus[0]; ++i) {
        draw_line_ref(map, corpus[i].x0, corpus[i].y0,
                      corpus[i].x1, corpus[i].y1,
                      corpus[i].vctfad,
                      (uint8_t)(corpus[i].flags & 0x01u),
                      0, (uint8_t)FB_HEIGHT);
    }
    for (y = 0; y < 192; ++y) {
        for (x = 0; x < 256; ++x) {
            if (map[y * FB_WIDTH + x]) {
                m65[((y >> 3) << 8) + (y & 7) + (x & 0xF8u)]
                    |= (uint8_t)(0x80u >> (x & 7u));
            }
        }
    }

    f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    while (n < sizeof got && fgets(line, sizeof line, f)) {
        char *p = line;
        while (p[0] && p[1] && p[0] != '\n' && n < sizeof got) {
            unsigned b;
            if (sscanf(p, "%2x", &b) != 1) {
                break;
            }
            got[n++] = (uint8_t)b;
            p += 2;
        }
    }
    fclose(f);
    if (n != sizeof got) {
        fprintf(stderr, "MOS-DRAW MISMATCH: dump has %zu bytes, want 6144\n",
                n);
        return 1;
    }
    for (i = 0; i < sizeof m65; ++i) {
        if (m65[i] != got[i]) {
            if (ndiff < 10) {
                fprintf(stderr, "  off %zu: ref %02X mos %02X\n",
                        i, m65[i], got[i]);
            }
            ++ndiff;
        }
    }
    if (ndiff) {
        fprintf(stderr, "MOS-DRAW MISMATCH: %u bytes differ\n", ndiff);
        return 1;
    }
    printf("MOS-DRAW IDENTICAL (%zu corpus lines, 6144 bytes)\n",
           sizeof corpus / sizeof corpus[0]);
    return 0;
}
