/* draw_ref.c - exact C port of VECTOR.ASM (see draw_ref.h).
 *
 * Correspondence with the 6809 source, kept line-for-line auditable:
 *   INC VCTFAD / BEQ        -> period = vctfad+1; 0 means invisible
 *   LENGTH = max(|dx|,|dy|) ; zero length draws nothing
 *   INCRE/DIVIDE            -> (delta<<8)/length, truncated toward zero
 *   XX = X0 + 0.5           -> 24.8 fixed point, fraction byte = 0x80
 *   VECT30 loop             -> plot-candidate FIRST, then step; runs
 *                              LENGTH times (endpoint never plotted)
 *   TST XX / BNE            -> skip when x-integer has high-byte bits
 *   CMPX VDBAS/VDEND        -> skip when scanline outside [ymin,ymax)
 *   VDGINV                  -> inverse mode clears pixels
 */
#include "draw_ref.h"

static int16_t incre(int16_t delta, uint16_t length)
{
    /* DIVIDE computes abs(delta)*256/length; INCRE restores the sign.
     * C's toward-zero division on the positive value matches. */
    int32_t q = ((int32_t)(delta < 0 ? -delta : delta) << 8)
                / (int32_t)length;
    return (int16_t)(delta < 0 ? -q : q);
}

void draw_line_ref(uint8_t *fb,
                   int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   dodBYTE vctfad, uint8_t inverse,
                   uint8_t ymin, uint8_t ymax_excl)
{
    dodBYTE period = (dodBYTE)(vctfad + 1u);
    dodBYTE fadcnt;
    int16_t dx, dy;
    uint16_t adx, ady, length, i;
    int32_t xx, yy, xinc, yinc;

    if (period == 0) {            /* VCTFAD == 0xFF: invisible */
        return;
    }
    fadcnt = period;

    dx = (int16_t)(x1 - x0);
    dy = (int16_t)(y1 - y0);
    adx = (uint16_t)(dx < 0 ? -dx : dx);
    ady = (uint16_t)(dy < 0 ? -dy : dy);
    length = (ady < adx) ? adx : ady;
    if (length == 0) {
        return;
    }

    xinc = incre(dx, length);
    yinc = incre(dy, length);

    xx = ((int32_t)x0 << 8) | 0x80;   /* X0 + 0.5 */
    yy = ((int32_t)y0 << 8) | 0x80;

    for (i = length; i != 0; --i) {
        --fadcnt;
        if (fadcnt == 0) {
            int16_t xi = (int16_t)(xx >> 8);
            int16_t yi = (int16_t)(yy >> 8);
            fadcnt = period;
            if ((xi & (int16_t)0xFF00) == 0 &&
                yi >= (int16_t)ymin && yi < (int16_t)ymax_excl) {
                uint8_t *p = fb + ((uint16_t)yi * FB_WIDTH) + (uint8_t)xi;
                *p = inverse ? 0u : 1u;
            }
        }
        xx += xinc;
        yy += yinc;
    }
}
