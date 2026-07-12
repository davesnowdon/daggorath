/* player.h - HUMAN.ASM / P*.ASM: player state and command handlers.
 *
 * The port aliases the player vitals into PLRBLK (an ATB) via C++
 * reference members; here PLRBLK is the storage and PPOW/PDAM/... are
 * macros, so extracted code can keep using the original names bare.
 *
 * The keyboard ring lives in parser_state (parser_KBDPUT/KBDGET),
 * exactly as in the port's Parser class - player has no second ring.
 */
#ifndef DOD_PLAYER_H
#define DOD_PLAYER_H

#include "dod_types.h"

typedef struct {
    ATB      PLRBLK;    /* power/damage/offence/defence live here */
    OCB      EMPHND;    /* empty-hand pseudo-object (bare-fist ATTACK) */
    /* heartbeat (HUPDAT.ASM) */
    dodBYTE  HEARTF;    /* heart display enabled */
    dodBYTE  HEARTC;    /* countdown, jiffies */
    dodBYTE  HEARTR;    /* rate, jiffies */
    dodBYTE  HEARTS;    /* 0 = small drawn next, 0xFF = large */
    dodBYTE  HBEATF;    /* beating at all */
    dodBYTE  FAINT;
    /* position */
    dodBYTE  PROW;
    dodBYTE  PCOL;
    dodBYTE  PDIR;
    /* hands, pack, torch */
    int8_t   PLHAND;    /* object index or DOD_NONE */
    int8_t   PRHAND;
    int8_t   BAGPTR;
    int8_t   PTORCH;    /* lit torch object index, DOD_NONE = none */
    dodSHORT POBJWT;    /* carry weight */
    dodBYTE  PRLITE;    /* reveal-light radii */
    dodBYTE  PMLITE;
    uint8_t  turning;   /* suppress heart redraw during turns */
} player_state;

extern player_state player;

/* original bare names, aliased into PLRBLK exactly like the port */
#define PPOW (player.PLRBLK.P_ATPOW)
#define PMGO (player.PLRBLK.P_ATMGO)
#define PMGD (player.PLRBLK.P_ATMGD)
#define PPHO (player.PLRBLK.P_ATPHO)
#define PPHD (player.PLRBLK.P_ATPHD)
#define PDAM (player.PLRBLK.P_ATDAM)

void player_Reset(void);
/* nonzero = attract-mode demo loadout (the port passes game.AUTFLG);
 * zero = authentic new-game loadout (RandomMaze enhancement dropped). */
void player_setInitialObjects(uint8_t isDemo);
int8_t player_PLAYER(void);          /* the PLAYER task */
uint8_t player_HUMAN(dodBYTE c);     /* one keystroke of command input */
int8_t player_HSLOW(void);           /* heart-slow task */
int8_t player_BURNER(void);          /* torch burn task */
void player_HUPDAT(void);            /* heart update: J=(P*64)/(P+2D)-19 */
/* the raw HEARTR formula (HUPDAT.ASM divide quirk: -18 not -19); pure */
dodBYTE player_heartr_formula(dodSHORT pow, dodSHORT dam);
uint8_t player_ATTACK(dodSHORT AP, dodSHORT DP, dodSHORT DD);
/* 7th arg is the defender's accumulated-damage accumulator, exactly as
 * in the port's DAMAGE(int,int,int,int,int,int,dodSHORT*): &PDAM when a
 * creature hits the player, &CCBLND[i].P_CCDAM when the player hits. */
uint8_t player_DAMAGE(dodSHORT AP, dodBYTE AMO, dodBYTE APO,
                      dodSHORT DP, dodBYTE DMD, dodBYTE DPD,
                      dodSHORT *DD);

/* command handlers (PATTK.ASM, PTURN.ASM, ...) */
void player_PATTK(void);
void player_PCLIMB(void);
void player_PDROP(void);
void player_PEXAM(void);
void player_PGET(void);
void player_PINCAN(void);
void player_PLOOK(void);
void player_PMOVE(void);
void player_PPULL(void);
void player_PREVEA(void);
void player_PSTOW(void);
void player_PTURN(void);
void player_PUSE(void);
void player_PZLOAD(void);            /* PZTAPE.ASM */
void player_PZSAVE(void);
uint8_t player_PSTEP(dodBYTE dir);
void player_ShowTurn(dodBYTE A);     /* turn-sweep animation */

#endif /* DOD_PLAYER_H */
