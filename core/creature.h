/* creature.h - CRETUR.ASM / COMCRE.ASM: creature data and movement.
 *
 * Extracted from the C++ port's src/creature.{h,cpp} (Creature class) with
 * widths narrowed to the original 6809 ones.  Indices into CCBLND replace
 * the original pointers; DOD_NONE is the -1 sentinel.
 */
#ifndef DOD_CREATURE_H
#define DOD_CREATURE_H

#include "dod_types.h"

/* Creature type ids (DTABAS.ASM CREXXX order) */
enum {
    CRT_SPIDER = 0,
    CRT_VIPER = 1,
    CRT_GIANT1 = 2,
    CRT_BLOB = 3,
    CRT_KNIGHT1 = 4,
    CRT_GIANT2 = 5,
    CRT_SCORPION = 6,
    CRT_KNIGHT2 = 7,
    CRT_WRAITH = 8,
    CRT_GALDROG = 9,
    CRT_WIZIMG = 10,
    CRT_WIZARD = 11,

    CTYPES = 12
};

typedef struct {
    CCB     CCBLND[32];     /* the creature control blocks */
    dodBYTE FRZFLG;         /* freeze flag (wizard death sequence) */
    dodBYTE CMXPTR;         /* index of current level's row in CMXLND
                               (= LEVEL * CTYPES; the port kept an int) */
    dodBYTE CMXLND[60];     /* creature counts, 5 levels x 12 types;
                               filled at Reset - varies with
                               game.VisionScroll */
} creature_state;

extern creature_state creature;

void    creature_Reset(void);
void    creature_NEWLVL(void);          /* NEWLVL.ASM: populate new level */
int8_t  creature_CREGEN(void);          /* COMCRE.ASM: 5-min regen task */
int8_t  creature_CMOVE(int8_t task, int8_t cidx);  /* CRETUR.ASM move task */
uint8_t creature_CWALK(dodBYTE dir, CCB *cr);      /* CRETUR.ASM: one step */
uint8_t creature_CFIND(dodBYTE rw, dodBYTE cl);    /* cell free of creatures? */
int8_t  creature_CFIND2(RowCol rc);                /* creature idx in cell */
void    creature_CBIRTH(dodBYTE typ);   /* COMCRE.ASM: create a creature */

#endif /* DOD_CREATURE_H */
