/* dungeon.c - DGNGEN.ASM / VERT.ASM: maze generation and row/col math.
 *
 * Direct extraction of the C++ port's Dungeon class (dungeon.cpp).
 *
 * ms->jiffy conversions:
 * | site            | port (ms wall-clock)                  | this file (jiffies)          | original ASM                              |
 * |-----------------|---------------------------------------|------------------------------|-------------------------------------------|
 * | DGNGEN RNG spin | scheduler.curTime == 0 ?              | sched.curTime (jiffy_t),     | DGNGEN.ASM `LDB SECOND` - spins RANDOM    |
 * |                 |   (LEVEL==0 ? 6 : 21) : curTime % 60  | identical formula            | SECOND times (seconds byte; 0 spins 256)  |
 * The port's spin logic differs from the ASM (fixed 6/21 at cold start,
 * mod-60 later); rule 11 says match the PORT, so we do.
 *
 * Omitted (RandomMaze enhancement, authentic mode only - game.RandomMaze
 * is always 0 in V1):
 *   - printMaze()            (debug stdio scaffolding)
 *   - ReseedMap()
 *   - SetLEVTABRandomMap()   (held the port's only SDL_GetTicks call, as
 *                             srand(SDL_GetTicks()) seed - gone with it)
 *   - SetVFTTABRandomMap()
 * The game.RandomMaze / game.IsDemo checks themselves are kept.
 */
#include "dungeon.h"
#include "game.h"
#include "player.h"
#include "rng.h"
#include "sched.h"

dungeon_state dungeon;

/* CRETUR.ASM: step offsets per direction (row,col for N,E,S,W) */
const int8_t STPTAB[8] = { -1, 0, 0, 1, 1, 0, 0, -1 };

/* DGNGEN.ASM: per-side bit mask / door / secret-door tables */
static const dodBYTE MSKTAB[4] = { 0x03, 0x0C, 0x30, 0xC0 };
static const dodBYTE DORTAB[4] = {
    HF_DOR, HF_DOR << 2, HF_DOR << 4, HF_DOR << 6
};
static const dodBYTE SDRTAB[4] = {
    HF_SDR, HF_SDR << 2, HF_SDR << 4, HF_SDR << 6
};

/* COMCRE.ASM: original vertical features table (port values: 0xFF fences
 * where the original assembled $80; port behavior is authoritative). */
static const dodBYTE VFTTAB_ORIG[42] = {
    0xFF,
    1,  0, 23,      /* level 0/1 ladder */
    0, 15,  4,      /* level 0/1 hole   */
    0, 20, 17,      /* level 0/1 hole   */
    1, 28, 30,      /* level 0/1 ladder */
    0xFF,
    1,  2,  3,      /* level 1/2 ladder */
    0,  3, 31,      /* level 1/2 hole   */
    0, 19, 20,      /* level 1/2 hole   */
    0, 31,  0,      /* level 1/2 hole   */
    0xFF, 0xFF,
    0,  0, 31,      /* level 3/4 hole   */
    0,  5,  0,      /* level 3/4 hole   */
    0, 22, 28,      /* level 3/4 hole   */
    0, 31, 16,      /* level 3/4 hole   */
    0xFF, 0xFF
};

/* Reset to the state a freshly constructed global Dungeon had in the port
 * (globals are zero-initialized, then the constructor set the tables). */
void dungeon_Reset(void)
{
    dodSHORT ctr;

    for (ctr = 0; ctr < 1024; ++ctr)
        dungeon.MAZLND[ctr] = 0;
    for (ctr = 0; ctr < 9; ++ctr)
        dungeon.NEIBOR[ctr] = 0;
    dungeon.DROW.row = 0;
    dungeon.DROW.col = 0;
    dungeon.VFTPTR = 0;
    dungeon_SetLEVTABOrig();
    dungeon_SetVFTTABOrig();
}

/* DGNGEN.ASM BORDER: is the row/col position legal (0..31)? */
uint8_t dungeon_BORDER(dodBYTE R, dodBYTE C)
{
    if ((R & 224) != 0) return 0;
    if ((C & 224) != 0) return 0;
    return 1;
}

/* DGNGEN.ASM MAP32: cell index from row/col */
dodSHORT dungeon_RC2IDX(dodBYTE R, dodBYTE C)
{
    R &= 31;
    C &= 31;
    return (dodSHORT)(R * 32 + C);
}

/* Random direction (0..3) and distance (1..8) */
void dungeon_RndDstDir(dodBYTE *DIR, dodBYTE *DST)
{
    *DIR = (rng_RANDOM() & 3);
    *DST = (rng_RANDOM() & 7) + 1;
}

/* DGNGEN.ASM: create the dungeon maze.  Kept as one routine like the
 * original and the port; the local DROW shadows dungeon.DROW exactly as
 * the port's local did. */
void dungeon_DGNGEN(void)
{
    dodSHORT mzctr;
    dodSHORT maz_idx;
    dodSHORT cell_ctr;
    dodBYTE  a_row;
    dodBYTE  a_col;
    dodBYTE  b_row;
    dodBYTE  b_col;
    dodBYTE  DIR;
    dodBYTE  DST;
    RowCol   DROW;
    RowCol   ROW;
    dodBYTE  spin;

    /* Phase 1: Create Maze */

    /* Set Cells to 0xFF */
    for (mzctr = 0; mzctr < 1024; ++mzctr)
    {
        dungeon.MAZLND[mzctr] = 0xFF;
    }

    /* Initialize the RNG from the level's seed triple */
    rng_set_seed(dungeon.LEVTAB[game.LEVEL],
                 dungeon.LEVTAB[game.LEVEL + 1],
                 dungeon.LEVTAB[game.LEVEL + 2]);
    cell_ctr = 500;   /* Room Counter */

    /* Set Starting Room */
    if (!game.RandomMaze || game.IsDemo)
    {   /* Is this an original game?  Yes: */
        a_col = (rng_RANDOM() & 31);
        a_row = (rng_RANDOM() & 31);
        DROW.row = a_row;
        DROW.col = a_col;
        dungeon_RndDstDir(&DIR, &DST);
        /* make sure VFT isn't left overwritten from a previous new game */
        dungeon_SetVFTTABOrig();
    }
    else
    {   /* RandomMaze enhancement path (unreachable in V1: RandomMaze==0) */
        switch (game.LEVEL)
        {
            case 0:
            case 3:
                a_col = (rng_RANDOM() & 31);
                a_row = (rng_RANDOM() & 31);
                break;
            case 1:
                a_row = dungeon.VFTTAB[5];
                a_col = dungeon.VFTTAB[6];
                break;
            case 2:
                a_row = dungeon.VFTTAB[9];
                a_col = dungeon.VFTTAB[10];
                break;
            default:
                a_row = dungeon.VFTTAB[14];
                a_col = dungeon.VFTTAB[15];
                break;
        }

        if (player.PROW == 0x10 && player.PCOL == 0x0B && game.LEVEL == 0)
        {   /* Are we starting a new game? */
            player.PROW = a_row;
            player.PCOL = a_col;

            /* SetVFTTABRandomMap() omitted (RandomMaze enhancement) */
            dungeon.VFTTAB[1] = a_row;
            dungeon.VFTTAB[2] = a_col;
        }

        /* Original didn't tunnel out the starting room; the enhancement
         * does so the player never starts inside a wall. */
        DROW.row = a_row;
        DROW.col = a_col;
        dungeon_RndDstDir(&DIR, &DST);
        maz_idx = dungeon_RC2IDX(a_row, a_col);
        dungeon.MAZLND[maz_idx] = 0;
        --cell_ctr;
    }

    while (cell_ctr > 0)
    {
        /* Take a step */
        b_row = DROW.row;
        b_col = DROW.col;
        b_row += STPTAB[DIR * 2];
        b_col += STPTAB[(DIR * 2) + 1];

        /* Check if it's out of bounds */
        if (dungeon_BORDER(b_row, b_col) == 0)
        {
            dungeon_RndDstDir(&DIR, &DST);
            continue;
        }

        /* Store index and temp room */
        maz_idx = dungeon_RC2IDX(b_row, b_col);
        ROW.row = b_row;
        ROW.col = b_col;

        /* If not yet touched */
        if (dungeon.MAZLND[maz_idx] == 0xFF)
        {
            dungeon_FRIEND(ROW);
            if (dungeon.NEIBOR[3] + dungeon.NEIBOR[0] + dungeon.NEIBOR[1] == 0 ||
                dungeon.NEIBOR[1] + dungeon.NEIBOR[2] + dungeon.NEIBOR[5] == 0 ||
                dungeon.NEIBOR[5] + dungeon.NEIBOR[8] + dungeon.NEIBOR[7] == 0 ||
                dungeon.NEIBOR[7] + dungeon.NEIBOR[6] + dungeon.NEIBOR[3] == 0)
            {
                dungeon_RndDstDir(&DIR, &DST);
                continue;
            }
            dungeon.MAZLND[maz_idx] = 0;
            --cell_ctr;
        }
        if (cell_ctr > 0)
        {
            DROW = ROW;
            --DST;
            if (DST == 0)
            {
                dungeon_RndDstDir(&DIR, &DST);
                continue;
            }
            else
            {
                continue;
            }
        }
    }

    /* Phase 2: Create Walls */

    for (DROW.row = 0; DROW.row < 32; ++DROW.row)
    {
        for (DROW.col = 0; DROW.col < 32; ++DROW.col)
        {
            maz_idx = dungeon_RC2IDX(DROW.row, DROW.col);
            if (dungeon.MAZLND[maz_idx] != 0xFF)
            {
                dungeon_FRIEND(DROW);
                if (dungeon.NEIBOR[1] == 0xFF)
                    dungeon.MAZLND[maz_idx] |= N_WALL;
                if (dungeon.NEIBOR[3] == 0xFF)
                    dungeon.MAZLND[maz_idx] |= W_WALL;
                if (dungeon.NEIBOR[5] == 0xFF)
                    dungeon.MAZLND[maz_idx] |= E_WALL;
                if (dungeon.NEIBOR[7] == 0xFF)
                    dungeon.MAZLND[maz_idx] |= S_WALL;
            }
        }
    }

    /* Phase 3: Create Doors/Secret Doors */

    for (mzctr = 0; mzctr < 70; ++mzctr)
    {
        dungeon_MAKDOR(DORTAB);
    }

    for (mzctr = 0; mzctr < 45; ++mzctr)
    {
        dungeon_MAKDOR(SDRTAB);
    }

    /* Phase 4: Create vertical feature (RandomMaze enhancement;
     * unreachable in V1, check kept as in the port) */
    if (game.RandomMaze && !game.IsDemo &&
        (game.LEVEL == 0 || game.LEVEL == 1 || game.LEVEL == 3))
    {
        do
        {
            do
            {
                a_col = (rng_RANDOM() & 31);
                a_row = (rng_RANDOM() & 31);
                ROW.row = a_row;
                ROW.col = a_col;
                maz_idx = dungeon_RC2IDX(a_row, a_col);
            } while (dungeon.MAZLND[maz_idx] == 0xFF);
        } while ((game.LEVEL == 0 && dungeon.VFTTAB[1] == a_row &&
                  dungeon.VFTTAB[2] == a_col) ||
                 (game.LEVEL == 1 && dungeon.VFTTAB[5] == a_row &&
                  dungeon.VFTTAB[6] == a_col));
        switch (game.LEVEL)
        {
            case 0:
                if (dungeon.VFTTAB[5] == 0 && dungeon.VFTTAB[6] == 0) {
                    dungeon.VFTTAB[5] = a_row;
                    dungeon.VFTTAB[6] = a_col;
                }
                break;
            case 1:
                if (dungeon.VFTTAB[9] == 0 && dungeon.VFTTAB[10] == 0) {
                    dungeon.VFTTAB[9] = a_row;
                    dungeon.VFTTAB[10] = a_col;
                }
                break;
            default:
                if (dungeon.VFTTAB[14] == 0 && dungeon.VFTTAB[15] == 0) {
                    dungeon.VFTTAB[14] = a_row;
                    dungeon.VFTTAB[15] = a_col;
                }
                break;
        }
    }

    /* Spin the RNG (port logic; original used `LDB SECOND` - see header) */
    if (sched.curTime == 0)
    {
        if (game.LEVEL == 0)
        {
            spin = 6;
        }
        else
        {
            spin = 21;
        }
    }
    else
    {
        spin = (sched.curTime % 60);
    }

    while (spin > 0)
    {
        rng_RANDOM();
        --spin;
    }
}

/* NEWLVL.ASM: position VFTPTR at the current level's VFT sub-table */
void dungeon_CalcVFI(void)
{
    dodBYTE lvl = game.LEVEL;
    dodBYTE idx = 0;
    do
    {
        dungeon.VFTPTR = idx;
        while (dungeon.VFTTAB[idx++] != 0xFF)
            ;   /* loop !!! */
        --lvl;
    } while (lvl != 0xFF);
}

/* Used by VFIND (port's VFINDsub; C++ references become pointers) */
static uint8_t VFINDsub(dodBYTE *a, dodSHORT *u, const RowCol *rc)
{
    dodBYTE r, c;

    do
    {
        *a = dungeon.VFTTAB[(*u)++];
        if (*a == 0xFF)
            return 0;
        r = dungeon.VFTTAB[(*u)++];
        c = dungeon.VFTTAB[(*u)++];
    } while (!((r == rc->row) && (c == rc->col)));
    return 1;
}

/* VERT.ASM: is a hole/ladder in this cell?  Scans this level's VFT block
 * (features above), then the next (features below), since each vertical
 * feature is stored only once in the VFT. */
dodBYTE dungeon_VFIND(RowCol rc)
{
    dodSHORT u = dungeon.VFTPTR;
    dodBYTE  a = 0;
    uint8_t  res;
    res = VFINDsub(&a, &u, &rc);
    if (res != 0)
        return a;
    res = VFINDsub(&a, &u, &rc);
    if (res != 0)
        return a + 2;
    else
        return 0xFF;   /* VF_NULL (port: return -1) */
}

/* Checks for a wall in the given direction from the player's cell */
uint8_t dungeon_TryMove(dodBYTE dir)
{
    dodSHORT idx;
    dodBYTE  a;
    idx = dungeon_RC2IDX(player.PROW, player.PCOL);
    a = ((dungeon.MAZLND[idx] >> (dir * 2)) & 3);
    if (a != 3)
        return 1;
    else
        return 0;
}

/* DGNGEN.ASM MAKDOR: knock a door (table=DORTAB) or secret door
 * (table=SDRTAB) into a random legal cell side, and mirror it into the
 * adjoining cell's opposite side. */
void dungeon_MAKDOR(const dodBYTE *table)
{
    dodBYTE  a_row;
    dodBYTE  a_col;
    dodSHORT maz_idx;
    dodBYTE  val;
    dodBYTE  DIR;
    RowCol   ROW;

    do
    {
        do
        {
            a_col = (rng_RANDOM() & 31);
            a_row = (rng_RANDOM() & 31);
            ROW.row = a_row;
            ROW.col = a_col;
            maz_idx = dungeon_RC2IDX(a_row, a_col);
            val = dungeon.MAZLND[maz_idx];
        } while (val == 0xFF);

        DIR = (rng_RANDOM() & 3);
    } while ((val & MSKTAB[DIR]) != 0);

    dungeon.MAZLND[maz_idx] |= table[DIR];

    ROW.row += STPTAB[DIR * 2];
    ROW.col += STPTAB[(DIR * 2) + 1];
    DIR += 2;
    DIR &= 3;
    maz_idx = dungeon_RC2IDX(ROW.row, ROW.col);
    dungeon.MAZLND[maz_idx] |= table[DIR];
}

/* DGNGEN.ASM FRIEND: copy the 3x3 neighborhood around RC into NEIBOR
 * (out-of-bounds cells read as 0xFF solid) */
void dungeon_FRIEND(RowCol RC)
{
    dodBYTE r3, c3;
    dodBYTE u = 0;

    for (r3 = RC.row; r3 <= (RC.row + 2); ++r3)
    {
        for (c3 = RC.col; c3 <= (RC.col + 2); ++c3)
        {
            if (dungeon_BORDER((r3 - 1), (c3 - 1)) == 0)
            {
                dungeon.NEIBOR[u] = 0xFF;
            }
            else
            {
                dungeon.NEIBOR[u] =
                    dungeon.MAZLND[dungeon_RC2IDX((r3 - 1), (c3 - 1))];
            }
            ++u;
        }
    }
}

/* Can a step be taken from R,C in the given direction? */
uint8_t dungeon_STEPOK(dodBYTE R, dodBYTE C, dodBYTE dir)
{
    R += STPTAB[dir * 2];
    C += STPTAB[(dir * 2) + 1];
    if (dungeon_BORDER(R, C) == 0) return 0;
    if (dungeon.MAZLND[dungeon_RC2IDX(R, C)] == 255) return 0;
    return 1;
}

/* Load the original vertical features table */
void dungeon_SetVFTTABOrig(void)
{
    dodBYTE ctr;
    for (ctr = 0; ctr < 42; ++ctr)
    {
        dungeon.VFTTAB[ctr] = VFTTAB_ORIG[ctr];
    }
}

/* DGNGEN.ASM LVLTAB: original maze seed values */
void dungeon_SetLEVTABOrig(void)
{
    dungeon.LEVTAB[0] = 0x73;
    dungeon.LEVTAB[1] = 0xC7;
    dungeon.LEVTAB[2] = 0x5D;
    dungeon.LEVTAB[3] = 0x97;
    dungeon.LEVTAB[4] = 0xF3;
    dungeon.LEVTAB[5] = 0x13;
    dungeon.LEVTAB[6] = 0x87;
}
