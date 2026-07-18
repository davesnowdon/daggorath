/* main.c - Enterprise 64/128 entry point: bring the platform up and hand
 * control to the core's restart loop (never returns). */
#include "platform.h"
#include "game.h"

#ifdef EP_TEST_PATTERN
/* Backend isolation probe: draw a known pattern spanning the full 256x192
 * via the platform primitives, then halt.  If this renders cleanly the
 * framebuffer/cell-addressing/Nick chain is correct and any in-game garbage is the
 * game's own content/timing, not the backend. */
static const uint8_t box_glyph[7] = {
    0xFF, 0x81, 0xBD, 0xA5, 0xBD, 0x81, 0xFF
};

int main(void)
{
    uint8_t c;
    plat_init();

    plat_draw_line(0, 0, 255, 191, 0, 0);      /* diagonal TL->BR */
    plat_draw_line(255, 0, 0, 191, 0, 0);      /* diagonal TR->BL */
    plat_draw_line(0, 0, 255, 0, 0, 0);        /* top edge */
    plat_draw_line(0, 191, 255, 191, 0, 0);    /* bottom edge */
    plat_draw_line(0, 96, 255, 96, 0, 0);      /* horizontal mid */
    plat_draw_line(128, 0, 128, 191, 0, 0);    /* vertical mid */

    for (c = 0; c < 32u; ++c) {
        plat_blit_glyph(c, 0, box_glyph);      /* text row 0 (top) */
        plat_blit_glyph(c, 11, box_glyph);     /* text row 11 (mid) */
        plat_blit_glyph(c, 23, box_glyph);     /* text row 23 (bottom) */
    }

    for (;;) { }
    return 0;
}
#else
int main(void)
{
    plat_init();
    game_run();
    return 0;
}
#endif
