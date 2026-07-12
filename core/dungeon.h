/* dungeon.h - DGNGEN.ASM / VERT.ASM (COMCRE.ASM data): maze storage,
 * dungeon generation, and row/column math.
 *
 * Extracted from the C++ port's Dungeon class (dungeon.cpp).  Routine
 * names cross-reference the annotated 6809 disassembly.
 */
#ifndef DOD_DUNGEON_H
#define DOD_DUNGEON_H

#include "dod_types.h"

/* Cell bit patterns (DGNGEN.ASM): two bits per side,
 * 00=passage 01=door 10=secret door 11=wall. */
enum {
    N_WALL = 0x03,
    E_WALL = 0x0C,
    S_WALL = 0x30,
    W_WALL = 0xC0,
    HF_PAS = 0,
    HF_DOR = 1,
    HF_SDR = 2,
    HF_WAL = 3,

    /* VFIND() results (vertical features) */
    VF_HOLE_UP     = 0,
    VF_LADDER_UP   = 1,
    VF_HOLE_DOWN   = 2,
    VF_LADDER_DOWN = 3,
    VF_NULL        = 255
};

typedef struct {
    dodBYTE  MAZLND[1024];  /* the maze, 32x32 cells */
    dodBYTE  NEIBOR[9];     /* 3x3 neighborhood scratch (FRIEND); the 3D
                               viewer also reuses it for a cell's walls */
    dodBYTE  LEVTAB[7];     /* RNG seeds (LVLTAB in DGNGEN.ASM) */
    RowCol   DROW;          /* current cell being rendered (viewer-owned;
                               DGNGEN shadows it with a local, as the port) */
    dodBYTE  VFTTAB[42];    /* vertical features table; 0xFF fences in the
                               port (original COMCRE.ASM used $80) */
    dodSHORT VFTPTR;        /* index into VFTTAB (original: RMB 2 pointer) */
} dungeon_state;

extern dungeon_state dungeon;

/* Step offsets per direction, row/col pairs N,E,S,W
 * (original: CRETUR.ASM `STPTAB FCB -1,0 / 0,1 / 1,0 / 0,-1`);
 * shared with player/creature movement. */
extern const int8_t STPTAB[8];

void     dungeon_Reset(void);
void     dungeon_DGNGEN(void);
void     dungeon_CalcVFI(void);
dodSHORT dungeon_RC2IDX(dodBYTE R, dodBYTE C);
uint8_t  dungeon_STEPOK(dodBYTE R, dodBYTE C, dodBYTE dir);
dodBYTE  dungeon_VFIND(RowCol rc);
uint8_t  dungeon_TryMove(dodBYTE dir);
uint8_t  dungeon_BORDER(dodBYTE R, dodBYTE C);
void     dungeon_MAKDOR(const dodBYTE *table);
void     dungeon_FRIEND(RowCol RC);
void     dungeon_RndDstDir(dodBYTE *DIR, dodBYTE *DST);
void     dungeon_SetLEVTABOrig(void);
void     dungeon_SetVFTTABOrig(void);

#endif /* DOD_DUNGEON_H */
