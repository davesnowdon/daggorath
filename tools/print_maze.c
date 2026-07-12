/* print_maze.c - dump a generated level as ASCII for scenario authoring.
 *
 * Links only core/dungeon.c + core/rng.c and supplies the few globals
 * they reference; walls come out exactly as the game generates them
 * (LEVTAB seeds), so routes plotted here replay identically in-game.
 *
 *   gcc -std=c99 -I../core -o print_maze print_maze.c \
 *       ../core/dungeon.c ../core/rng.c && ./print_maze 0
 *
 * Legend: 3-char cells; walls '#', doors '%', secret doors '$';
 * vertical features printed below the map from VFTTAB.
 */
#include <stdio.h>
#include <stdlib.h>
#include "dungeon.h"
#include "game.h"
#include "player.h"
#include "sched.h"

/* globals dungeon.c references; their owning .c files are not linked */
game_state game;
player_state player;
sched_state sched;
void (*core_prompt_hook)(void);

static char wallch(dodBYTE cell, int shift)
{
    switch ((cell >> shift) & 3) {
    case 1:  return '%';   /* door */
    case 2:  return '$';   /* secret door */
    case 3:  return '#';   /* wall */
    }
    return ' ';
}

int main(int argc, char **argv)
{
    int r, c, i;

    game.LEVEL = (argc > 1) ? (dodBYTE)atoi(argv[1]) : 0;
    game.RandomMaze = 0;
    game.IsDemo = 0;
    sched.curTime = 0;
    player.PROW = 0x10;
    player.PCOL = 0x0B;

    dungeon_Reset();
    dungeon_DGNGEN();
    dungeon_CalcVFI();

    if (argc > 2) {   /* raw mode: 1024 hex bytes for route scripts */
        for (r = 0; r < 1024; ++r) {
            printf("%02X%c", dungeon.MAZLND[r], (r % 32 == 31) ? '\n' : ' ');
        }
        return 0;
    }

    printf("level %u (displayed %u)   cols 0-31 ->\n",
           game.LEVEL, game.LEVEL + 1u);
    for (r = 0; r < 32; ++r) {
        for (c = 0; c < 32; ++c) {
            dodBYTE cell = dungeon.MAZLND[r * 32 + c];
            putchar('+');
            putchar(cell == 0xFF ? '#' : wallch(cell, 0));   /* north */
        }
        printf("+\n");
        for (c = 0; c < 32; ++c) {
            dodBYTE cell = dungeon.MAZLND[r * 32 + c];
            putchar(cell == 0xFF ? '#' : wallch(cell, 6));   /* west */
            putchar(cell == 0xFF ? '#' : ' ');
        }
        /* east wall of the last col */
        putchar(wallch(dungeon.MAZLND[r * 32 + 31], 2));
        printf("  r%d\n", r);
    }
    for (c = 0; c < 32; ++c) {
        dodBYTE cell = dungeon.MAZLND[31 * 32 + c];
        putchar('+');
        putchar(cell == 0xFF ? '#' : wallch(cell, 4));       /* south */
    }
    printf("+\n\nVFTTAB from VFTPTR %u (this level, then next below):\n",
           dungeon.VFTPTR);
    i = dungeon.VFTPTR;
    for (r = 0; r < 2; ++r) {
        while (dungeon.VFTTAB[i] != 0xFF) {
            printf("  %s type %u at r%u c%u\n",
                   r == 0 ? "here " : "below",
                   dungeon.VFTTAB[i], dungeon.VFTTAB[i + 1],
                   dungeon.VFTTAB[i + 2]);
            i += 3;
        }
        ++i;
    }
    return 0;
}
