/* viewer_list.c - VCTLST.ASM: display-list processing.
 *
 * Scaling follows ASCALX/ASCALY exactly: the coordinate's distance from
 * the centroid's LOW byte is an 8-bit two's-complement value; the 8x8
 * unsigned MUL runs on its absolute value; NEGD fixes the sign; ASRD7
 * arithmetic-shifts the 16-bit product right 7 (floor divide by 128);
 * ADDD adds the full 16-bit centroid.  This deliberately diverges from
 * the C++ port's float "/127.0" - the 6809 is the authority.
 *
 * The shape tables are the port's EXPANDED absolute-pair layout
 * ([nlists][npts][x,y]*), byte-verified against the original packed
 * lists by tools/gen_vectables.py.
 */
#include "viewer.h"
#include "platform.h"

/* C99 leaves >> on negatives implementation-defined; every target we
 * build for (gcc, clang/llvm-mos, sdcc/zsdcc) implements an arithmetic
 * shift, which is what ASRD7 is.  Guard the assumption. */
typedef char assert_arith_shift[((-2 >> 1) == -1) ? 1 : -1];

int16_t viewer_SCALE(dodBYTE coord, dodBYTE scale, dodSHORT centr)
{
    int8_t dist = (int8_t)(coord - (dodBYTE)centr);   /* SUBB centr LOW */
    int16_t prod;
    if (dist < 0) {
        prod = (int16_t)-(int16_t)((uint8_t)-dist * scale);  /* NEG,MUL,NEGD */
    } else {
        prod = (int16_t)((uint8_t)dist * scale);             /* MUL */
    }
    return (int16_t)((int16_t)centr + (prod >> 7));          /* ASRD7,ADDD */
}

void viewer_drawVectorList(const uint8_t *vla)
{
    uint8_t nlists, l, p, npts;
    uint16_t i = 1;
    uint8_t flags;

    if (viewer.VCTFAD == 0xFF) {
        return;
    }
    flags = viewer.INVFLG ? PLAT_LINE_INVERSE : 0u;
    nlists = vla[0];
    for (l = 0; l < nlists; ++l) {
        npts = vla[i++];
        for (p = 0; p + 1u < npts; ++p) {
            int16_t x0 = viewer_SCALE(vla[i + 2u * p], viewer.VXSCAL,
                                      viewer.VCNTRX);
            int16_t y0 = viewer_SCALE(vla[i + 2u * p + 1u], viewer.VYSCAL,
                                      viewer.VCNTRY);
            int16_t x1 = viewer_SCALE(vla[i + 2u * p + 2u], viewer.VXSCAL,
                                      viewer.VCNTRX);
            int16_t y1 = viewer_SCALE(vla[i + 2u * p + 3u], viewer.VYSCAL,
                                      viewer.VCNTRY);
            plat_draw_line(x0, y0, x1, y1, viewer.VCTFAD, flags);
        }
        i += 2u * (uint16_t)npts;
    }
}

/* SETFAX (VCTLST.ASM): brightness = light - 7 - RANGE.
 *   >= 0            -> solid (VCTFAD = 0)
 *   -1 .. -6        -> BITMSK dot periods $01,$02,$04,$08,$10,$20
 *   <= -7           -> invisible (0xFF)
 * MAGFLG selects MLIGHT for one call, then self-clears. */
static const dodBYTE FADE_BITMSK[8] = {
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};

void viewer_SETFAD(void)
{
    int8_t a;
    dodBYTE b = 0;

    if (viewer.MAGFLG) {
        a = (int8_t)viewer.MLIGHT;
        viewer.MAGFLG = 0;
    } else {
        a = (int8_t)viewer.RLIGHT;
    }
    a = (int8_t)(a - 7);
    a = (int8_t)(a - viewer.RANGE);
    if (a < 0) {
        if (a <= -7) {
            b = 0xFF;                        /* total darkness */
        } else {
            b = FADE_BITMSK[8 + a];          /* a in -6..-1 */
        }
    }
    viewer.VCTFAD = b;
}
