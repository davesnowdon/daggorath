/* player.c - HUMAN.ASM / COMPLR.ASM / HUPDAT.ASM: player state, the
 * PLAYER input task, HUMAN command dispatch, heartbeat (HSLOW/HUPDAT),
 * torch burn (BURNER) and the shared combat math (ATTACK/DAMAGE).
 *
 * Extracted from the C++ port's Player class (player.cpp); movement
 * handlers live in player_move.c, the other command handlers in
 * player_cmds.c.  Every blocking-wait conversion is documented in
 * docs/wait-site-audit.md.
 *
 * ms -> jiffy conversions in this file:
 *   750 ms faint fade step      -> WAIT_FAINT_STEP (45 jiffies)
 *   HEARTR*17 ms HSLOW reperiod -> HEARTR jiffies (17 ms = 1 jiffy)
 *
 * Enhancements NOT extracted (authentic mode): g_cheats (CHEAT_ITEMS/
 * CHEAT_TORCH), PreTranslateCommand (SETOPT/SETCHEAT/RESTART console),
 * RandomMaze seeding, CreaturesInstaRegen.
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
#include "rng.h"
#include "platform.h"

player_state player;

void player_Reset(void)
{
    player.PROW = 12;
    player.PCOL = 22;
    PPOW = (dodSHORT)((0x17 << 8) | 160);
    player.POBJWT = 35;
    player.FAINT = 0;
    player.PRLITE = 0;
    player.PMLITE = 0;
    player.PLHAND = DOD_NONE;
    player.PRHAND = DOD_NONE;
    player.PTORCH = DOD_NONE;
    player.BAGPTR = DOD_NONE;
    player.EMPHND.obj_type = OBJT_WEAPON;
    player.EMPHND.obj_reveal_lvl = 0;
    player.EMPHND.P_OCMGO = 0;
    player.EMPHND.P_OCPHO = 5;
    PDAM = 0;
    player.PDIR = 0;
    player.HEARTF = 0;
    player.HEARTC = 0;
    player.HEARTR = 0;
    player.HEARTS = 0;
    player.HBEATF = 0;
    player.turning = 0;
}

/* Used during initialization to set data for either the built-in demo
 * or starting a live game.  isDemo nonzero = attract-mode loadout
 * (the port passes game.AUTFLG). */
void player_setInitialObjects(uint8_t isDemo)
{
    int8_t x, y;

    if (isDemo) {
        game.IsDemo = 1;
        game.LEVEL = 2;
        dungeon_SetLEVTABOrig();
        /* demo: iron sword, pine torch, leather shield */
        x = object_OBIRTH(OBJ_SWORD_IRON, 0);
        ++object.OCBLND[x].P_OCOWN;
        object_OCBFIL(OBJ_SWORD_IRON, x);
        object.OCBLND[x].obj_reveal_lvl = 0;
        player.BAGPTR = x;
        y = x;

        x = object_OBIRTH(OBJ_TORCH_PINE, 0);
        ++object.OCBLND[x].P_OCOWN;
        object_OCBFIL(OBJ_TORCH_PINE, x);
        object.OCBLND[x].obj_reveal_lvl = 0;
        object.OCBLND[y].P_OCPTR = x;
        y = x;

        x = object_OBIRTH(OBJ_SHIELD_LEATHER, 0);
        ++object.OCBLND[x].P_OCOWN;
        object_OCBFIL(OBJ_SHIELD_LEATHER, x);
        object.OCBLND[x].obj_reveal_lvl = 0;
        object.OCBLND[y].P_OCPTR = x;
    } else {
        game.IsDemo = 0;
        game.LEVEL = 0;
        /* authentic maze only (RandomMaze enhancement not extracted) */
        dungeon_SetLEVTABOrig();
        player.PROW = 0x10;
        player.PCOL = 0x0B;
        player.PLRBLK.P_ATPOW = 160;

        /* starting loadout: pine torch (bag head), wooden sword */
        x = object_OBIRTH(OBJ_TORCH_PINE, 0);
        ++object.OCBLND[x].P_OCOWN;
        object_OCBFIL(OBJ_TORCH_PINE, x);
        object.OCBLND[x].obj_reveal_lvl = 0;
        player.BAGPTR = x;
        y = x;

        x = object_OBIRTH(OBJ_SWORD_WOOD, 0);
        ++object.OCBLND[x].P_OCOWN;
        object_OCBFIL(OBJ_SWORD_WOOD, x);
        object.OCBLND[x].obj_reveal_lvl = 0;
        object.OCBLND[y].P_OCPTR = x;
    }
}

/* COMPLR.ASM PLAYER: called from the scheduler as often as possible;
 * retrieves keyboard input, or commands from the demo data. */
int8_t player_PLAYER(void)
{
    dodBYTE tokCnt, tokCtr;
    dodBYTE objstr[10];
    const dodBYTE *X;
    dodBYTE *U;
    int16_t Xup;
    dodBYTE c;

    sched.TCBLND[TID_PLAYER].next_time =
        (jiffy_t)(sched.curTime + sched.TCBLND[TID_PLAYER].frequency);

    if (game.AUTFLG == 0) {
        /* process keyboard buffer (ring carries raw ASCII, as the port) */
        do {
            c = parser_KBDGET();
            if (c == 0) {
                return 0;
            }
            if (player.FAINT != 0) {
                while (parser_KBDGET() != 0)
                    ;   /* drain the buffer while fainted */
                return 0;
            }

            /* convert from ASCII to internal codes */
            if (c >= 'A' && c <= 'Z') {
                c &= 0x1F;
            } else if (c == C_BS) {
                c = I_BS;
            } else if (c == C_CR) {
                c = I_CR;
            } else {
                c = I_SP;
            }

            if (!player_HUMAN(c)) {
                return -1;
            }
        } while (1);
    } else {
        /* process autoplay commands */
        tokCnt = GAME_DEMO_CMDS[game.DEMOPTR++];
        if (tokCnt == 0) {
            game_WAIT();
            game_WAIT();
            game.hasWon = 1;
            game.demoRestart = 1;
            return 0;
        }

        /* feed next autoplay command to HUMAN */
        tokCtr = 1;
        do {
            if (tokCtr == 1) {
                X = &CMDTAB[GAME_DEMO_CMDS[game.DEMOPTR]];
            } else if (tokCtr == 2) {
                X = &DIRTAB[GAME_DEMO_CMDS[game.DEMOPTR]];
            } else {
                X = &GENTAB[GAME_DEMO_CMDS[game.DEMOPTR]];
            }
            ++game.DEMOPTR;
            U = &objstr[1];
            parser_EXPAND(X, &Xup, U);
            ++U;
            game_WAIT();
            do {
                player_HUMAN(*U);
                ++U;
            } while (*U != 0xFF);
            player_HUMAN(I_SP);
            ++tokCtr;
        } while (tokCtr <= tokCnt);
        --viewer.UPDATE;
        viewer_draw_game();
        player_HUMAN(I_CR);
    }

    return 0;
}

/* HUMAN.ASM: buffer one keystroke; on CR, parse and dispatch. */
uint8_t player_HUMAN(dodBYTE c)
{
    int8_t res;
    dodBYTE A, B;

    /* check if we are displaying the map */
    if (player.HEARTF == 0) {
        game_INIVU();
        viewer_PROMPT();
    }
    if (c == I_CR) {
carriage_return:
        viewer_OUTCHR(I_SP);
        parser.LINBUF[parser.LINPTR] = I_NULL;
        parser.LINBUF[parser.LINPTR + 1] = I_NULL;
        parser.LINPTR = 0;

        /* (port: PreTranslateCommand SETOPT/SETCHEAT/RESTART console
         * hook here - enhancement, not extracted) */

        /* dispatch to proper routine */
        res = parser_PARSER(&CMDTAB[0], &A, &B, 1);
        if (res == 1) {
            switch (A) {
            case CMD_ATTACK:  player_PATTK();  break;
            case CMD_CLIMB:   player_PCLIMB(); break;
            case CMD_DROP:    player_PDROP();  break;
            case CMD_EXAMINE: player_PEXAM();  break;
            case CMD_GET:     player_PGET();   break;
            case CMD_INCANT:  player_PINCAN(); break;
            case CMD_LOOK:    player_PLOOK();  break;
            case CMD_MOVE:    player_PMOVE();  break;
            case CMD_PULL:    player_PPULL();  break;
            case CMD_REVEAL:  player_PREVEA(); break;
            case CMD_STOW:    player_PSTOW();  break;
            case CMD_TURN:    player_PTURN();  break;
            case CMD_USE:     player_PUSE();   break;
            case CMD_ZLOAD:   player_PZLOAD(); break;
            case CMD_ZSAVE:   player_PZSAVE(); break;
            default:                           break;
            }
        }
        if (res == -1) {
            parser_CMDERR();
        }

        if ((player.HEARTF != 0) && (player.FAINT == 0)) {
            viewer_PROMPT();
        }

        parser.LINPTR = 0;
        return 1;
    }
    if (c == I_BS) {
        if (parser.LINPTR == 0) {
            return 1;
        }
        --parser.LINPTR;
        viewer_OUTSTR(M_ERAS);
        return 1;
    }
    /* buffer normal characters */
    viewer_OUTCHR(c);
    parser.LINBUF[parser.LINPTR] = c;
    ++parser.LINPTR;
    viewer_OUTSTR(M_CURS);
    if (parser.LINPTR >= 32) {
        goto carriage_return;
    }
    return 1;
}

/* HUPDAT.ASM HSLOW: runs at the current heart rate; damage recovery,
 * indicated by slowing the heartbeat. */
int8_t player_HSLOW(void)
{
    player.PLRBLK.P_ATDAM = (dodSHORT)(player.PLRBLK.P_ATDAM
                                       - (player.PLRBLK.P_ATDAM >> 6));
    if ((player.PLRBLK.P_ATDAM & 0x8000u) != 0) {
        player.PLRBLK.P_ATDAM = 0;
    }
    player_HUPDAT();

    /* port: next_time = curTime + HEARTR*17 ms; 17 ms = 1 jiffy */
    sched.TCBLND[TID_HRTSLOW].next_time =
        (jiffy_t)(sched.curTime + player.HEARTR);

    return 0;
}

/* HUPDAT.ASM: heartbeat from power level and damage; also processes
 * fainting and death.
 *
 *               PPOW * 64
 * HEARTR = ------------------- - 19
 *           PPOW + (PDAM * 2)
 *
 * The original division routine added one to the integer quotient so
 * that [(1/5) == 1], [(5/5) == 2], etc.; the formula below reflects
 * that peculiarity by only subtracting 18 (port comment).  The original
 * used 24-bit intermediates (PPOW*64 reaches 22 bits) -> uint32_t.
 * The formula itself is a pure helper so the A/B harness can drive it. */
dodBYTE player_heartr_formula(dodSHORT pow, dodSHORT dam)
{
    uint32_t quot = ((uint32_t)pow * 64u) /
                    ((uint32_t)pow + ((uint32_t)dam * 2u));
    return (dodBYTE)(quot - 18u);
}

void player_HUPDAT(void)
{
    player.HEARTR = player_heartr_formula(player.PLRBLK.P_ATPOW,
                                          player.PLRBLK.P_ATDAM);

    if (player.FAINT == 0) {
        /* not in a faint */
        if (player.HEARTR <= 3 || (player.HEARTR & 128u) != 0) {
            /* do faint */
            player.FAINT = 0xFF;
            viewer_clearArea(&viewer.TXTPRI);
            viewer.OLIGHT = viewer.RLIGHT;
            do {
                --viewer.MLIGHT;
                --viewer.UPDATE;
                viewer_draw_game();
                --viewer.RLIGHT;
                core_wait_jiffies(WAIT_FAINT_STEP);
            } while (viewer.RLIGHT != 248);     /* not equal to -8 */
            --viewer.UPDATE;
            parser.KBDHDR = 0;
            parser.KBDTAL = 0;
        }
    } else {
        /* in a faint */
        if (player.HEARTR >= 4 && (player.HEARTR & 128u) == 0) {
            /* do recover from faint */
            do {
                --viewer.UPDATE;
                viewer_draw_game();
                ++viewer.MLIGHT;
                ++viewer.RLIGHT;
                core_wait_jiffies(WAIT_FAINT_STEP);
            } while (viewer.RLIGHT != viewer.OLIGHT);
            player.FAINT = 0;
            viewer_PROMPT();
            --viewer.UPDATE;
        }
    }
    if (player.PLRBLK.P_ATPOW < player.PLRBLK.P_ATDAM) {
        /* do death (the port also flushed the SDL event queue - shell
         * concern; the pump reports PUMP_DEATH from the same compare) */
        viewer_clearArea(&viewer.TXTSTS);
        viewer_clearArea(&viewer.TXTPRI);
        (void)viewer_ShowFade(FADE_DEATH);
    }
}

/* BURNER: every five seconds, manage the lit torch's timers.  The full
 * burn time is stored in XX0 in 5-second units (15 min = 180); the
 * magical/physical light values (XX1/XX2) are in minutes - hence the
 * conversion half way through (port comment). */
int8_t player_BURNER(void)
{
    dodSHORT A;

    sched.TCBLND[TID_TORCHBURN].next_time =
        (jiffy_t)(sched.curTime + sched.TCBLND[TID_TORCHBURN].frequency);

    if (player.PTORCH == DOD_NONE) {
        --viewer.NEWLUK;
        return 0;
    }
    A = object.OCBLND[player.PTORCH].P_OCXX0;
    if (A == 0) {
        --viewer.NEWLUK;
        return 0;
    }
    --A;
    object.OCBLND[player.PTORCH].P_OCXX0 = A;

    /* convert A to minutes (round up) */
    if (A % 12u == 0) {
        A = (dodSHORT)(A / 12u);
    } else {
        A = (dodSHORT)((A / 12u) + 1u);
    }

    if (A <= 5) {
        object.OCBLND[player.PTORCH].obj_id = OBJ_TORCH_DEAD;
        object.OCBLND[player.PTORCH].obj_reveal_lvl = 0;
    }
    if (A < object.OCBLND[player.PTORCH].P_OCXX1) {
        object.OCBLND[player.PTORCH].P_OCXX1 = A;
    }
    if (A < object.OCBLND[player.PTORCH].P_OCXX2) {
        object.OCBLND[player.PTORCH].P_OCXX2 = A;
    }

    --viewer.NEWLUK;
    return 0;
}

/* PATTK.ASM ATTACK: determines if an attack strikes its target. */
uint8_t player_ATTACK(dodSHORT AP, dodSHORT DP, dodSHORT DD)
{
    int8_t T0 = 15;
    int8_t pidx;
    int16_t adjust, ret;
    int32_t Dval = ((int32_t)DP - (int32_t)DD) * 4;

    do {
        Dval -= AP;
        if (Dval < 0) {
            break;
        }
        --T0;
    } while (T0 > 0);

    pidx = (int8_t)(T0 - 3);
    if (pidx > 0) {
        adjust = (int16_t)(pidx * 10);
    } else {
        adjust = (int16_t)(pidx * 25);
    }

    ret = (int16_t)(rng_RANDOM() + adjust - 127);

    return (ret < 0) ? 0 : 1;
}

/* PATTK.ASM DAMAGE: calculates and assesses damage from a successful
 * attack; *DD is the defender's damage accumulator.  Returns nonzero
 * while the defender survives (DP > *DD). */
uint8_t player_DAMAGE(dodSHORT AP, dodBYTE AMO, dodBYTE APO,
                      dodSHORT DP, dodBYTE DMD, dodBYTE DPD,
                      dodSHORT *DD)
{
    uint32_t a;

    a = ((uint32_t)AP * AMO) >> 7;
    a = (a * DMD) >> 7;
    *DD = (dodSHORT)(*DD + (dodSHORT)a);

    a = ((uint32_t)AP * APO) >> 7;
    a = (a * DPD) >> 7;
    *DD = (dodSHORT)(*DD + (dodSHORT)a);

    return (DP > *DD) ? 1 : 0;
}
