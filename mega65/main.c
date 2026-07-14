/* main.c - MEGA65 entry point: bring the platform up and hand
 * control to the core's restart loop (never returns). */
#include "platform.h"
#include "game.h"

int main(void)
{
    plat_init();
    game_run();
    return 0;
}
