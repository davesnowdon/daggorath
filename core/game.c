/* game.c - dodgame.cpp / DAGGORATH.ASM: top-level game controller.
 *
 * game_run() is the port's oslink main loop: COMINI once, then pump the
 * scheduler until death/win/load, handling demo restart and the
 * demo-to-real-game transition.
 */
#include "game.h"
#include "sched.h"
#include "player.h"
#include "object.h"
#include "creature.h"
#include "parser.h"
#include "dungeon.h"
#include "viewer.h"
#include "sound.h"
#include "rng.h"
#include "platform.h"

game_state game;

/* wait constants (dodgame.cpp ms -> jiffies) */
#define WAIT_PREPARE   150u   /* 2500 ms with "PREPARE!" up */
#define WAIT_SEER_MAP  180u   /* 3000 ms demo map display */
#define WAIT_GAME      90u    /* 1500 ms game_WAIT */

/* Attract-mode token stream: EXAMINE; PULL RIGHT TORCH; USE RIGHT; LOOK;
 * MOVE; PULL LEFT SHIELD; PULL RIGHT SWORD; MOVE; MOVE; ATTACK RIGHT;
 * TURN RIGHT; MOVE x3; TURN RIGHT; MOVE x2; END. */
const dodBYTE GAME_DEMO_CMDS[GAME_DEMO_LEN] = {
    0x01, 0x0F,
    0x03, 0x26, 0x05,
    0x19,
    0x02, 0x37, 0x05,
    0x01, 0x1E,
    0x01, 0x22,
    0x03, 0x26, 0x01,
    0x0F,
    0x03, 0x26, 0x05,
    0x14,
    0x01, 0x22,
    0x01, 0x22,
    0x02, 0x01, 0x05,
    0x02, 0x33, 0x05,
    0x01, 0x22,
    0x01, 0x22,
    0x01, 0x22,
    0x02, 0x33, 0x05,
    0x01, 0x22,
    0x01, 0x22,
    0x00
};

void game_INIVU(void)
{
    viewer_clearArea(&viewer.TXTSTS);
    viewer_clearArea(&viewer.TXTPRI);
    player_HUPDAT();
    ++player.HEARTC;
    --player.HEARTF;
    --player.HBEATF;
    viewer_STATUS();
    player_PLOOK();
}

void game_COMINI(void)
{
    sched_SYSTCB();
    object_CreateAll();
    player.HBEATF = 0;
    viewer_clearArea(&viewer.TXTSTS);
    viewer_clearArea(&viewer.TXTPRI);
    viewer.VXSCAL = 0x80;
    viewer.VYSCAL = 0x80;

    /* title fade: copyright + welcome + buzz; a keypress starts the
     * real game, completion of the fade starts the demo */
    game.AUTFLG = viewer_ShowFade(FADE_BEGIN);

    player_setInitialObjects(game.AUTFLG);
    viewer_displayPrepare();
    viewer.display_mode = MODE_TITLE;
    viewer_draw_game();
    core_wait_jiffies(WAIT_PREPARE);

    creature_NEWLVL();
    if (game.AUTFLG) {
        /* demo: show the seer's map of the level for 3 seconds */
        viewer.display_mode = MODE_TITLE;
        viewer.showSeerMap = 1;
        --viewer.UPDATE;
        viewer_draw_game();
        core_wait_jiffies(WAIT_SEER_MAP);
    }
    game_INIVU();
    viewer_PROMPT();
}

void game_Restart(void)
{
    object_Reset();
    creature_Reset();
    parser_Reset();
    player_Reset();
    sched_Reset();
    viewer_Reset();
    game.hasWon = 0;

    dungeon.VFTPTR = 0;
    sched_SYSTCB();
    object_CreateAll();
    player.HBEATF = 0;
    player_setInitialObjects(0);
    viewer_displayPrepare();
    viewer_displayCopyright();
    viewer.display_mode = MODE_TITLE;
    viewer_draw_game();
    core_wait_jiffies(WAIT_PREPARE);

    creature_NEWLVL();
    game_INIVU();
    viewer_PROMPT();
}

static void game_LoadGame(void)
{
    /* scheduler LOAD via plat_load_state; wired with the save format */
    viewer_setVidInv((uint8_t)(game.LEVEL % 2u));
    --viewer.UPDATE;
    viewer_draw_game();
    game_INIVU();
    viewer_PROMPT();
}

void game_WAIT(void)
{
    if (core_wait_jiffies_abortable(WAIT_GAME)) {
        return;
    }
}

void game_run(void)
{
    /* C++ global constructors ran before the port's main(); the C core
     * must do that work explicitly before anything draws or schedules. */
    rng_reset();
    dungeon_Reset();
    parser_Reset();
    object_Reset();
    creature_Reset();
    player_Reset();
    viewer_Reset();
    sched_Reset();

    /* port dodGame constructor state */
    game.LEVEL = 2;
    game.AUTFLG = 1;
    game.hasWon = 0;
    game.DEMOPTR = 0;
    game.demoRestart = 1;

    game_COMINI();
    for (;;) {
        uint8_t status = core_pump();
        if (status == PUMP_RUN) {
            plat_yield();
            continue;
        }
        if (status == PUMP_LOAD_ABANDON) {
            sched.ZFLAG = 0;
            game_LoadGame();
        } else if (game.AUTFLG) {
            if (game.demoRestart) {
                /* demo over: restart the attract sequence */
                game.hasWon = 0;
                game.DEMOPTR = 0;
                object_Reset();
                creature_Reset();
                parser_Reset();
                player_Reset();
                sched_Reset();
                viewer_Reset();
                dungeon.VFTPTR = 0;
                game_COMINI();
            } else {
                /* keypress during demo: start a real game */
                game.AUTFLG = 0;
                game_Restart();
            }
        } else {
            game_Restart();
        }
    }
}
