/* creature_deps.h - dungeon-module declarations needed by creature.c.
 *
 * dungeon.h is being extracted in parallel by another agent; per the
 * extraction plan, creature.c declares ONLY what it needs here instead of
 * creating dungeon.h.  Every declaration below mirrors the C++ port's
 * src/dungeon.h with narrowed widths.
 *
 * TODO(merge into dungeon.h): reconcile all of this file against the real
 * dungeon module when it lands.  The extern data symbols are declared flat
 * (dungeon_MAZLND / dungeon_STPTAB); if the dungeon module keeps them
 * inside a dungeon_state struct, references in creature.c must be renamed
 * (a link error will flag every site - nothing fails silently).
 */
#ifndef DOD_CREATURE_DEPS_H
#define DOD_CREATURE_DEPS_H

#include "dod_types.h"

/* TODO(merge into dungeon.h): NEWLVL.ASM vertical-feature setup */
void dungeon_CalcVFI(void);

/* TODO(merge into dungeon.h): DGNGEN.ASM maze generator */
void dungeon_DGNGEN(void);

/* TODO(merge into dungeon.h): row/col -> MAZLND index (0..1023) */
dodSHORT dungeon_RC2IDX(dodBYTE R, dodBYTE C);

/* TODO(merge into dungeon.h): can a step be taken from (R,C) in dir? */
uint8_t dungeon_STEPOK(dodBYTE R, dodBYTE C, dodBYTE dir);

/* TODO(merge into dungeon.h): the 32x32 maze; 0xFF = solid rock cell */
extern dodBYTE dungeon_MAZLND[1024];

/* TODO(merge into dungeon.h): step offsets per direction, pairs of
 * (row,col) deltas for dirs 0..3; port init {-1,0, 0,1, 1,0, 0,-1} */
extern const int8_t dungeon_STPTAB[8];

#endif /* DOD_CREATURE_DEPS_H */
