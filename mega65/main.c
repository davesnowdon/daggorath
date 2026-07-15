/* main.c - MEGA65 entry point: bring the platform up and hand
 * control to the core's restart loop (never returns).
 *
 * The llvm-mos soft stack stays at its platform default ($D000,
 * growing down over the link region's slack): the big buffers that
 * once crowded it out live at absolute low-RAM addresses instead
 * (layout.s), which keeps ~3K of stack headroom. */
#include "platform.h"
#include "game.h"

int main(void)
{
    plat_init();
    game_run();
    return 0;
}
