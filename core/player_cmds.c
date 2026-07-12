/* player_cmds.c - PATTK.ASM, PGET.ASM, PEXAM.ASM, PLOOK.ASM, PUSE.ASM,
 * PINCAN.ASM, PREVEA.ASM, PZTAPE.ASM: the non-movement player command
 * handlers.  (The ShowTurn sweep animation lives with its only callers
 * in player_move.c to keep this file under the 800-line limit.)
 *
 * Extracted from the C++ port's Player class (player.cpp).  Wait-site
 * conversions are documented in docs/wait-site-audit.md.
 *
 * ms -> jiffy conversions in this file:
 *   wizDelay = 500 ms pauses -> WAIT_WIZARD (30 jiffies); the port's
 *     loops here were raw SDL_GetTicks spins that did NOT tick CLOCK -
 *     core_wait_jiffies does, which only advances the heartbeat
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

/* port viewer.exps ("16F7B0"): 5-bit packed damage-flash string, only
 * ever printed by PATTK - private here per extraction rule 9 */
static const dodBYTE EXPS[3] = { 0x16, 0xF7, 0xB0 };

/* The port's `while (Mix_Playing()) { CLOCK-if-due; demo-abort ->
 * return }` idiom (same helper shape as creature.c); returns 1 when the
 * caller must bail out because the demo was aborted mid-sound. */
static uint8_t snd_wait_abortable(void)
{
    while (snd_playing()) {
        sched.curTime = plat_jiffies();
        if ((jiffy_t)(sched.curTime - sched.TCBLND[0].next_time) < 0x8000u) {
            sched_CLOCK();
            if (game.AUTFLG && game.demoRestart == 0) {
                return 1;
            }
        }
        plat_yield();
    }
    return 0;
}

/* PATTK.ASM: processes ATTACK command. */
void player_PATTK(void)
{
    int8_t res, idx, cidx, optr;
    OCB *U;
    dodBYTE r, c, val;
    RowCol rc;

    res = parser_PARHND();
    if (res == -1) {
        return;
    }

    if (res == 0) {
        idx = player.PLHAND;
        if (idx == DOD_NONE) {
            U = &player.EMPHND;
        } else {
            U = &object.OCBLND[idx];
        }
    } else {
        idx = player.PRHAND;
        if (idx == DOD_NONE) {
            U = &player.EMPHND;
        } else {
            U = &object.OCBLND[idx];
        }
    }

    PMGO = U->P_OCMGO;
    PPHO = U->P_OCPHO;
    PDAM = (dodSHORT)(PDAM + (dodSHORT)
           (((uint32_t)PPOW * (((uint32_t)PMGO + (uint32_t)PPHO) / 8u)) >> 7));

    /* make sound for appropriate object */
    snd_play(objSound[U->obj_type], SND_VOL_MAX);
    if (snd_wait_abortable()) {
        return;
    }

    /* incanted rings lose a charge per swing */
    if (U->obj_id >= OBJ_RING_ENERGY && U->obj_id <= OBJ_RING_FIRE) {
        --U->P_OCXX0;
        if (U->P_OCXX0 == 0) {
            U->obj_id = OBJ_RING_GOLD;
            object_OCBFIL(OBJ_RING_GOLD, idx);
            U->obj_reveal_lvl = 0;
            viewer_STATUS();
        }
    }

    rc.row = player.PROW;
    rc.col = player.PCOL;
    cidx = creature_CFIND2(rc);
    if (cidx == DOD_NONE) {
        player_HUPDAT();
        return;
    }

    if (!player_ATTACK(player.PLRBLK.P_ATPOW,
                       creature.CCBLND[cidx].P_CCPOW,
                       creature.CCBLND[cidx].P_CCDAM)) {
        player_HUPDAT();
        return;
    }

    /* fighting blind (no live torch) only lands 1 in 4 */
    if (player.PTORCH == DOD_NONE ||
        object.OCBLND[player.PTORCH].obj_id == OBJ_TORCH_DEAD) {
        if ((rng_RANDOM() & 3) != 0) {
            player_HUPDAT();
            return;
        }
    }

    /* make KLINK sound */
    snd_play(SND_KLINK, SND_VOL_MAX);
    if (snd_wait_abortable()) {
        return;
    }

    viewer_OUTSTI(EXPS);

    /* do damage */
    if (player_DAMAGE(player.PLRBLK.P_ATPOW, player.PLRBLK.P_ATMGO,
                      player.PLRBLK.P_ATPHO,
                      creature.CCBLND[cidx].P_CCPOW,
                      creature.CCBLND[cidx].P_CCMGD,
                      creature.CCBLND[cidx].P_CCPHD,
                      &creature.CCBLND[cidx].P_CCDAM)) {
        /* creature still alive */
        player_HUPDAT();
        return;
    }

    /* creature killed: drop its carried objects in its cell */
    optr = creature.CCBLND[cidx].P_CCOBJ;
    while (optr != DOD_NONE) {
        object.OCBLND[optr].P_OCOWN = 0;
        object.OCBLND[optr].P_OCROW = creature.CCBLND[cidx].P_CCROW;
        object.OCBLND[optr].P_OCCOL = creature.CCBLND[cidx].P_CCCOL;
        optr = object.OCBLND[optr].P_OCPTR;
    }

    --creature.CMXLND[creature.CMXPTR + creature.CCBLND[cidx].creature_id];
    creature.CCBLND[cidx].P_CCUSE = 0;
    viewer_PUPDAT();

    /* do loud explosion sound */
    snd_play(SND_BANG, SND_VOL_MAX);
    if (snd_wait_abortable()) {
        return;
    }

    PPOW = (dodSHORT)(PPOW + (creature.CCBLND[cidx].P_CCPOW >> 3));
    if ((PPOW & 0x8000u) != 0) {
        PPOW = 0x7FFF;
    }

    if (creature.CCBLND[cidx].creature_id == CRT_WIZIMG) {
        /* Wizard's Image killed: transport to 4th level.
         * Pause so the player can see the scroll (port: raw spin). */
        core_wait_jiffies(WAIT_WIZARD);
        /* (port flushed the SDL event queue here - shell concern) */
        viewer_clearArea(&viewer.TXTSTS);
        viewer_clearArea(&viewer.TXTPRI);
        /* (port fade-loop scaffolding done/fadeVal and the float scale
         * shadows are dropped; viewer_ShowFade drives the fade) */
        viewer.VCTFAD = 32;
        viewer.VXSCAL = 0x80;
        viewer.VYSCAL = 0x80;
        (void)viewer_ShowFade(FADE_MIDDLE);

        player.BAGPTR = player.PTORCH;
        if (player.PTORCH != DOD_NONE) {
            object.OCBLND[player.PTORCH].P_OCPTR = DOD_NONE;
        }

        player.POBJWT = 200;
        game.LEVEL = 3;
        creature_NEWLVL();

        do {
            c = (dodBYTE)(rng_RANDOM() & 31);
            r = (dodBYTE)(rng_RANDOM() & 31);
            val = dungeon.MAZLND[dungeon_RC2IDX(r, c)];
        } while (val == 0xFF);

        player.PROW = r;
        player.PCOL = c;

        game_INIVU();
    }

    if (creature.CCBLND[cidx].creature_id != CRT_WIZARD) {
        player_HUPDAT();
        return;
    }

    /* killed the real Wizard */
    --creature.FRZFLG;
    player.PRLITE = 0x07;
    player.PMLITE = 0x13;
    object.OCBPTR = 1;
    player.BAGPTR = DOD_NONE;
    player.PTORCH = DOD_NONE;
    player.PRHAND = DOD_NONE;
    player.PLHAND = DOD_NONE;
    game_INIVU();
    player_HUPDAT();
    return;
}

/* PGET.ASM: processes GET command. */
void player_PGET(void)
{
    int8_t res, idx;
    uint8_t match;
    RowCol rc;

    res = parser_PARHND();
    if (res == -1) {
        return;
    }
    if (res == 0 && player.PLHAND != DOD_NONE) {
        parser_CMDERR();
        return;
    }
    if (res == 1 && player.PRHAND != DOD_NONE) {
        parser_CMDERR();
        return;
    }
    if (!object_PAROBJ()) {
        return;
    }

    match = 0;
    object.OFINDF = 0;
    do {
        rc.row = player.PROW;
        rc.col = player.PCOL;
        idx = object_OFIND(rc);
        if (idx == DOD_NONE) {
            parser_CMDERR();
            return;
        }
        if (object.SPEFLG == 0) {
            if (object.OBJCLS == object.OCBLND[idx].obj_type) {
                match = 1;
            }
        } else {
            if (object.OBJTYP == object.OCBLND[idx].obj_id) {
                match = 1;
            }
        }
    } while (match == 0);

    if (res == 0) {
        player.PLHAND = idx;
    } else {
        player.PRHAND = idx;
    }
    ++object.OCBLND[idx].P_OCOWN;

    player.POBJWT = (dodSHORT)(player.POBJWT +
                               OBJWGT[object.OCBLND[idx].obj_type]);
    player_HUPDAT();

    viewer_STATUS();
    viewer_PUPDAT();
}

/* Processes DROP command. */
void player_PDROP(void)
{
    int8_t res, idx;

    res = parser_PARHND();
    if (res == -1) {
        return;
    }
    if (res == 0 && player.PLHAND == DOD_NONE) {
        parser_CMDERR();
        return;
    }
    if (res == 1 && player.PRHAND == DOD_NONE) {
        parser_CMDERR();
        return;
    }

    if (res == 0) {
        idx = player.PLHAND;
        player.PLHAND = DOD_NONE;
    } else {
        idx = player.PRHAND;
        player.PRHAND = DOD_NONE;
    }

    object.OCBLND[idx].P_OCOWN = 0;
    object.OCBLND[idx].P_OCROW = player.PROW;
    object.OCBLND[idx].P_OCCOL = player.PCOL;
    object.OCBLND[idx].P_OCLVL = game.LEVEL;

    player.POBJWT = (dodSHORT)(player.POBJWT -
                               OBJWGT[object.OCBLND[idx].obj_type]);
    player_HUPDAT();

    viewer_STATUS();
    viewer_PUPDAT();
}

/* PEXAM.ASM: processes EXAMINE command. */
void player_PEXAM(void)
{
    viewer.display_mode = MODE_EXAMINE;
    viewer_PUPDAT();
}

/* PLOOK.ASM: processes LOOK command. */
void player_PLOOK(void)
{
    viewer.display_mode = MODE_3D;
    viewer_PUPDAT();
}

/* Processes STOW command. */
void player_PSTOW(void)
{
    int8_t res;

    res = parser_PARHND();
    if (res == -1) {
        return;
    }
    if (res == 0 && player.PLHAND == DOD_NONE) {
        parser_CMDERR();
        return;
    }
    if (res == 1 && player.PRHAND == DOD_NONE) {
        parser_CMDERR();
        return;
    }

    if (res == 0) {
        object.OCBLND[player.PLHAND].P_OCPTR = player.BAGPTR;
        player.BAGPTR = player.PLHAND;
        player.PLHAND = DOD_NONE;
    } else {
        object.OCBLND[player.PRHAND].P_OCPTR = player.BAGPTR;
        player.BAGPTR = player.PRHAND;
        player.PRHAND = DOD_NONE;
    }
    viewer_STATUS();
    viewer_PUPDAT();
}

/* Processes PULL command. */
void player_PPULL(void)
{
    int8_t res;
    uint8_t onHead, match;
    int8_t curPtr, prevPtr;

    if (player.BAGPTR == DOD_NONE) {
        parser_CMDERR();
        return;
    }

    res = parser_PARHND();
    if (res == -1) {
        return;
    }
    if (res == 0 && player.PLHAND != DOD_NONE) {
        parser_CMDERR();
        return;
    }
    if (res == 1 && player.PRHAND != DOD_NONE) {
        parser_CMDERR();
        return;
    }
    if (!object_PAROBJ()) {
        return;
    }

    onHead = 1;
    match = 0;
    curPtr = DOD_NONE;
    prevPtr = DOD_NONE;

    do {
        if (onHead) {
            curPtr = player.BAGPTR;
        } else {
            prevPtr = curPtr;
            curPtr = object.OCBLND[curPtr].P_OCPTR;
            if (curPtr == DOD_NONE) {
                parser_CMDERR();
                return;
            }
        }

        if (object.SPEFLG == 0) {
            if (object.OCBLND[curPtr].obj_type == object.OBJCLS) {
                match = 1;
            }
        } else {
            if (object.OCBLND[curPtr].obj_id == object.OBJTYP) {
                match = 1;
            }
        }
        if (match) {
            break;
        }
        if (onHead) {
            onHead = 0;
        }
    } while (1);

    if (onHead) {
        player.BAGPTR = object.OCBLND[curPtr].P_OCPTR;
    } else {
        object.OCBLND[prevPtr].P_OCPTR = object.OCBLND[curPtr].P_OCPTR;
    }

    if (res == 0) {
        player.PLHAND = curPtr;
    } else {
        player.PRHAND = curPtr;
    }

    if (curPtr == player.PTORCH) {
        player.PTORCH = DOD_NONE;
    }

    viewer_STATUS();
    viewer_PUPDAT();
}

/* PREVEA.ASM: processes REVEAL command. */
void player_PREVEA(void)
{
    int8_t res, idx;
    dodBYTE req;

    res = parser_PARHND();
    if (res == -1) {
        return;
    }
    if (res == 0 && player.PLHAND == DOD_NONE) {
        return;
    }
    if (res == 1 && player.PRHAND == DOD_NONE) {
        return;
    }
    if (res == 0) {
        idx = player.PLHAND;
    } else {
        idx = player.PRHAND;
    }
    req = object.OCBLND[idx].obj_reveal_lvl;
    if (req && (((dodSHORT)(req * 25) <= PPOW) ||
                (req == 50 && game.VisionScroll && 400 <= PPOW))) {
        object_OCBFIL(object.OCBLND[idx].obj_id, idx);
        object.OCBLND[idx].obj_reveal_lvl = 0;
        viewer_STATUS();
    }
}

/* PINCAN.ASM: processes INCANT command. */
void player_PINCAN(void)
{
    int8_t res;
    dodBYTE A, B;

    res = parser_PARSER(ADJTAB, &A, &B, 1);
    if (res <= 0) {
        return;
    }

    if (parser.FULFLG == 0) {
        return;
    }

    object.OBJTYP = A;
    object.OBJCLS = B;

    if (player.PLHAND != DOD_NONE) {
        if (object.OCBLND[player.PLHAND].obj_type == OBJT_RING) {
            if (object.OCBLND[player.PLHAND].P_OCXX1 == object.OBJTYP) {
                object.OCBLND[player.PLHAND].obj_id = object.OBJTYP;
                object_OCBFIL(object.OBJTYP, player.PLHAND);

                /* make ring sound */
                snd_play(objSound[object.OCBLND[player.PLHAND].obj_type],
                         SND_VOL_MAX);
                snd_wait_done();

                viewer_STATUS();
                viewer_PUPDAT();
                object.OCBLND[player.PLHAND].P_OCXX1 = (dodSHORT)0xFFFFu;
                if (object.OBJTYP == OBJ_RING_FINAL) {   /* 0x12: winner */
                    /* (port flushed the SDL event queue - shell) */
                    /* pause so the player can see the status line
                     * (port: raw spin) */
                    core_wait_jiffies(WAIT_WIZARD);
                    viewer_clearArea(&viewer.TXTSTS);
                    viewer_clearArea(&viewer.TXTPRI);
                    (void)viewer_ShowFade(FADE_VICTORY);
                    game.hasWon = 1;
                } else {
                    return;
                }
            }
        }
    }

    if (player.PRHAND != DOD_NONE) {
        if (object.OCBLND[player.PRHAND].obj_type == OBJT_RING) {
            if (object.OCBLND[player.PRHAND].P_OCXX1 == object.OBJTYP) {
                object.OCBLND[player.PRHAND].obj_id = object.OBJTYP;
                object_OCBFIL(object.OBJTYP, player.PRHAND);

                /* make ring sound */
                snd_play(objSound[object.OCBLND[player.PRHAND].obj_type],
                         SND_VOL_MAX);
                snd_wait_done();

                viewer_STATUS();
                viewer_PUPDAT();
                object.OCBLND[player.PRHAND].P_OCXX1 = (dodSHORT)0xFFFFu;
                if (object.OBJTYP == OBJ_RING_FINAL) {   /* 0x12: winner */
                    /* (port flushed the SDL event queue - shell) */
                    core_wait_jiffies(WAIT_WIZARD);
                    viewer_clearArea(&viewer.TXTSTS);
                    viewer_clearArea(&viewer.TXTPRI);
                    (void)viewer_ShowFade(FADE_VICTORY);
                    game.hasWon = 1;
                } else {
                    return;
                }
            }
        }
    }
}

/* PUSE.ASM: processes USE command. */
void player_PUSE(void)
{
    int8_t res, idx;

    res = parser_PARHND();
    if (res == -1) {
        return;
    }
    if (res == 0 && player.PLHAND == DOD_NONE) {
        return;
    }
    if (res == 1 && player.PRHAND == DOD_NONE) {
        return;
    }
    if (res == 0) {
        idx = player.PLHAND;
    } else {
        idx = player.PRHAND;
    }

    if (object.OCBLND[idx].obj_type == OBJT_TORCH) {
        player.PTORCH = idx;
        if (res == 0) {
            object.OCBLND[player.PLHAND].P_OCPTR = player.BAGPTR;
            player.BAGPTR = player.PLHAND;
            player.PLHAND = DOD_NONE;
        } else {
            object.OCBLND[player.PRHAND].P_OCPTR = player.BAGPTR;
            player.BAGPTR = player.PRHAND;
            player.PRHAND = DOD_NONE;
        }
        viewer_STATUS();
        viewer_PUPDAT();

        /* make torch sound */
        snd_play(objSound[object.OCBLND[idx].obj_type], SND_VOL_MAX);
        snd_wait_done();

        viewer_PUPDAT();
        return;
    } else if (object.OCBLND[idx].obj_id == OBJ_FLASK_THEWS) {
        PPOW = (dodSHORT)(PPOW + 1000);
        object.OCBLND[idx].obj_id = OBJ_FLASK_EMPTY;
        object.OCBLND[idx].obj_reveal_lvl = 0;

        /* make flask sound */
        snd_play(objSound[object.OCBLND[idx].obj_type], SND_VOL_MAX);
        snd_wait_done();

        viewer_STATUS();
        player_HUPDAT();
    } else if (object.OCBLND[idx].obj_id == OBJ_FLASK_HALE) {
        PDAM = 0;
        object.OCBLND[idx].obj_id = OBJ_FLASK_EMPTY;
        object.OCBLND[idx].obj_reveal_lvl = 0;

        /* make flask sound */
        snd_play(objSound[object.OCBLND[idx].obj_type], SND_VOL_MAX);
        snd_wait_done();

        viewer_STATUS();
        player_HUPDAT();
    } else if (object.OCBLND[idx].obj_id == OBJ_FLASK_ABYE) {
        /* port: PDAM += (short)((double)PPOW * 0.8); PPOW*4/5 is equal
         * for every dodSHORT PPOW (0.8 rounds up by ~4e-17, never enough
         * to cross an integer).  PUSE.ASM used SCAL16(PPOW,102) =
         * PPOW*102/128 - port divergence kept, per rule 11. */
        PDAM = (dodSHORT)(PDAM + (dodSHORT)(((uint32_t)PPOW * 4u) / 5u));

        object.OCBLND[idx].obj_id = OBJ_FLASK_EMPTY;
        object.OCBLND[idx].obj_reveal_lvl = 0;

        /* make flask sound */
        snd_play(objSound[object.OCBLND[idx].obj_type], SND_VOL_MAX);
        snd_wait_done();

        viewer_STATUS();
        player_HUPDAT();
    } else if (object.OCBLND[idx].obj_id == OBJ_SCROLL_SEER) {
        viewer.showSeerMap = 1;
        if (object.OCBLND[idx].obj_reveal_lvl != 0) {
            return;
        }

        /* make scroll sound */
        snd_play(objSound[object.OCBLND[idx].obj_type], SND_VOL_MAX);
        snd_wait_done();

        player.HEARTF = 0;
        viewer.display_mode = MODE_MAP;
        viewer_PUPDAT();
        return;
    } else if (object.OCBLND[idx].obj_id == OBJ_SCROLL_VISION) {
        viewer.showSeerMap = 0;
        if (object.OCBLND[idx].obj_reveal_lvl != 0) {
            return;
        }

        /* make scroll sound */
        snd_play(objSound[object.OCBLND[idx].obj_type], SND_VOL_MAX);
        snd_wait_done();

        player.HEARTF = 0;
        viewer.display_mode = MODE_MAP;
        viewer_PUPDAT();
        return;
    }
}

/* --- PZTAPE.ASM: ZSAVE / ZLOAD ---------------------------------------
 * The port built a filename from the token and signalled the scheduler
 * via ZFLAG; Scheduler::SAVE/LOAD then did fstream I/O.  Core: one save
 * slot through plat_save_state/plat_load_state.  The blob is a straight
 * byte copy of the flat state structs the port's SAVE dumped (game,
 * player, object, creature, dungeon, rng) plus the viewer light/scale
 * scalars.  The token is still parsed - and discarded - so the command
 * grammar matches the port.  Timing note: the port serialized at the
 * END of the scheduler pass (SAVE) / after pump exit (LOAD); here the
 * blob is built/applied inside the handler - see the audit doc. */

#define SAVE_VIEWER_SCALARS 6u

#define SAVE_BLOB_LEN (sizeof(game_state) + sizeof(player_state) + \
                       sizeof(object_state) + sizeof(creature_state) + \
                       sizeof(dungeon_state) + sizeof(rng_state) + \
                       SAVE_VIEWER_SCALARS)

static uint8_t SAVBUF[SAVE_BLOB_LEN];
static uint16_t savoff;

static void sav_put(const void *src, uint16_t n)
{
    const uint8_t *s = (const uint8_t *)src;
    uint16_t i;
    for (i = 0; i < n; ++i) {
        SAVBUF[savoff + i] = s[i];
    }
    savoff = (uint16_t)(savoff + n);
}

static void sav_get(void *dst, uint16_t n)
{
    uint8_t *d = (uint8_t *)dst;
    uint16_t i;
    for (i = 0; i < n; ++i) {
        d[i] = SAVBUF[savoff + i];
    }
    savoff = (uint16_t)(savoff + n);
}

static void save_build_blob(void)
{
    savoff = 0;
    sav_put(&game, (uint16_t)sizeof(game_state));
    sav_put(&player, (uint16_t)sizeof(player_state));
    sav_put(&object, (uint16_t)sizeof(object_state));
    sav_put(&creature, (uint16_t)sizeof(creature_state));
    sav_put(&dungeon, (uint16_t)sizeof(dungeon_state));
    sav_put(&rng, (uint16_t)sizeof(rng_state));
    sav_put(&viewer.RLIGHT, 1);
    sav_put(&viewer.MLIGHT, 1);
    sav_put(&viewer.OLIGHT, 1);
    sav_put(&viewer.VXSCAL, 1);
    sav_put(&viewer.VYSCAL, 1);
    sav_put(&viewer.TXBFLG, 1);
}

static void load_apply_blob(void)
{
    savoff = 0;
    sav_get(&game, (uint16_t)sizeof(game_state));
    sav_get(&player, (uint16_t)sizeof(player_state));
    sav_get(&object, (uint16_t)sizeof(object_state));
    sav_get(&creature, (uint16_t)sizeof(creature_state));
    sav_get(&dungeon, (uint16_t)sizeof(dungeon_state));
    sav_get(&rng, (uint16_t)sizeof(rng_state));
    sav_get(&viewer.RLIGHT, 1);
    sav_get(&viewer.MLIGHT, 1);
    sav_get(&viewer.OLIGHT, 1);
    sav_get(&viewer.VXSCAL, 1);
    sav_get(&viewer.VYSCAL, 1);
    sav_get(&viewer.TXBFLG, 1);
}

/* Processes ZLOAD command. */
void player_PZLOAD(void)
{
    uint8_t i;

    for (i = 0; i < 33; ++i) {
        parser.TOKEN[i] = 0xFF;
    }
    (void)parser_GETTOK();   /* consume the save-name token (one slot) */

    if (plat_load_state(SAVBUF, (uint16_t)SAVE_BLOB_LEN) != PLAT_OK) {
        parser_CMDERR();     /* port: named save file did not exist */
        return;
    }
    load_apply_blob();
    --sched.ZFLAG;   /* 0xFF -> PUMP_LOAD_ABANDON -> game_LoadGame */
}

/* Processes ZSAVE command. */
void player_PZSAVE(void)
{
    uint8_t i;

    for (i = 0; i < 33; ++i) {
        parser.TOKEN[i] = 0xFF;
    }
    (void)parser_GETTOK();   /* consume the save-name token (one slot) */

    save_build_blob();
    (void)plat_save_state(SAVBUF, (uint16_t)SAVE_BLOB_LEN);
    ++sched.ZFLAG;
}
