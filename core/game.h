/* game.h - top-level game controller (dodgame.cpp / DAGGORATH.ASM). */
#ifndef DOD_GAME_H
#define DOD_GAME_H

#include "dod_types.h"

typedef struct {
    dodBYTE LEVEL;       /* current dungeon level 0..4 */
    uint8_t AUTFLG;      /* demo/autoplay active */
    uint8_t hasWon;
    uint8_t demoRestart;
    uint8_t RandomMaze;  /* enhancement: off in authentic mode */
    uint8_t IsDemo;
} game_state;

extern game_state game;

void game_COMINI(void);   /* cold start: title, demo, wait for key */
void game_INIVU(void);    /* view initialization */
void game_Restart(void);
void game_NEWLVL(void);   /* level change (NEWLVL.ASM) */

#endif /* DOD_GAME_H */
