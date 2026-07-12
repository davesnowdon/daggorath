/* object.c - OBIRTH.ASM / DTABAS.ASM: object creation, lookup, naming.
 *
 * Extracted from the C++ port's src/object.cpp; logic and iteration order
 * are byte-identical to the port, with types narrowed per dod_types.h.
 *
 * Width/flag notes vs the original ASM (rule 11: port wins, flagged):
 *  - XXXTAB torch timers (P_OXXX0) are in BURNER-task ticks (one tick per
 *    5 s, FRQ_TORCH = 300 jiffies).  DTABAS.ASM stores minutes (SOLAR 60,
 *    LUNAR 30, PINE 15); the port stores minutes*12 (720/360/180) because
 *    its BURNER decrements once per 5 s and converts to minutes with /12.
 *    We keep the PORT values - behaviour is identical at 5 s granularity.
 */
#include "object.h"
#include "object_deps.h"
#include "game.h"
#include "sound_ids.h"

object_state object;

/* ODBTAB: Object Definition Blocks (DTABAS.ASM OBJXXX/SPCXXX macros).
 * {class, reveal power, magic offense, physical offense} - never written. */
static const ODB ODBTAB[25] = {
    { OBJT_RING,   255,   0,   5 },     /* 0  Supreme Ring */
    { OBJT_RING,   170,   0,   5 },     /* 1  Joule Ring */
    { OBJT_WEAPON, 150,  64,  64 },     /* 2  Elvish Sword */
    { OBJT_SHIELD, 140,  13,  26 },     /* 3  Mithril Shield */
    { OBJT_SCROLL, 130,   0,   5 },     /* 4  Seer Scroll */
    { OBJT_FLASK,   70,   0,   5 },     /* 5  Thews Flask */
    { OBJT_RING,    52,   0,   5 },     /* 6  Rime Ring */
    { OBJT_SCROLL,  50,   0,   5 },     /* 7  Vision Scroll */
    { OBJT_FLASK,   48,   0,   5 },     /* 8  Abye Flask */
    { OBJT_FLASK,   40,   0,   5 },     /* 9  Hale Flask */
    { OBJT_TORCH,   70,   0,   5 },     /* 10 Solar Torch */
    { OBJT_SHIELD,  25,   0,  26 },     /* 11 Bronze Shield */
    { OBJT_RING,    13,   0,   5 },     /* 12 Vulcan Ring */
    { OBJT_WEAPON,  13,   0,  40 },     /* 13 Iron Sword */
    { OBJT_TORCH,   25,   0,   5 },     /* 14 Lunar Torch */
    { OBJT_TORCH,    5,   0,   5 },     /* 15 Pine Torch */
    { OBJT_SHIELD,   5,   0,  10 },     /* 16 Leather Shield */
    { OBJT_WEAPON,   5,   0,  16 },     /* 17 Wooden Sword */
    { OBJT_RING,     0,   0,   0 },     /* 18 Incanted Supreme Ring */
    { OBJT_RING,     0, 255, 255 },     /* 19 Incanted Joule Ring */
    { OBJT_RING,     0, 255, 255 },     /* 20 Incanted Rime Ring */
    { OBJT_RING,     0, 255, 255 },     /* 21 Incanted Vulcan Ring */
    { OBJT_RING,     0,   0,   5 },     /* 22 Gold Ring */
    { OBJT_FLASK,    0,   0,   5 },     /* 23 Empty Flask */
    { OBJT_TORCH,    5,   0,   5 }      /* 24 Dead Torch */
};

/* XXXTAB: Special Parameter Table base values (DTABAS.ASM XXXTAB).
 * Torches: {id, timer, regular light, magical light}
 * Shields: {id, magical defense filter, physical defense filter, unused}
 * Rings:   {id, charges, incanted id, unused} */
static const XDB XXXTAB_INIT[11] = {
    { 0x00, 0x0003, 0x0012, 0x0000 },   /* Supreme Ring */
    { 0x01, 0x0003, 0x0013, 0x0000 },   /* Joule Ring */
    { 0x03, 0x0040, 0x0040, 0x0000 },   /* Mithril Shield */
    { 0x06, 0x0003, 0x0014, 0x0000 },   /* Rime Ring */
    { 0x0A, 0x02D0, 0x000D, 0x000B },   /* Solar Torch */
    { 0x0B, 0x0060, 0x0080, 0x0000 },   /* Bronze Shield */
    { 0x0C, 0x0003, 0x0015, 0x0000 },   /* Vulcan Ring */
    { 0x0E, 0x0168, 0x000A, 0x0004 },   /* Lunar Torch */
    { 0x0F, 0x00B4, 0x0007, 0x0000 },   /* Pine Torch */
    { 0x10, 0x006C, 0x0080, 0x0000 },   /* Leather Shield */
    { 0x18, 0x0000, 0x0000, 0x0000 }    /* Dead Torch */
};

/* game.ShieldFix swaps the magical/physical filters of the two weak
 * shields (port enhancement, kept for state-compatibility) */
static const XDB XXXTAB_SHIELDFIX[2] = {
    { 0x0B, 0x0080, 0x0060, 0x0000 },   /* Bronze Shield */
    { 0x10, 0x0080, 0x006C, 0x0000 }    /* Leather Shield */
};

/* OMXTAB: Object Distribution Table (DTABAS.ASM OMX macro).
 * High nibble = start level, low nibble = count.  The VisionScroll
 * variant turns entry 7 (Vision Scroll) from lvl1/cnt3 into lvl0/cnt4. */
static const dodBYTE OMXTAB_STD[18] = {
    0x41, 0x31, 0x31, 0x32, 0x23, 0x23, 0x11, 0x13, 0x16,
    0x14, 0x14, 0x16, 0x01, 0x04, 0x08, 0x08, 0x03, 0x04
};
static const dodBYTE OMXTAB_VISION[18] = {
    0x41, 0x31, 0x31, 0x32, 0x23, 0x23, 0x11, 0x04, 0x16,
    0x14, 0x14, 0x16, 0x01, 0x04, 0x08, 0x08, 0x03, 0x04
};

/* GENVAL: incanted-form redirection per class; 0xFF = none.
 * SHIELD->16, SWORD->17, TORCH->15 (generic default fill) */
static const dodBYTE GENVAL[6] = { 0xFF, 0xFF, 0xFF, 0x10, 0x11, 0x0F };

/* OBJWGT: object weight per class (DTABAS.ASM WGT macro, FCB) */
const dodBYTE OBJWGT[6] = { 0x05, 0x01, 0x0A, 0x19, 0x19, 0x0A };

/* ADJTAB: packed 5-bit adjective strings, one per object id */
const dodBYTE ADJTAB[119] = {
    0x19, 0x38, 0x67, 0x58, 0x48, 0xAD, 0x28, 0x28, 0x54, 0xFA, 0xB0, 0xA0,
    0x31, 0x0A, 0xCB, 0x26, 0x68, 0x38, 0xDA, 0x9A, 0x22, 0x49, 0x60, 0x20,
    0xA6, 0x52, 0xC8, 0x28, 0x28, 0x82, 0xDE, 0x60, 0x20, 0x64, 0x96, 0x94,
    0x30, 0xAC, 0x99, 0xA5, 0xEE, 0x20, 0x02, 0x2C, 0x94, 0x20, 0x10, 0x16,
    0x14, 0x29, 0x66, 0xF6, 0x06, 0x40, 0x30, 0xC5, 0x27, 0xBB, 0x45, 0x30,
    0x6D, 0x56, 0x0C, 0x2E, 0x21, 0x13, 0x27, 0xB8, 0x29, 0x59, 0x57, 0x06,
    0x40, 0x21, 0x60, 0x97, 0x14, 0x38, 0xD8, 0x50, 0xD1, 0x05, 0x90, 0x31,
    0x2E, 0xF7, 0x90, 0xAE, 0x28, 0x4C, 0x97, 0x05, 0x80, 0x30, 0x4A, 0xE2,
    0xC8, 0xF9, 0x18, 0x52, 0x32, 0x80, 0x20, 0x4C, 0x99, 0x14, 0x20, 0x4E,
    0xF6, 0x10, 0x28, 0x0A, 0xD8, 0x53, 0x20, 0x21, 0x48, 0x50, 0x90
};

/* GENTAB: packed 5-bit generic class name strings */
const dodBYTE GENTAB[30] = {
    0x06, 0x28, 0x0C, 0xC0, 0xCD, 0x60, 0x20, 0x64, 0x97, 0x1C, 0x30, 0xA6,
    0x39, 0x3D, 0x8C, 0x30, 0xE6, 0x84, 0x95, 0x84, 0x29, 0x27, 0x77, 0xC8,
    0x80, 0x29, 0x68, 0xF9, 0x0D, 0x00
};

/* obj_type -> SND id.  The port loaded objSound[i] from WAV files whose
 * filename numbers equal the SND_ ids:
 *   0: 0C_gluglg (flask)   1: 0D_phaser (ring)   2: 0E_whoop (scroll)
 *   3: 0F_clang (shield)   4: 10_whoosh (sword)  5: 11_chuck (torch) */
const uint8_t objSound[6] = {
    SND_GLUGLG, SND_PHASER, SND_WHOOP, SND_CLANG, SND_WHOOSH, SND_CHUCK
};

static const OCB OCB_EMPTY = { .P_OCPTR = DOD_NONE };

void object_Reset(void)
{
    dodBYTE i;

    object.OFINDP = 0;
    object.OFINDF = 0;
    object.OCBPTR = 0;
    object.OBJTYP = 0;
    object.OBJCLS = 0;
    object.SPEFLG = 0;

    for (i = 0; i < 11; ++i)
    {
        object.XXXTAB[i] = XXXTAB_INIT[i];
    }
    if (game.ShieldFix)     /* do they want the shield fix? */
    {
        object.XXXTAB[5] = XXXTAB_SHIELDFIX[0];     /* Bronze Shield */
        object.XXXTAB[9] = XXXTAB_SHIELDFIX[1];     /* Leather Shield */
    }

    for (i = 0; i < 18; ++i)
    {
        object.OMXTAB[i] = game.VisionScroll ? OMXTAB_VISION[i]
                                             : OMXTAB_STD[i];
    }
}

/* OBIRTH.ASM: creates all the objects during initialization */
void object_CreateAll(void)
{
    dodBYTE a = 0, b, x;
    dodBYTE OBJCNT, OBJLVL;

    for (x = 0; x < 72; ++x)
    {
        object.OCBLND[x] = OCB_EMPTY;
    }

    do
    {
        OBJCNT = (object.OMXTAB[a] & 0x0F);
        OBJLVL = (object.OMXTAB[a] >> 4);
        b = OBJLVL;

        do
        {
            x = (dodBYTE)object_OBIRTH(a, b);
            object.OCBLND[x].P_OCOWN = 0xFF;
            ++b;
            if (b > 4)
            {
                b = OBJLVL;
            }
            --OBJCNT;
        } while (OBJCNT != 0);

        ++a;
    } while (a < 18);
}

/* Finds object on the floor in a cell */
int8_t object_OFIND(RowCol rc)
{
    int8_t idx;
    do
    {
        idx = object_FNDOBJ();
        if (idx == DOD_NONE)
            return DOD_NONE;
    } while ((!((object.OCBLND[idx].P_OCROW == rc.row) &&
                (object.OCBLND[idx].P_OCCOL == rc.col))) ||
             (object.OCBLND[idx].P_OCOWN != 0));
    return idx;
}

/* Finds objects in the OCB table (on the current level) */
int8_t object_FNDOBJ(void)
{
    int8_t x = object.OFINDP;
    if (object.OFINDF == 0)
    {
        x = -1;
        object.OFINDF = -1;
    }

    do
    {
        ++x;
        object.OFINDP = x;
        if (x == object.OCBPTR)
        {
            return DOD_NONE;
        }
    } while (object.OCBLND[x].P_OCLVL != game.LEVEL);
    return x;
}

/* Returns the object's name by expanding ADJTAB/GENTAB into the parser's
 * STRING buffer and copying into TOKEN, exactly like the port. */
void object_OBJNAM(int8_t idx)
{
    dodBYTE ctr = 0;
    const dodBYTE *X;
    dodBYTE Xup;
    dodBYTE A;
    dodBYTE offset;

    if (idx == DOD_NONE)
    {
        /* return "EMPTY" */
        parser_TOKEN[0] = 0x05;
        parser_TOKEN[1] = 0x0D;
        parser_TOKEN[2] = 0x10;
        parser_TOKEN[3] = 0x14;
        parser_TOKEN[4] = 0x19;
        parser_TOKEN[5] = I_NULL;
        return;
    }

    if (object.OCBLND[idx].obj_reveal_lvl == 0)
    {
        X = &ADJTAB[1];
        A = object.OCBLND[idx].obj_id;
        while (A != 0xFF)
        {
            parser_EXPAND(X, &Xup, (dodBYTE *)0);
            X += Xup;
            --A;
        }

        do
        {
            parser_TOKEN[ctr] = parser_STRING[ctr + 2];
        } while (parser_STRING[ctr++ + 2] != I_NULL);

        parser_TOKEN[ctr - 1] = 0;
    }

    X = &GENTAB[1];
    A = object.OCBLND[idx].obj_type;
    while (A != 0xFF)
    {
        parser_EXPAND(X, &Xup, (dodBYTE *)0);
        X += Xup;
        --A;
    }

    offset = ctr;

    do
    {
        parser_TOKEN[ctr] = parser_STRING[ctr - offset + 2];
    } while (parser_STRING[ctr++ - offset + 2] != I_NULL);
}

/* Parses an object name ([adjective] class) */
uint8_t object_PAROBJ(void)
{
    int8_t  res;
    dodBYTE A, B;

    object.SPEFLG = 0;
    res = parser_PARSER(GENTAB, &A, &B, 1);
    if (res == 0)
    {
        parser_CMDERR();
        return 0;
    }
    if (res > 0)
    {
        object.OBJTYP = A;
        object.OBJCLS = B;
        return 1;
    }

    --object.SPEFLG;
    res = parser_PARSER(ADJTAB, &A, &B, 0);
    if (res <= 0)
    {
        parser_CMDERR();
        return 0;
    }
    object.OBJTYP = A;
    object.OBJCLS = B;
    res = parser_PARSER(GENTAB, &A, &B, 1);
    if (res <= 0)
    {
        parser_CMDERR();
        return 0;
    }
    if (B != object.OBJCLS)
    {
        parser_CMDERR();
        return 0;
    }
    return 1;
}

/* OBIRTH.ASM: creates a new object */
int8_t object_OBIRTH(dodBYTE OBJTYP, dodBYTE OBJLVL)
{
    dodBYTE tmp;
    int8_t originalOCBPTR = object.OCBPTR;
    object.OCBLND[object.OCBPTR].obj_id = OBJTYP;
    object.OCBLND[object.OCBPTR].P_OCLVL = OBJLVL;
    object_OCBFIL(OBJTYP, object.OCBPTR);
    if (GENVAL[object.OCBLND[object.OCBPTR].obj_type] != 0xFF)
    {
        tmp = object.OCBLND[object.OCBPTR].obj_reveal_lvl;
        OBJTYP = GENVAL[object.OCBLND[object.OCBPTR].obj_type];
        object_OCBFIL(OBJTYP, object.OCBPTR);
        object.OCBLND[object.OCBPTR].obj_reveal_lvl = tmp;
    }
    ++object.OCBPTR;
    return originalOCBPTR;
}

/* Fills in default values for object */
void object_OCBFIL(dodBYTE OBJTYP, int8_t ptr)
{
    dodBYTE ctr = 0;

    object.OCBLND[ptr].obj_type = ODBTAB[OBJTYP].P_ODCLS;
    object.OCBLND[ptr].obj_reveal_lvl = ODBTAB[OBJTYP].P_ODREV;
    object.OCBLND[ptr].P_OCMGO = ODBTAB[OBJTYP].P_ODMGO;
    object.OCBLND[ptr].P_OCPHO = ODBTAB[OBJTYP].P_ODPHO;

    while (ctr < 11)
    {
        if (OBJTYP == object.XXXTAB[ctr].P_OXIDX)
        {
            object.OCBLND[ptr].P_OCXX0 = object.XXXTAB[ctr].P_OXXX0;
            object.OCBLND[ptr].P_OCXX1 = object.XXXTAB[ctr].P_OXXX1;
            object.OCBLND[ptr].P_OCXX2 = object.XXXTAB[ctr].P_OXXX2;
        }
        ++ctr;
    }
}
