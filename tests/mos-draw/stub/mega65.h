/* Stub <mega65.h> for the mos-draw identity harness: just enough for
 * mega65/plat_mega65.c to COMPILE under mos-sim-clang.  Hardware
 * registers become a plain RAM struct - the harness only ever calls
 * plat_draw_line, which touches fb/rowbase/bit_mask, no hardware. */
#ifndef MOSDRAW_STUB_MEGA65_H
#define MOSDRAW_STUB_MEGA65_H

#include <stdint.h>

struct stub_viciv {
    uint8_t  key, ctrlc, sdbdrwd_msb, ctrlb, ctrl1, ctrl2, addr;
    uint8_t  bordercol, screencol;
    uint32_t scrnptr;
    uint8_t  charptr_lsb, charptr_msb, charptr_bnk;
    uint8_t  rasline0, rasterline, imr, irr;
};
extern struct stub_viciv VICIV;

#endif
