/* cref_ep.c - zsdcc-compiled C copy of the plat_ep.c DDA, exporting
 * the same plat_draw_line_asm symbol as draw_ep.asm.
 *
 * Two purposes:
 *   1. cycle baseline: run.sh links the wrapper against this instead of
 *      the asm and compares z88dk-ticks cycle counts;
 *   2. second cross-check: this build must ALSO produce the identical
 *      framebuffer, exercising the whole harness path independently of
 *      the asm.
 *
 * The body is a verbatim copy of the plat_ep.c C rasterizer (which
 * itself mirrors core/draw_ref.c), with row_addr built lazily from
 * draw_base since there is no plat_init in the test image.  ep_tbl_pg
 * is defined here only so the wrapper links (the C DDA has no lookup
 * tables).
 */
#include <stdint.h>

#define PLAT_LINE_INVERSE 0x01u

extern uint8_t *draw_base;         /* wrapper.c: 0x4000 */
uint8_t ep_tbl_pg;                 /* unused by the C DDA */

static uint8_t *row_addr[192];
static uint8_t row_ready;

static const uint8_t bit_mask[8] = {
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};

static void build_rows(void)
{
    uint16_t y;
    for (y = 0; y < 192u; ++y) {
        row_addr[y] = draw_base + (y * 32u);
    }
    row_ready = 1u;
}

static int16_t incre(int16_t delta, uint16_t length)
{
    int32_t q = ((int32_t)(delta < 0 ? -delta : delta) << 8)
                / (int32_t)length;
    return (int16_t)(delta < 0 ? -q : q);
}

void plat_draw_line_asm(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                        uint8_t vctfad, uint8_t flags)
{
    uint8_t period = (uint8_t)(vctfad + 1u);
    uint8_t fadcnt;
    int16_t dx, dy;
    uint16_t adx, ady, length, i;
    int32_t xx, yy, xinc, yinc;

    if (!row_ready) {
        build_rows();
    }

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
                yi >= 0 && yi < 192) {
                uint8_t *p = row_addr[yi] + ((uint8_t)xi >> 3);
                uint8_t m = bit_mask[(uint8_t)xi & 7u];
                if (flags & PLAT_LINE_INVERSE) {
                    *p &= (uint8_t)~m;
                } else {
                    *p |= m;
                }
            }
        }
        xx += xinc;
        yy += yinc;
    }
}
