/* test_draw.c - invariants of the VECTOR.ASM reference rasterizer,
 * hand-derived from the 6809 semantics (plot-candidate before step,
 * LENGTH = max(|dx|,|dy|) iterations, +0.5 start bias, fade period,
 * x high-byte clip, [ymin,ymax) scanline clip, inverse mode). */
#include "test_util.h"
#include "../core/draw_ref.h"

static uint8_t fb[FB_HEIGHT * FB_WIDTH];

static void clear(void) { memset(fb, 0, sizeof fb); }

static int count_set(void)
{
    int n = 0;
    for (size_t i = 0; i < sizeof fb; ++i) {
        n += fb[i];
    }
    return n;
}

static int px(int x, int y) { return fb[y * FB_WIDTH + x]; }

int main(void)
{
    /* solid horizontal: start plotted, endpoint not, length pixels */
    clear();
    draw_line_ref(fb, 10, 10, 20, 10, 0, 0, 0, FB_HEIGHT);
    T_CHECK(count_set() == 10, "solid h-line pixels = %d, want 10",
            count_set());
    T_CHECK(px(10, 10) == 1 && px(19, 10) == 1 && px(20, 10) == 0,
            "h-line endpoints: start on, x1 off");

    /* fade period 2 plots candidates 2,4,6,8,10 -> odd x */
    clear();
    draw_line_ref(fb, 10, 10, 20, 10, 1, 0, 0, FB_HEIGHT);
    T_CHECK(count_set() == 5, "fade-1 h-line pixels = %d, want 5",
            count_set());
    T_CHECK(px(11, 10) == 1 && px(13, 10) == 1 && px(10, 10) == 0,
            "fade-1 plots every 2nd candidate starting at the 2nd");

    /* VCTFAD 0xFF = invisible */
    clear();
    draw_line_ref(fb, 10, 10, 20, 10, 0xFF, 0, 0, FB_HEIGHT);
    T_CHECK(count_set() == 0, "invisible line drew %d pixels", count_set());

    /* diagonal */
    clear();
    draw_line_ref(fb, 0, 0, 5, 5, 0, 0, 0, FB_HEIGHT);
    T_CHECK(count_set() == 5 && px(0, 0) == 1 && px(4, 4) == 1 &&
            px(5, 5) == 0, "diagonal plots (0,0)..(4,4)");

    /* steep */
    clear();
    draw_line_ref(fb, 10, 0, 10, 100, 0, 0, 0, FB_HEIGHT);
    T_CHECK(count_set() == 100, "steep line pixels = %d, want 100",
            count_set());

    /* x clip: candidates -5..4 -> only 0..4 plotted */
    clear();
    draw_line_ref(fb, -5, 50, 5, 50, 0, 0, 0, FB_HEIGHT);
    T_CHECK(count_set() == 5 && px(0, 50) == 1 && px(4, 50) == 1,
            "left-clipped line plots x 0..4 (got %d px)", count_set());

    /* x clip right: 250..259 candidates -> 250..255 */
    clear();
    draw_line_ref(fb, 250, 50, 260, 50, 0, 0, 0, FB_HEIGHT);
    T_CHECK(count_set() == 6, "right-clipped pixels = %d, want 6",
            count_set());

    /* y window clip */
    clear();
    draw_line_ref(fb, 100, 0, 100, 100, 0, 0, 20, 30);
    T_CHECK(count_set() == 10 && px(100, 20) == 1 && px(100, 29) == 1 &&
            px(100, 19) == 0 && px(100, 30) == 0,
            "y-window [20,30) keeps 10 pixels (got %d)", count_set());

    /* inverse mode clears */
    clear();
    memset(fb, 1, sizeof fb);
    draw_line_ref(fb, 10, 10, 20, 10, 0, 1, 0, FB_HEIGHT);
    T_CHECK((int)(sizeof fb) - count_set() == 10,
            "inverse clears exactly 10 pixels");

    /* zero length draws nothing */
    clear();
    draw_line_ref(fb, 10, 10, 10, 10, 0, 0, 0, FB_HEIGHT);
    T_CHECK(count_set() == 0, "zero-length line drew %d", count_set());

    return t_report("test_draw");
}
