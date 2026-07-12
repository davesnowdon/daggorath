/* viewer_room.c - VIEWER.ASM: the 3D maze compositor.
 *
 * Walks cells front-to-back from the player position (RANGE 0..9),
 * drawing per cell IN THIS ORDER (draw order = overdraw order, so it is
 * load-bearing and mirrors the port's Viewer::VIEWER exactly):
 *   1. Left arch, Front arch, Right arch (FLATAB order 3,0,1); a secret
 *      door draws the door magic-lit, then a wall over it regular-lit
 *   2. creature in this cell (FWDCRE list by creature_id)
 *   3. peek-a-boo creature sliver left (LPK), then right (RPK)
 *   4. vertical features (ladder/hole composites, else ceiling line)
 *   5. objects on the floor (FWDOBJ by obj_type), each drawn TWICE:
 *      once magic-lit, once regular-lit
 * then steps forward if the front is open (line-of-sight).
 *
 * SETSCL runs per cell before drawing; SETFAD runs per shape (inside
 * DRAWIT).  VCNTRX/VCNTRY stay at the fixed 128/76 centroid (port ctor).
 */
#include "viewer.h"
#include "player.h"
#include "dungeon.h"
#include "creature.h"
#include "object.h"
#include "tables_vec.h"

/* relative direction changes: left(+3), front(+0), right(+1) */
static const dodBYTE FLATAB[3] = { 3, 0, 1 };

/* architecture lists indexed by HF_PAS/HF_DOR/HF_SDR/HF_WAL */
static const uint8_t *const LArch[4] = {
    LPAS_VLA, LDOR_VLA, LSD_VLA, LWAL_VLA
};
static const uint8_t *const FArch[4] = {
    FPAS_VLA, FDOR_VLA, FSD_VLA, FWAL_VLA
};
static const uint8_t *const RArch[4] = {
    RPAS_VLA, RDOR_VLA, RSD_VLA, RWAL_VLA
};
static const uint8_t *const *const FLATABv[3] = { LArch, FArch, RArch };

/* forward-looking creature lists, indexed by creature_id
 * (DTABAS.ASM order: SP,VI,S1,BL,K1,S2,SC,K2,WR,GL,W0,W1) */
static const uint8_t *const FWDCRE[12] = {
    SP_VLA, VI_VLA, S1_VLA, BL_VLA, K1_VLA, S2_VLA,
    SC_VLA, K2_VLA, WR_VLA, GL_VLA, W0_VLA, W1_VLA
};

/* forward-looking object lists, indexed by obj_type (OBJT_FLASK..TORCH) */
static const uint8_t *const FWDOBJ[6] = {
    FLAS_VLA, RING_VLA, SCRO_VLA, SHIE_VLA, SWOR_VLA, TORC_VLA
};

/* VIEWER.ASM DRAWIT: set fade from lighting, draw the list */
static void DRAWIT(const uint8_t *vl)
{
    viewer_SETFAD();
    viewer_drawVectorList(vl);
}

/* VIEWER.ASM CMRDRW: magical creatures draw with magic light */
static void CMRDRW(const uint8_t *vl, int8_t creNum)
{
    if (creature.CCBLND[creNum].P_CCMGO != 0) {
        --viewer.MAGFLG;
    }
    DRAWIT(vl);
}

/* VIEWER.ASM PDRAW: peek-a-boo - if the side is open and a creature
 * stands there, draw the peek sliver (not the creature's own list) */
static void PDRAW(const uint8_t *vl, dodBYTE dir, dodBYTE pdir)
{
    RowCol side;
    dodBYTE DIR;
    int8_t creNum;

    if (dungeon.NEIBOR[pdir + dir] != 0) {
        return;
    }
    DIR = (dodBYTE)((dir + player.PDIR) & 3);
    side.row = (dodBYTE)(dungeon.DROW.row + STPTAB[DIR * 2]);
    side.col = (dodBYTE)(dungeon.DROW.col + STPTAB[DIR * 2 + 1]);
    creNum = creature_CFIND2(side);
    if (creNum == DOD_NONE) {
        return;
    }
    CMRDRW(vl, creNum);
}

/* VIEWER.ASM VIEWER: the 3D viewport */
void viewer_VIEWER(void)
{
    dodBYTE a, b, x, u, ftctr, vft;
    int8_t creNum, objIdx;

    viewer.RANGE = 0;
    dungeon.DROW.row = player.PROW;
    dungeon.DROW.col = player.PCOL;

    do {
        viewer_SETSCL();

        /* unpack the cell's four 2-bit sides into NEIBOR, twice over,
         * so direction+relative indexing needs no mod-4 */
        a = dungeon.MAZLND[dungeon_RC2IDX(dungeon.DROW.row,
                                          dungeon.DROW.col)];
        u = 0;
        x = 4;
        do {
            b = (dodBYTE)(a & 3);
            dungeon.NEIBOR[u + 4] = b;
            dungeon.NEIBOR[u] = b;
            ++u;
            a >>= 2;
            --x;
        } while (x != 0);

        b = player.PDIR;
        u = b;

        /* forward-looking architectural features */
        for (ftctr = 0; ftctr < 3u; ++ftctr) {
            b = dungeon.NEIBOR[u + FLATAB[ftctr]];
            if (b == HF_SDR) {
                /* secret door: draw it magic-lit, then a wall over it */
                --viewer.MAGFLG;
                DRAWIT(FLATABv[ftctr][b]);
                b = HF_WAL;
            }
            DRAWIT(FLATABv[ftctr][b]);
        }

        /* forward-looking creature */
        creNum = creature_CFIND2(dungeon.DROW);
        if (creNum != DOD_NONE) {
            CMRDRW(FWDCRE[creature.CCBLND[creNum].creature_id], creNum);
        }

        /* peek-a-boo creatures to our left, then right */
        PDRAW(LPK_VLA, 3, u);
        PDRAW(RPK_VLA, 1, u);

        /* vertical features */
        vft = dungeon_VFIND(dungeon.DROW);
        if (vft == VF_NULL) {
            DRAWIT(CEI_VLA);
        } else {
            switch (vft) {
            case VF_HOLE_UP:
                DRAWIT(HUP_VLA);
                break;
            case VF_LADDER_UP:
                DRAWIT(LAD_VLA);
                DRAWIT(HUP_VLA);
                break;
            case VF_HOLE_DOWN:
                DRAWIT(HDN_VLA);
                DRAWIT(CEI_VLA);
                break;
            case VF_LADDER_DOWN:
                DRAWIT(LAD_VLA);
                DRAWIT(HDN_VLA);
                DRAWIT(CEI_VLA);
                break;
            default:
                break; /* should never get here */
            }
        }

        /* objects: each drawn twice - magic light, then regular light */
        object.OFINDF = 0;
        for (;;) {
            objIdx = object_OFIND(dungeon.DROW);
            if (objIdx == DOD_NONE) {
                break;
            }
            --viewer.MAGFLG;
            DRAWIT(FWDOBJ[object.OCBLND[objIdx].obj_type]);
            DRAWIT(FWDOBJ[object.OCBLND[objIdx].obj_type]);
        }

        /* line-of-sight: blocked in front? then we are done */
        if (dungeon.NEIBOR[u] != 0) {
            break;
        }

        /* take a step and increase range */
        dungeon.DROW.row = (dodBYTE)(dungeon.DROW.row
                                     + STPTAB[player.PDIR * 2]);
        dungeon.DROW.col = (dodBYTE)(dungeon.DROW.col
                                     + STPTAB[player.PDIR * 2 + 1]);
        ++viewer.RANGE;
    } while (viewer.RANGE <= 9u);
}
