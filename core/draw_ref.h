/* draw_ref.h - the REFERENCE line rasterizer: an exact C port of
 * VECTOR.ASM's fixed-point DDA, including its fade (dot-period) gate,
 * clipping and rounding.  Every backend must produce these exact pixels:
 * desktop and MEGA65 compile this file; the Spectrum Next Z80n assembly
 * mirrors it and is validated against desktop golden framebuffers.
 *
 * fb is a 256x192 byte-per-pixel buffer (0 = off, 1 = on).
 */
#ifndef DOD_DRAW_REF_H
#define DOD_DRAW_REF_H

#include "dod_types.h"

#define FB_WIDTH  256u
#define FB_HEIGHT 192u

/* vctfad: the VCTFAD value; effective plot period is vctfad+1
 * (0 = solid, 1 = every 2nd dot, ..., 0xFF = invisible: no-op).
 * inverse: VDGINV - clear pixels instead of setting them.
 * ymin/ymax_excl: VDBAS/VDEND scanline clip window. */
void draw_line_ref(uint8_t *fb,
                   int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   dodBYTE vctfad, uint8_t inverse,
                   uint8_t ymin, uint8_t ymax_excl);

#endif /* DOD_DRAW_REF_H */
