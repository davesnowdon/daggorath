/* game.h - top-level game controller (dodgame.cpp / DAGGORATH.ASM). */
#ifndef DOD_GAME_H
#define DOD_GAME_H

#include "dod_types.h"

typedef struct {
    dodBYTE LEVEL;       /* current dungeon level 0..4 */
    uint8_t AUTFLG;      /* demo/autoplay active */
    uint8_t hasWon;
    uint8_t demoRestart;
    dodSHORT DEMOPTR;    /* cursor into GAME_DEMO_CMDS (attract mode) */
    uint8_t RandomMaze;  /* enhancement: off in authentic mode */
    uint8_t IsDemo;
    /* Port option toggles (dodgame.h); all 0 in authentic mode.
     * object.c and creature.c read them at Reset/CMOVE time. */
    uint8_t ShieldFix;              /* swap weak shields' defense filters */
    uint8_t VisionScroll;           /* extra vision scroll variant */
    uint8_t CreaturesIgnoreObjects; /* don't pick up under the player */
} game_state;

extern game_state game;

/* attract-mode command token stream (dodgame.cpp DEMO_CMDS) */
#define GAME_DEMO_LEN 45
extern const dodBYTE GAME_DEMO_CMDS[GAME_DEMO_LEN];

void game_COMINI(void);   /* cold start: title, demo, wait for key */
void game_INIVU(void);    /* view initialization */
void game_Restart(void);
void game_WAIT(void);     /* 1.5 s pause, demo-abortable */
void game_run(void);      /* the outer restart loop (oslink main loop) */

#endif /* DOD_GAME_H */
