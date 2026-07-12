/* object.h - OBIRTH.ASM / DTABAS.ASM: object creation, lookup, naming.
 *
 * Extracted from the C++ port's src/object.{h,cpp} (Object class) with
 * widths narrowed to the original 6809 ones.  Indices into OCBLND replace
 * the original pointers; DOD_NONE is the -1 sentinel.
 */
#ifndef DOD_OBJECT_H
#define DOD_OBJECT_H

#include "dod_types.h"

/* Object type ids (obj_id) - indices into ODBTAB, original names */
enum {
    OBJ_SWORD_WOOD = 17,
    OBJ_SWORD_IRON = 13,
    OBJ_SWORD_ELVISH = 2,

    OBJ_SHIELD_LEATHER = 16,
    OBJ_SHIELD_BRONZE = 11,
    OBJ_SHIELD_MITHRIL = 3,

    OBJ_SCROLL_SEER = 4,
    OBJ_SCROLL_VISION = 7,

    OBJ_RING_JOULE = 1,
    OBJ_RING_ENERGY = 19,   /* incanted */
    OBJ_RING_RIME = 6,
    OBJ_RING_ICE = 20,      /* incanted */
    OBJ_RING_VULCAN = 12,
    OBJ_RING_FIRE = 21,     /* incanted */
    OBJ_RING_SUPREME = 0,
    OBJ_RING_FINAL = 18,    /* incanted */
    OBJ_RING_GOLD = 22,

    OBJ_FLASK_THEWS = 5,
    OBJ_FLASK_ABYE = 8,
    OBJ_FLASK_HALE = 9,
    OBJ_FLASK_EMPTY = 23,

    OBJ_TORCH_SOLAR = 10,
    OBJ_TORCH_LUNAR = 14,
    OBJ_TORCH_PINE = 15,
    OBJ_TORCH_DEAD = 24
};

/* Object classes (obj_type) - K.FLAS..K.TORC in DTABAS.ASM */
enum {
    OBJT_FLASK = 0,
    OBJT_RING = 1,
    OBJT_SCROLL = 2,
    OBJT_SHIELD = 3,
    OBJT_WEAPON = 4,
    OBJT_TORCH = 5
};

typedef struct {
    OCB     OCBLND[72];     /* the object control blocks */
    int8_t  OFINDF;         /* FNDOBJ restart flag: 0 = restart scan */
    int8_t  OFINDP;         /* FNDOBJ scan position */
    int8_t  OCBPTR;         /* next free OCB slot (0..72) */
    dodBYTE OBJTYP;         /* PAROBJ result: object id */
    dodBYTE OBJCLS;         /* PAROBJ result: object class */
    dodBYTE SPEFLG;         /* PAROBJ: 0xFF when a specific adjective given */
    dodBYTE OMXTAB[18];     /* object distribution (lvl<<4 | count); filled
                               at Reset - varies with game.VisionScroll */
    XDB     XXXTAB[11];     /* special parameters (torch timers, shield
                               filters); filled at Reset - varies with
                               game.ShieldFix */
} object_state;

extern object_state object;

/* Shared constant tables (DTABAS.ASM); read by player/parser modules too */
extern const dodBYTE ADJTAB[119];   /* packed adjective strings */
extern const dodBYTE GENTAB[30];    /* packed generic-class strings */
extern const dodBYTE OBJWGT[6];     /* object weight per class (FCB) */

/* obj_type (OBJT_*) -> SND_ id; replaces the port's objSound Mix_Chunks.
 * Play with snd_play(objSound[obj_type], volume). */
extern const uint8_t objSound[6];

void    object_Reset(void);
void    object_CreateAll(void);                 /* OBIRTH.ASM: all levels */
int8_t  object_FNDOBJ(void);                    /* scan OCBs on this level */
int8_t  object_OFIND(RowCol rc);                /* object on floor in cell */
void    object_OBJNAM(int8_t idx);              /* name -> parser TOKEN */
int8_t  object_OBIRTH(dodBYTE OBJTYP, dodBYTE OBJLVL);  /* create object */
void    object_OCBFIL(dodBYTE OBJTYP, int8_t ptr);      /* fill defaults */
uint8_t object_PAROBJ(void);                    /* parse an object name */

#endif /* DOD_OBJECT_H */
