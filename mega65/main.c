/* main.c - MEGA65 entry point: bring the platform up and hand
 * control to the core's restart loop (never returns). */
#include "platform.h"
#include "game.h"

int main(void)
{
    /* The platform link script puts the llvm-mos soft stack at $D000,
     * the top of the program region - which BSS now reaches within
     * bytes (the region holds the whole core plus the 7.5K shadow
     * framebuffer).  Repoint the soft stack pointer (__rc0/__rc1) at
     * $2000 so it grows down through the free $0200-$1FFF block
     * instead; the $03xx vector page is only reached ~7 KB deep, far
     * beyond any real call depth.  main() has no frame of its own and
     * never returns, so repointing here is safe. */
    asm volatile ("lda #$00\n\t"
                  "sta __rc0\n\t"
                  "lda #$20\n\t"
                  "sta __rc1");
    plat_init();
    game_run();
    return 0;
}
