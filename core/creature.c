/* creature.c - CRETUR.ASM / COMCRE.ASM: creature logic and movement.
 *
 * Extracted from the C++ port's src/creature.cpp; logic and iteration
 * order are byte-identical to the port, with types narrowed per
 * dod_types.h and time restored to 60 Hz jiffies.
 *
 * CDBTAB move/attack times: the port stored MILLISECONDS (original tenths
 * of a second * 100).  DTABAS.ASM CREXXX (lines 211-222) stores the delay
 * times as single bytes queued into the TENTH-second queue (COMCRE.ASM
 * "LDB #Q.TEN" after "LDA P.CCTMV,X"; same in CRETUR.ASM CMOV98), so one
 * unit = 0.1 s = 6 jiffies.  Conversion table (move, attack):
 *
 *   creature   port ms      DTABAS tenths   jiffies (tenths*6)
 *   SPIDER     2300, 1100   23, 11          138,  66
 *   VIPER      1500,  700   15,  7           90,  42
 *   GIANT1     2900, 2300   29, 23          174, 138
 *   BLOB       3100, 3100   31, 31          186, 186
 *   KNIGHT1    1300,  700   13,  7           78,  42
 *   GIANT2     1700, 1300   17, 13          102,  78
 *   SCORPION    500,  400    5,  4           30,  24
 *   KNIGHT2    1300,  700   13,  7           78,  42
 *   WRAITH      300,  300    3,  3           18,  18
 *   GALDROG     400,  300    4,  3           24,  18
 *   WIZIMG     1300,  700   13,  7           78,  42
 *   WIZARD     1300,  700   13,  7           78,  42
 *
 * (port ms = tenths*100, so jiffies = ms*6/100 exactly; no rounding.)
 */
#include "creature.h"
#include "creature_deps.h"
#include "game.h"
#include "object.h"
#include "sched.h"
#include "viewer.h"
#include "player.h"
#include "sound.h"
#include "rng.h"

creature_state creature;

/* CDBTAB: Creature Definition Blocks (DTABAS.ASM CDB macro).
 * {power, mag off, mag def, phys off, phys def, move jiffies, attack
 * jiffies} - never written (the port's UpdateCreSpeed menu hook that
 * rescaled these is an enhancement and was not extracted). */
static const CDB CDBTAB[CTYPES] = {
    {   32,   0, 255, 128, 255, 138,  66 },     /* SPIDER */
    {   56,   0, 255,  80, 128,  90,  42 },     /* VIPER */
    {  200,   0, 255,  52, 192, 174, 138 },     /* GIANT1 */
    {  304,   0, 255,  96, 167, 186, 186 },     /* BLOB */
    {  504,   0, 128,  96,  60,  78,  42 },     /* KNIGHT1 */
    {  704,   0, 128, 128,  48, 102,  78 },     /* GIANT2 */
    {  400, 255, 128, 255, 128,  30,  24 },     /* SCORPION */
    {  800,   0,  64, 255,   8,  78,  42 },     /* KNIGHT2 */
    {  800, 192,  16, 192,   8,  18,  18 },     /* WRAITH */
    { 1000, 255,   5, 255,   3,  24,  18 },     /* GALDROG */
    { 1000, 255,   6, 255,   0,  78,  42 },     /* WIZIMG */
    { 8000, 255,   6, 255,   0,  78,  42 }      /* WIZARD */
};

/* MOVTAB: preferential random movement table (CRETUR.ASM MOVTAB):
 * fwd/left/right, fwd/right/left, then back out */
static const dodBYTE MOVTAB[7] = { 0, 3, 1, 0, 1, 3, 0 };

/* creature_id -> SND id.  The port loaded creSound[i] from WAV files
 * whose filename numbers equal the SND_ ids (00_squeak .. 0B_bdlbdl). */
static const uint8_t creSound[CTYPES] = {
    SND_SQUEAK,  SND_RATTLE,  SND_GROWL,  SND_BEOOP,
    SND_KLANK,   SND_GRAWL,   SND_PSSST,  SND_KKLANK,
    SND_PSSHT,   SND_SNARL,   SND_BDLBDL1, SND_BDLBDL2
};

/* CMXLND initial creature counts, 5 levels x 12 types.  The VisionScroll
 * variant trades a level-0 viper for a blob (which carries the extra
 * vision scroll). */
static const dodBYTE CMXLND_STD[60] = {
    9, 9, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0,
    2, 4, 0, 6, 6, 6, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 4, 0, 6, 8, 4, 0, 0, 1, 0,
    0, 0, 0, 0, 0, 0, 8, 6, 6, 4, 0, 0,
    2, 2, 2, 2, 2, 2, 2, 4, 4, 8, 0, 1
};
static const dodBYTE CMXLND_VISION[60] = {
    9, 8, 4, 3, 0, 0, 0, 0, 0, 0, 0, 0,
    2, 4, 0, 6, 6, 6, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 4, 0, 6, 8, 4, 0, 0, 1, 0,
    0, 0, 0, 0, 0, 0, 8, 6, 6, 4, 0, 0,
    2, 2, 2, 2, 2, 2, 2, 4, 4, 8, 0, 1
};

static const CCB CCB_EMPTY = { .P_CCOBJ = DOD_NONE };

void creature_Reset(void)
{
    dodBYTE i;

    creature.CMXPTR = 0;
    creature.FRZFLG = 0;

    for (i = 0; i < 60; ++i)
    {
        creature.CMXLND[i] = game.VisionScroll ? CMXLND_VISION[i]
                                               : CMXLND_STD[i];
    }
}

/* NEWLVL.ASM: creates a new dungeon level, filling it with objects and
 * creatures.  (Port note: "should probably be moved to the Dungeon
 * class".) */
void creature_NEWLVL(void)
{
    dodBYTE a, b;
    int8_t  u, idx, tmp;

    creature.CMXPTR = (dodBYTE)(game.LEVEL * CTYPES);
    dungeon_CalcVFI();
    for (tmp = 0; tmp < 32; ++tmp)
    {
        creature.CCBLND[tmp] = CCB_EMPTY;
    }
    sched_SYSTCB();
    dungeon_DGNGEN();
    u = (int8_t)creature.CMXPTR;
    a = CTYPES - 1;
    do
    {
        b = creature.CMXLND[u + a];
        if (b != 0)
        {
            do
            {
                creature_CBIRTH(a);
                --b;
            } while (b != 0);
        }
        --a;
    } while (a != 0xFF);

    /* hand the level's carried objects to the creatures */
    u = -1;
    object.OFINDF = 0;
    for (;;)
    {
        idx = object_FNDOBJ();
        if (idx == DOD_NONE)
        {
            break;
        }
        if (object.OCBLND[idx].P_OCOWN == 0xFF)
        {
            for (;;)
            {
                ++u;
                if (u == 32)
                {
                    u = 0;
                }
                if (creature.CCBLND[u].P_CCUSE != 0)
                {
                    tmp = creature.CCBLND[u].P_CCOBJ;
                    creature.CCBLND[u].P_CCOBJ = idx;
                    object.OCBLND[idx].P_OCPTR = tmp;
                    break;
                }
            }
        }
    }

    /* determine video invert setting */
    viewer_setVidInv((game.LEVEL % 2) ? 1 : 0);
}

/* COMCRE.ASM CBIRTH: creates a new creature and places it in the maze */
void creature_CBIRTH(dodBYTE typ)
{
    int8_t   u, TCBindex;
    dodSHORT maz_idx;
    dodBYTE  rw, cl;

    u = -1;
    do
    {
        ++u;
    } while (creature.CCBLND[u].P_CCUSE != 0);
    --creature.CCBLND[u].P_CCUSE;       /* 0 -> 0xFF: in use */

    creature.CCBLND[u].creature_id = typ;
    creature.CCBLND[u].P_CCPOW = CDBTAB[typ].P_CDPOW;
    creature.CCBLND[u].P_CCMGO = CDBTAB[typ].P_CDMGO;
    creature.CCBLND[u].P_CCMGD = CDBTAB[typ].P_CDMGD;
    creature.CCBLND[u].P_CCPHO = CDBTAB[typ].P_CDPHO;
    creature.CCBLND[u].P_CCPHD = CDBTAB[typ].P_CDPHD;
    creature.CCBLND[u].P_CCTMV = CDBTAB[typ].P_CDTMV;
    creature.CCBLND[u].P_CCTAT = CDBTAB[typ].P_CDTAT;

    do
    {
        do
        {
            cl = (rng_RANDOM() & 31);
            rw = (rng_RANDOM() & 31);
            maz_idx = dungeon_RC2IDX(rw, cl);
        } while (dungeon_MAZLND[maz_idx] == 0xFF);
    } while (creature_CFIND(rw, cl) == 0);

    creature.CCBLND[u].P_CCROW = rw;
    creature.CCBLND[u].P_CCCOL = cl;

    TCBindex = sched_GETTCB();
    sched.TCBLND[TCBindex].data = u;
    sched.TCBLND[TCBindex].type = TID_CRTMOVE;
    sched.TCBLND[TCBindex].frequency = creature.CCBLND[u].P_CCTMV;
}

/* Checks for a creature in the given cell; returns 0 if occupied */
uint8_t creature_CFIND(dodBYTE rw, dodBYTE cl)
{
    int8_t ctr = 0;
    while (ctr < 32)
    {
        if (creature.CCBLND[ctr].P_CCROW == rw &&
            creature.CCBLND[ctr].P_CCCOL == cl)
        {
            if (creature.CCBLND[ctr].P_CCUSE != 0)
                return 0;
        }
        ++ctr;
    }
    return 1;
}

/* Checks for a creature in the given cell; returns its index or DOD_NONE */
int8_t creature_CFIND2(RowCol rc)
{
    int8_t ctr = 0;
    while (ctr < 32)
    {
        if (creature.CCBLND[ctr].P_CCROW == rc.row &&
            creature.CCBLND[ctr].P_CCCOL == rc.col)
        {
            if (creature.CCBLND[ctr].P_CCUSE != 0)
            {
                return ctr;
            }
        }
        ++ctr;
    }
    return DOD_NONE;
}

/* COMCRE.ASM CREGEN: called from the scheduler once every five minutes;
 * generates random new creatures.  (The port's CHEAT_REGEN_SCALING branch
 * was an enhancement and was not extracted; the authentic path below
 * matches the ASM: ANDA #7 / ADDA #2.) */
int8_t creature_CREGEN(void)
{
    dodBYTE X, B, A;

    /* update task's next_time */
    sched.TCBLND[TID_CRTREGEN].next_time =
        (jiffy_t)(plat_jiffies() +
                  sched.TCBLND[TID_CRTREGEN].frequency);

    X = creature.CMXPTR;
    B = CTYPES - 1;
    A = 0;
    do
    {
        A += creature.CMXLND[X + B];
        --B;
    } while (B != 255);
    if (A < 32)
    {
        A = rng_RANDOM();
        A &= 7;         /* (0-7) */
        A += 2;         /* (2-9) */
        ++creature.CMXLND[X + A];
    }
    return 0;
}

/* CRETUR.ASM CMOVE: called from the scheduler to move one creature.
 * Frequency is per creature type and its position relative to the
 * player. */
int8_t creature_CMOVE(int8_t task, int8_t cidx)
{
    int8_t   oidx, loop;
    dodBYTE  dir, X;
    uint8_t  doRandom = 0;
    dodBYTE  r, c, rnd, d;
    dodBYTE  shA, shB;
    dodSHORT shD, shD2;

    if (creature.FRZFLG == 0)
    {
        /* ignore dead creatures */
        if (creature.CCBLND[cidx].P_CCUSE == 0)
        {
            return 0;
        }

        /* pick up object */
        if (creature.CCBLND[cidx].creature_id != CRT_SCORPION &&
            creature.CCBLND[cidx].creature_id < CRT_WIZIMG &&
            !(game.CreaturesIgnoreObjects &&
              creature.CCBLND[cidx].P_CCROW == player.PROW.row &&
              creature.CCBLND[cidx].P_CCCOL == player.PROW.col))
        {
            RowCol rc;
            rc.row = creature.CCBLND[cidx].P_CCROW;
            rc.col = creature.CCBLND[cidx].P_CCCOL;
            object.OFINDF = 0;
            oidx = object_OFIND(rc);
            if (oidx != DOD_NONE)
            {
                object.OCBLND[oidx].P_OCPTR = creature.CCBLND[cidx].P_CCOBJ;
                creature.CCBLND[cidx].P_CCOBJ = oidx;
                --object.OCBLND[oidx].P_OCOWN;
                viewer_PUPDAT();
                if (creature.CCBLND[cidx].P_CCROW == player.PROW.row &&
                    creature.CCBLND[cidx].P_CCCOL == player.PROW.col)
                {
                    viewer_PUPDAT();
                    viewer.NEWLUK = 0;
                    sched.TCBLND[task].next_time =
                        (jiffy_t)(plat_jiffies() +
                                  creature.CCBLND[cidx].P_CCTAT);
                    return 0;
                }
                else
                {
                    sched.TCBLND[task].next_time =
                        (jiffy_t)(plat_jiffies() +
                                  creature.CCBLND[cidx].P_CCTMV);
                    return 0;
                }
            }
        }

        /* attack player */
        if (creature.CCBLND[cidx].P_CCROW == player.PROW.row &&
            creature.CCBLND[cidx].P_CCCOL == player.PROW.col)
        {
            /* do creature sound (same cell: range 0) */
            snd_creature(creSound[creature.CCBLND[cidx].creature_id], 0);
            while (snd_playing())
            {
                core_pump();
                if (game.AUTFLG && game.demoRestart == 0)
                {
                    return 0;
                }
            }

            /* set player shielding parameters */
            shA = 0x80;
            shB = 0x80;

            if (player.PLHAND != DOD_NONE &&
                object.OCBLND[player.PLHAND].obj_type == OBJT_SHIELD)
            {
                shD = (dodSHORT)((shA << 8) | shB);
                shD2 = (dodSHORT)((object.OCBLND[player.PLHAND].P_OCXX0 << 8) |
                                   object.OCBLND[player.PLHAND].P_OCXX1);
                if (shD2 < shD)
                {
                    shA = (dodBYTE)(shD2 >> 8);
                    shB = (dodBYTE)(shD2 & 255);
                }
            }

            if (player.PRHAND != DOD_NONE &&
                object.OCBLND[player.PRHAND].obj_type == OBJT_SHIELD)
            {
                shD = (dodSHORT)((shA << 8) | shB);
                shD2 = (dodSHORT)((object.OCBLND[player.PRHAND].P_OCXX0 << 8) |
                                   object.OCBLND[player.PRHAND].P_OCXX1);
                if (shD2 < shD)
                {
                    shA = (dodBYTE)(shD2 >> 8);
                    shB = (dodBYTE)(shD2 & 255);
                }
            }

            player.PMGD = shA;
            player.PPHD = shB;

            /* process attack (the port's CHEAT_INVULNERABLE guard was an
             * enhancement and was not extracted) */
            if (player_ATTACK(creature.CCBLND[cidx].P_CCPOW, player.PPOW,
                              player.PDAM))
            {
                /* make CLANK sound */
                snd_creature(SND_CLANK, 0);
                while (snd_playing())
                {
                    core_pump();
                    if (game.AUTFLG && game.demoRestart == 0)
                    {
                        return 0;
                    }
                }

                player_DAMAGE(creature.CCBLND[cidx].P_CCPOW,
                              creature.CCBLND[cidx].P_CCMGO,
                              creature.CCBLND[cidx].P_CCPHO, player.PPOW,
                              player.PMGD, player.PPHD, &player.PDAM);
            }

            player_HUPDAT();
            sched.TCBLND[task].next_time =
                (jiffy_t)(plat_jiffies() + creature.CCBLND[cidx].P_CCTMV);
            return 0;
        }

        /* look for player along ROW axis */
        if (creature.CCBLND[cidx].P_CCROW == player.PROW.row)
        {
            if (creature.CCBLND[cidx].P_CCCOL < player.PROW.col)
            {
                dir = 1;
            }
            else
            {
                dir = 3;
            }
            r = creature.CCBLND[cidx].P_CCROW;
            c = creature.CCBLND[cidx].P_CCCOL;
            do
            {
                if (!dungeon_STEPOK(r, c, dir))
                {
                    doRandom = 1;
                    break;
                }
                r += dungeon_STPTAB[dir * 2];
                c += dungeon_STPTAB[(dir * 2) + 1];
            } while (!(r == player.PROW.row &&
                       c == player.PROW.col));
            if (doRandom == 0)
            {
                creature.CCBLND[cidx].P_CCDIR = dir;
                creature_CWALK(0, &creature.CCBLND[cidx]);
                if (creature.CCBLND[cidx].P_CCROW == player.PROW.row &&
                    creature.CCBLND[cidx].P_CCCOL == player.PROW.col)
                {
                    viewer_PUPDAT();
                    viewer.NEWLUK = 0;
                    sched.TCBLND[task].next_time =
                        (jiffy_t)(plat_jiffies() +
                                  creature.CCBLND[cidx].P_CCTAT);
                    return 0;
                }
                else
                {
                    sched.TCBLND[task].next_time =
                        (jiffy_t)(plat_jiffies() +
                                  creature.CCBLND[cidx].P_CCTMV);
                    return 0;
                }
            }
        }

        /* look for player along COL axis */
        if (creature.CCBLND[cidx].P_CCCOL == player.PROW.col)
        {
            if (creature.CCBLND[cidx].P_CCROW < player.PROW.row)
            {
                dir = 2;
            }
            else
            {
                dir = 0;
            }
            r = creature.CCBLND[cidx].P_CCROW;
            c = creature.CCBLND[cidx].P_CCCOL;
            do
            {
                if (!dungeon_STEPOK(r, c, dir))
                {
                    doRandom = 1;
                    break;
                }
                r += dungeon_STPTAB[dir * 2];
                c += dungeon_STPTAB[(dir * 2) + 1];
            } while (!(r == player.PROW.row &&
                       c == player.PROW.col));
            if (doRandom == 0)
            {
                creature.CCBLND[cidx].P_CCDIR = dir;
                creature_CWALK(0, &creature.CCBLND[cidx]);
                if (creature.CCBLND[cidx].P_CCROW == player.PROW.row &&
                    creature.CCBLND[cidx].P_CCCOL == player.PROW.col)
                {
                    viewer_PUPDAT();
                    viewer.NEWLUK = 0;
                    sched.TCBLND[task].next_time =
                        (jiffy_t)(plat_jiffies() +
                                  creature.CCBLND[cidx].P_CCTAT);
                    return 0;
                }
                else
                {
                    sched.TCBLND[task].next_time =
                        (jiffy_t)(plat_jiffies() +
                                  creature.CCBLND[cidx].P_CCTMV);
                    return 0;
                }
            }
        }

        /* player not seen so make random move */
        if (doRandom ||
            (creature.CCBLND[cidx].P_CCROW != player.PROW.row &&
             creature.CCBLND[cidx].P_CCCOL != player.PROW.col))
        {
            X = 0;
            rnd = rng_RANDOM();
            if ((rnd & 128) == 0)
            {
                X += 3;
            }
            rnd &= 3;
            if (rnd == 0)
            {
                X += 1;     /* (LT/RT) or (RT/LT) before FWD (magic!) */
            }
            loop = 3;
            do
            {
                d = MOVTAB[X++];
                if (creature_CWALK(d, &creature.CCBLND[cidx]))
                {
                    if (creature.CCBLND[cidx].P_CCROW == player.PROW.row &&
                        creature.CCBLND[cidx].P_CCCOL == player.PROW.col)
                    {
                        viewer_PUPDAT();
                        viewer.NEWLUK = 0;
                        sched.TCBLND[task].next_time =
                            (jiffy_t)(plat_jiffies() +
                                      creature.CCBLND[cidx].P_CCTAT);
                        return 0;
                    }
                    else
                    {
                        sched.TCBLND[task].next_time =
                            (jiffy_t)(plat_jiffies() +
                                      creature.CCBLND[cidx].P_CCTMV);
                        return 0;
                    }
                }
                --loop;
            } while (loop != 0);
            creature_CWALK(2, &creature.CCBLND[cidx]);
        }
    }

    if (creature.CCBLND[cidx].P_CCROW == player.PROW.row &&
        creature.CCBLND[cidx].P_CCCOL == player.PROW.col)
    {
        viewer_PUPDAT();
        viewer.NEWLUK = 0;
        sched.TCBLND[task].next_time =
            (jiffy_t)(plat_jiffies() + creature.CCBLND[cidx].P_CCTAT);
        return 0;
    }
    else
    {
        sched.TCBLND[task].next_time =
            (jiffy_t)(plat_jiffies() + creature.CCBLND[cidx].P_CCTMV);
        return 0;
    }
}

/* CRETUR.ASM CWALK: attempts to move the creature in the given relative
 * direction. */
uint8_t creature_CWALK(dodBYTE dir, CCB *cr)
{
    dodBYTE DIR, r, c, rr, cc, big, small;

    dir += cr->P_CCDIR;
    dir &= 3;
    DIR = dir;

    r = cr->P_CCROW;
    c = cr->P_CCCOL;
    if (dungeon_STEPOK(r, c, DIR))
    {
        r += dungeon_STPTAB[DIR * 2];
        c += dungeon_STPTAB[(DIR * 2) + 1];
        if (!creature_CFIND(r, c))
        {
            return 0;
        }

        rr = r;
        cc = c;

        if (r > player.PROW.row)
        {
            r = r - player.PROW.row;
        }
        else
        {
            r = player.PROW.row - r;
        }

        if (c > player.PROW.col)
        {
            c = c - player.PROW.col;
        }
        else
        {
            c = player.PROW.col - c;
        }

        if (r > c)
        {
            big = r;
            small = c;
        }
        else
        {
            big = c;
            small = r;
        }

        if (big > 8)
        {
            cr->P_CCROW = rr;
            cr->P_CCCOL = cc;
            cr->P_CCDIR = DIR;
            return 1;
        }

        if (small > 2)
        {
            cr->P_CCROW = rr;
            cr->P_CCCOL = cc;
            cr->P_CCDIR = DIR;
            return 1;
        }

        small = (rng_RANDOM() & 1);
        if (small == 1)
        {
            /* make sound, attenuated by maze range like SOUNDS.ASM.
             * (The port scaled Mix_Volume by (9-big)/8 and, with the
             * OPT_STEREO enhancement, panned by relative bearing; both
             * now live behind snd_creature in the platform layer.) */
            snd_creature(creSound[cr->creature_id], big);
            while (snd_playing())
            {
                core_pump();
            }
        }

        cr->P_CCROW = rr;
        cr->P_CCCOL = cc;
        cr->P_CCDIR = DIR;

        --viewer.NEWLUK;
        return 1;
    }

    return 0;
}
