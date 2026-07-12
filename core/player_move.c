/* player_move.c - PMOVE / PTURN (PTURN.ASM), PCLIMB (PCLIMB.ASM),
 * PSTEP and the ShowTurn sweep animation: player movement handlers.
 *
 * Extracted from the C++ port's Player class (player.cpp).  Wait-site
 * conversions are documented in docs/wait-site-audit.md.
 *
 * ms -> jiffy conversions in this file:
 *   moveDelay/2 = 12 ms half-step -> WAIT_HALF_MOVE (1 jiffy); note the
 *     port waits moveDelay/2 on BOTH halves of a forward/back move
 *   turnDelay = 20 ms per sweep line -> WAIT_TURN (1 jiffy)
 *   viewer.prepPause = 2500 ms level-change pause -> WAIT_PREP_PAUSE
 */
#include "player.h"
#include "parser.h"
#include "viewer.h"
#include "sched.h"
#include "object.h"
#include "dungeon.h"
#include "creature.h"
#include "game.h"
#include "sound.h"
#include "tables_vec.h"
#include "platform.h"

/* viewer.prepPause: 2500 ms with the blank/title screen up while the
 * new level generates (port PCLIMB), at 60 Hz */
#define WAIT_PREP_PAUSE 150u

/* ShowTurn sweep geometry (port locals inc/lines/y0/y1) */
#define TURN_INC   32
#define TURN_LINES 8
#define TURN_Y0    17
#define TURN_Y1    135

/* PCLIMB.ASM: processes CLIMB command. */
void player_PCLIMB(void)
{
    dodBYTE vres;
    int8_t res;
    dodBYTE A, B;
    RowCol rc;
    dodBYTE temp;

    rc.row = player.PROW;
    rc.col = player.PCOL;
    vres = dungeon_VFIND(rc);
    if (vres == VF_NULL) {
        parser_CMDERR();
        return;
    }
    res = parser_PARSER(DIRTAB, &A, &B, 1);
    if (res <= 0) {
        parser_CMDERR();
        return;
    }
    if (A == DIR_UP) {
        /* climb up (holes only climbable after the wizard is dead) */
        if (vres == VF_LADDER_UP ||
            (vres == VF_HOLE_UP && creature.FRZFLG)) {
            viewer_displayPrepare();
            temp = viewer.display_mode;
            viewer.display_mode = MODE_TITLE;
            viewer_draw_game();
            core_wait_jiffies(WAIT_PREP_PAUSE);
            viewer.display_mode = temp;
            --game.LEVEL;
            creature_NEWLVL();
            game_INIVU();
        } else {
            parser_CMDERR();
            return;
        }
    } else if (A == DIR_DOWN) {
        /* climb down */
        if (vres == VF_LADDER_DOWN || vres == VF_HOLE_DOWN) {
            viewer_displayPrepare();
            temp = viewer.display_mode;
            viewer.display_mode = MODE_TITLE;
            viewer_draw_game();
            core_wait_jiffies(WAIT_PREP_PAUSE);
            viewer.display_mode = temp;
            ++game.LEVEL;
            creature_NEWLVL();
            game_INIVU();
        } else {
            parser_CMDERR();
            return;
        }
    } else {
        parser_CMDERR();
        return;
    }
}

/* HUMAN.ASM PMOVE: processes MOVE command. */
void player_PMOVE(void)
{
    int8_t res;
    dodBYTE A, B;

    res = parser_PARSER(DIRTAB, &A, &B, 1);
    if (res < 0) {
        parser_CMDERR();
        return;
    } else if (res == 0) {
        /* move forward */
        --viewer.HLFSTP;
        viewer_PUPDAT();
        if (core_wait_jiffies_abortable(WAIT_HALF_MOVE)) {
            return;
        }
        viewer.HLFSTP = 0;
        player_PSTEP(0);
        PDAM = (dodSHORT)(PDAM + (dodSHORT)((player.POBJWT >> 3) + 3));
        player_HUPDAT();
        --viewer.UPDATE;
        viewer_draw_game();
        if (core_wait_jiffies_abortable(WAIT_HALF_MOVE)) {
            return;
        }
        return;
    } else if (A == DIR_BACK) {
        /* move back */
        --viewer.BAKSTP;
        viewer_PUPDAT();
        if (core_wait_jiffies_abortable(WAIT_HALF_MOVE)) {
            return;
        }
        viewer.BAKSTP = 0;
        player_PSTEP(2);
        PDAM = (dodSHORT)(PDAM + (dodSHORT)((player.POBJWT / 8) + 3));
        player_HUPDAT();
        --viewer.UPDATE;
        viewer_draw_game();
        if (core_wait_jiffies_abortable(WAIT_HALF_MOVE)) {
            return;
        }
        return;
    } else if (A == DIR_RIGHT) {
        /* move right */
        if (player_PSTEP(1)) {
            if (viewer.display_mode == MODE_3D) {
                player_ShowTurn(DIR_RIGHT);
            }
        }
        PDAM = (dodSHORT)(PDAM + (dodSHORT)((player.POBJWT / 8) + 3));
        player_HUPDAT();
        --viewer.UPDATE;
        viewer_draw_game();
        return;
    } else if (A == DIR_LEFT) {
        /* move left */
        if (player_PSTEP(3)) {
            if (viewer.display_mode == MODE_3D) {
                player_ShowTurn(DIR_LEFT);
            }
        }
        PDAM = (dodSHORT)(PDAM + (dodSHORT)((player.POBJWT / 8) + 3));
        player_HUPDAT();
        --viewer.UPDATE;
        viewer_draw_game();
        return;
    } else {
        parser_CMDERR();
        return;
    }
}

/* PTURN.ASM: processes TURN command. */
void player_PTURN(void)
{
    int8_t res;
    dodBYTE A, B;

    res = parser_PARSER(DIRTAB, &A, &B, 1);
    if (res != 1) {
        parser_CMDERR();
        return;
    }
    if (A == DIR_LEFT) {
        /* left turn */
        --player.PDIR;
        player.PDIR = (dodBYTE)(player.PDIR & 3);
        if (viewer.display_mode == MODE_3D) {
            player_ShowTurn(DIR_LEFT);
        }
        --viewer.UPDATE;
        viewer_draw_game();
        return;
    } else if (A == DIR_RIGHT) {
        /* right turn */
        ++player.PDIR;
        player.PDIR = (dodBYTE)(player.PDIR & 3);
        if (viewer.display_mode == MODE_3D) {
            player_ShowTurn(DIR_RIGHT);
        }
        --viewer.UPDATE;
        viewer_draw_game();
        return;
    } else if (A == DIR_AROUND) {
        /* about face */
        player.PDIR = (dodBYTE)(player.PDIR + 2);
        player.PDIR = (dodBYTE)(player.PDIR & 3);
        if (viewer.display_mode == MODE_3D) {
            player_ShowTurn(DIR_AROUND);
        }
        --viewer.UPDATE;
        viewer_draw_game();
        return;
    } else {
        parser_CMDERR();
        return;
    }
}

/* Turning animation: sweep a vertical line across the 3D view.  The
 * port redrew the identical frame on every CLOCK tick inside its
 * turnDelay wait; one draw per sweep step is pixel-identical. */
void player_ShowTurn(dodBYTE A)
{
    dodBYTE ctr, x, times;
    int16_t offset, dir, lx;

    offset = 8;
    dir = 1;
    times = 1;
    switch (A) {
    case DIR_LEFT:
        offset = 8;
        dir = 1;
        times = 1;
        break;
    case DIR_RIGHT:
        offset = 248;
        dir = -1;
        times = 1;
        break;
    case DIR_AROUND:
        offset = 248;
        dir = -1;
        times = 2;
        break;
    default:
        break;
    }

    viewer.VXSCAL = 0x80;
    viewer.VYSCAL = 0x80;
    viewer.RANGE = 0;
    viewer_SETFAD();
    player.turning = 1;
    for (ctr = 0; ctr < times; ++ctr) {
        for (x = 0; x < TURN_LINES; ++x) {
            lx = (int16_t)((int16_t)x * TURN_INC * dir + offset);
            plat_clear();
            viewer_drawVectorList(VEC_LINES);
            plat_draw_line(lx, TURN_Y0, lx, TURN_Y1, viewer.VCTFAD,
                           viewer.INVFLG ? PLAT_LINE_INVERSE : 0u);
            viewer_drawArea(&viewer.TXTSTS);
            viewer_drawArea(&viewer.TXTPRI);
            plat_present();
            if (core_wait_jiffies_abortable(WAIT_TURN)) {
                player.turning = 0;
                return;
            }
        }
    }
    player.turning = 0;
    --player.HEARTF;
}

/* Attempts to move the player in the given (relative) direction. */
uint8_t player_PSTEP(dodBYTE dir)
{
    dodBYTE B;

    B = (dodBYTE)(dir + player.PDIR);
    B &= 3;
    if (dungeon_STEPOK(player.PROW, player.PCOL, B)) {
        player.PROW = (dodBYTE)(player.PROW + STPTAB[B * 2]);
        player.PCOL = (dodBYTE)(player.PCOL + STPTAB[(B * 2) + 1]);
        return 1;
    } else {
        /* do thud sound (blocking, as the port) */
        snd_play(SND_THUD, SND_VOL_MAX);
        snd_wait_done();
        return 0;
    }
}
