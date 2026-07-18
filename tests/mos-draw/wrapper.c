/* wrapper.c - runs the REAL mega65/plat_mega65.c rasterizer under
 * mos-sim (llvm-mos 6502 codegen) over the shared draw corpus and hex-
 * dumps the resulting cell-interleaved framebuffer for host_check_m65
 * to compare against core/draw_ref.c.
 *
 * The backend .c is #included whole (stub/mega65.h shadows the real
 * hardware header), so the tested code is the exact shipping compile
 * unit; the statics (rowbase, fb layout) are reachable directly. */
#include <stdio.h>
#include <stdint.h>

#include "corpus_data.h"
#include "../../mega65/plat_mega65.c"

/* stubs for the link surface the harness never exercises */
struct stub_viciv VICIV;
uint8_t fb[6144];
uint8_t save_bounce[512];
void snd_load_blob(void) {}
void irq_handler(void) {}
uint8_t open(char *name) { (void)name; return 0xFF; }
void close(uint8_t fd) { (void)fd; }
uint16_t read512(uint8_t *buf) { (void)buf; return 0; }
uint8_t write512(const uint8_t *buf) { (void)buf; return 0; }

int main(void)
{
    uint16_t y, i, j;
    for (y = 0; y < 192u; ++y) {
        rowbase[y] = (uint16_t)(((y >> 3) << 8) + (y & 7u));
    }
    for (i = 0; i < (uint16_t)(sizeof corpus / sizeof corpus[0]); ++i) {
        plat_draw_line(corpus[i].x0, corpus[i].y0,
                       corpus[i].x1, corpus[i].y1,
                       corpus[i].vctfad, corpus[i].flags);
    }
    for (i = 0; i < 6144u; i += 32u) {
        for (j = 0; j < 32u; ++j) {
            printf("%02X", fb[i + j]);
        }
        printf("\n");
    }
    return 0;
}
