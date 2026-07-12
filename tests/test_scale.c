/* test_scale.c - viewer_SCALE must match ASCALX/ASCALY hand-derived
 * values (8-bit distance, unsigned MUL, sign fixup, ASR7, add centroid),
 * and SETFAD must match SETFAX's brightness table. */
#include "test_util.h"
#include "../core/viewer.h"

viewer_state viewer;   /* viewer.c not linked; provide the state */

int main(void)
{
    /* dist 32, scale 200: 6400>>7 = 50 */
    T_CHECK(viewer_SCALE(160, 0xC8, 128) == 178, "case1 = %d",
            viewer_SCALE(160, 0xC8, 128));
    /* dist 0 */
    T_CHECK(viewer_SCALE(76, 0x80, 76) == 76, "dist0");
    /* negative dist: -68*200 = -13600, ASR7 floors to -107 */
    T_CHECK(viewer_SCALE(60, 0xC8, 128) == 21, "neg = %d",
            viewer_SCALE(60, 0xC8, 128));
    /* the /127-vs->>7 divergence case: 127*255 = 32385 >>7 = 253
     * (the port's float /127.0 gives 255 - we follow the 6809) */
    T_CHECK(viewer_SCALE(255, 0xFF, 128) == 128 + 253, "radix7 = %d",
            viewer_SCALE(255, 0xFF, 128));
    /* negative floor: dist -1, scale 255: -255>>7 = -2 (floor, not -1) */
    T_CHECK(viewer_SCALE(127, 0xFF, 128) == 126, "negfloor = %d",
            viewer_SCALE(127, 0xFF, 128));

    /* SETFAD: light-7-range ladder */
    viewer.MAGFLG = 0;
    {
        static const struct { dodBYTE light, range, want; } cases[] = {
            {8, 1, 0x00},    /* 0 -> solid */
            {8, 2, 0x01},    /* -1 -> every 2nd */
            {8, 3, 0x02},    /* -2 */
            {8, 5, 0x08},    /* -4 */
            {8, 7, 0x20},    /* -6 */
            {8, 8, 0xFF},    /* -7 -> dark */
            {0, 0, 0xFF},    /* -7 -> dark */
            {9, 0, 0x00},    /* +2 -> solid */
        };
        for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
            viewer.RLIGHT = cases[i].light;
            viewer.RANGE = cases[i].range;
            viewer_SETFAD();
            T_CHECK(viewer.VCTFAD == cases[i].want,
                    "SETFAD(l=%u,r=%u) = %02X want %02X",
                    cases[i].light, cases[i].range, viewer.VCTFAD,
                    cases[i].want);
        }
    }
    /* MAGFLG uses MLIGHT once then clears */
    viewer.MAGFLG = 1;
    viewer.MLIGHT = 9;
    viewer.RLIGHT = 0;
    viewer.RANGE = 1;
    viewer_SETFAD();
    T_CHECK(viewer.VCTFAD == 0x00 && viewer.MAGFLG == 0,
            "MAGFLG one-shot (fad=%02X mag=%u)", viewer.VCTFAD,
            viewer.MAGFLG);

    return t_report("test_scale");
}
