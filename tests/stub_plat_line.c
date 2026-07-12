/* stub_plat_line.c - satisfy viewer_list.c's plat_draw_line in tests
 * that only exercise scaling/fade math. */
#include "../core/platform.h"

void plat_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                    uint8_t vctfad, uint8_t flags)
{
    (void)x0; (void)y0; (void)x1; (void)y1; (void)vctfad; (void)flags;
}
