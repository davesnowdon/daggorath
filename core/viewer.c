/* viewer.c - VIEWER.ASM SETSCL / PUPDAT.ASM / MAPPER.ASM / CLEAR.ASM:
 * viewer state, screen-update tasks, top-level renderer, top-view map.
 *
 * VEC_SCALE layout (VIEWER.ASM scaling constants, byte-verified):
 *   [0..9]   NORSCL - normal scaling for RANGE 0..9
 *            C8 80 50 32 1F 14 0C 08 04 02
 *   [10]     HLFSCL - half-step FORWARD base; HLFSCL[RANGE] overlays
 *            NORSCL shifted a half cell: FF (RANGE 0), then falls into...
 *   [11..19] BAKSCL - half-step BACKWARD base (HLFSCL+1, VIEWER.ASM
 *            "BAKSCL EQU *"): 9C 64 41 28 1A 10 0A 06 03
 *   [20]     01 - the ASM's trailing "*DEBUG" byte (HLFSCL[10] overrun
 *            guard; reachable as BAKSCL+9)
 * SETSCL picks the base: HLFSTP -> HLFSCL(10), else BAKSTP -> BAKSCL(11),
 * else NORSCL(0), and indexes it with RANGE - exactly the 6809's
 * LDX #HLFSCL / LEAX chain.  The port's Viewer::Reset int members
 * (NORSCL=0, HLFSCL=10, BAKSCL=11) are the same three bases.
 *
 * The port's parallel float table (Scalef/VXSCALf) is NOT ported: all
 * scaling goes through the radix-7 integer path (viewer_SCALE).
 */
#include "viewer.h"
#include "platform.h"
#include "draw_ref.h"
#include "sched.h"
#include "player.h"
#include "object.h"
#include "creature.h"
#include "dungeon.h"
#include "tables_vec.h"

viewer_state viewer;

/* VEC_SCALE base indices (see layout note above) */
enum {
    SCL_NOR = 0,
    SCL_HLF = 10,
    SCL_BAK = 11
};

/* VIEWER.ASM SETSCL: set scaling based on RANGE */
void viewer_SETSCL(void)
{
    uint8_t idx = SCL_HLF;           /* assume half-step forward */
    if (viewer.HLFSTP == 0) {
        ++idx;                       /* assume half-step backward (BAKSCL) */
        if (viewer.BAKSTP == 0) {
            idx = SCL_NOR;           /* normal scaling */
        }
    }
    viewer.VXSCAL = VEC_SCALE[idx + viewer.RANGE];
    viewer.VYSCAL = VEC_SCALE[idx + viewer.RANGE];
}

/* port Viewer::setVidInv - level-parity video inversion (VDGINV).
 * The port swapped GL fg/bg colors; here the same effect is INVFLG
 * (inverse line drawing + inverse screen clear) plus the text blocks'
 * P.TXINV bytes.  The status line always contrasts with the screen
 * (STATUS.ASM: COM VDGINV -> P.TXINV). */
void viewer_setVidInv(uint8_t inv)
{
    viewer.INVFLG = inv ? 0xFFu : 0x00u;
    viewer.TXTPRI.TXINV = viewer.INVFLG;
    viewer.TXTEXA.TXINV = viewer.INVFLG;
    viewer.TXTSTS.TXINV = (dodBYTE)~viewer.INVFLG;
}

/* CLEAR.ASM CLEAR: fill the display with the VDGINV background.
 * plat_clear gives black; inverted levels then paint every scanline
 * solid (a full-width solid line sets pixels x=0..255). */
void viewer_clear_screen(void)
{
    plat_clear();
    if (viewer.INVFLG != 0) {
        int16_t y;
        for (y = 0; y < (int16_t)FB_HEIGHT; ++y) {
            plat_draw_line(0, y, (int16_t)FB_WIDTH, y, 0, 0);
        }
    }
}

/* port Viewer::Reset (constructor state folded in) */
void viewer_Reset(void)
{
    /* TXB wiring (port TXB::SetVals in the constructor) */
    viewer.TXTEXA.area = viewer.examArea;
    viewer.TXTEXA.len = TEXT_COLS * 19;
    viewer.TXTEXA.top = 0;
    viewer.TXTPRI.area = viewer.textArea;
    viewer.TXTPRI.len = TEXT_COLS * PROMPT_ROWS;
    viewer.TXTPRI.top = 20;
    viewer.TXTSTS.area = viewer.statArea;
    viewer.TXTSTS.len = TEXT_COLS;
    viewer.TXTSTS.top = STAT_ROW;
    viewer.VCNTRX = 128;             /* port ctor: fixed view centroid */
    viewer.VCNTRY = 76;

    viewer.showSeerMap = 1;
    viewer_setVidInv(0);
    viewer.UPDATE = 0;
    viewer.display_mode = MODE_TITLE;
    viewer.HLFSTP = 0;
    viewer.BAKSTP = 0;
    viewer.MAGFLG = 0;
    viewer.TXBFLG = 0;
    viewer.TXB_U = 0;
    viewer.tcaret = 0;
    viewer.tlen = 0;
    viewer.RLIGHT = 0;
    viewer.MLIGHT = 0;
    viewer.VCTFAD = 32;
    viewer_clearArea(&viewer.TXTPRI);
    viewer_clearArea(&viewer.TXTEXA);
    viewer_clearArea(&viewer.TXTSTS);
}

/* COMPLR.ASM LUKNEW: refresh-display task, every 18 jiffies (0.3 s).
 * Only the map view auto-refreshes; everything else redraws on demand. */
int8_t viewer_LUKNEW(void)
{
    sched.TCBLND[TID_REFRESH_DISP].next_time =
        (jiffy_t)(sched.curTime + sched.TCBLND[TID_REFRESH_DISP].frequency);

    if (viewer.display_mode != MODE_MAP) {
        return 0;
    }

    viewer.NEWLUK = 0;
    viewer_PUPDAT();
    return 0;
}

/* PUPDAT.ASM PUPSUB: set lighting values from the player + lit torch */
static void PUPSUB(void)
{
    dodBYTE A = player.PRLITE;
    dodBYTE B = player.PMLITE;

    if (player.PTORCH != DOD_NONE) {
        A = (dodBYTE)(A + object.OCBLND[player.PTORCH].P_OCXX1);
        B = (dodBYTE)(B + object.OCBLND[player.PTORCH].P_OCXX2);
    }
    viewer.RLIGHT = A;
    viewer.MLIGHT = B;
}

/* PUPDAT.ASM PUPDAX: update the screen (unless fainted) */
void viewer_PUPDAT(void)
{
    if (player.FAINT != 0) {
        return;
    }

    PUPSUB();

    --viewer.UPDATE;
    viewer_draw_game();
}

/* port Viewer::draw_game - the main renderer: map, or
 * 3D/Examine/Title + status line + text area.  Only redraws when the
 * UPDATE dirty counter is nonzero, and clears it after presenting. */
void viewer_draw_game(void)
{
    if (viewer.UPDATE == 0) {
        return;
    }
    if (viewer.display_mode == MODE_MAP) {
        /* MAPPER paints every cell black or white itself (the map
         * ignores VDGINV, matching the port's absolute map colors) */
        plat_clear();
        viewer_MAPPER();
        plat_present();
    } else {
        viewer_clear_screen();
        switch (viewer.display_mode) {
        case MODE_3D:
            viewer_VIEWER();
            break;
        case MODE_EXAMINE:
            viewer_clearArea(&viewer.TXTEXA);
            viewer_EXAMIN();
            viewer_drawArea(&viewer.TXTEXA);
            if (player.PTORCH != DOD_NONE) {
                viewer_drawTorchHighlite();
            }
            break;
        case MODE_TITLE:
            viewer_drawArea(&viewer.TXTEXA);
            break;
        default:
            break;
        }

        viewer_drawArea(&viewer.TXTSTS);
        viewer_drawArea(&viewer.TXTPRI);
        plat_present();
    }
    viewer.UPDATE = 0;
}

/* ---- MAPPER.ASM: top-view map ----------------------------------------
 * The original stores raw bytes into the 32-byte-wide display: each maze
 * cell is 8x6 pixels at (col*8, row*6).  Walled-in cells (MAZLND 0xFF)
 * are solid white, visitable cells solid black; MARK4 overlays 8-bit
 * pixel patterns on cell scanlines 1..4.  Here each pattern byte becomes
 * horizontal solid-line runs (plat_draw_line never plots its endpoint,
 * so run [a..b] inclusive is the line a -> b+1). */

static void map_row_runs(dodBYTE pat, int16_t x, int16_t y)
{
    uint8_t i = 0;
    while (i < 8u) {
        if ((pat & (0x80u >> i)) != 0) {
            uint8_t j = i;
            while (j + 1u < 8u && (pat & (0x80u >> (j + 1u))) != 0) {
                ++j;
            }
            plat_draw_line((int16_t)(x + i), y,
                           (int16_t)(x + j + 1), y, 0, 0);
            i = (uint8_t)(j + 2u);
        } else {
            ++i;
        }
    }
}

/* MAPPER.ASM MARK4: scanlines 1 & 4 get pattern a, 2 & 3 pattern b */
static void MARK4(dodBYTE a, dodBYTE b, dodBYTE row, dodBYTE col)
{
    int16_t x = (int16_t)(col * 8);
    int16_t y = (int16_t)(row * 6);
    map_row_runs(a, x, (int16_t)(y + 1));
    map_row_runs(b, x, (int16_t)(y + 2));
    map_row_runs(b, x, (int16_t)(y + 3));
    map_row_runs(a, x, (int16_t)(y + 4));
}

void viewer_MAPPER(void)
{
    dodSHORT mazIdx;
    dodSHORT vftIdx;
    int8_t objIdx;
    dodBYTE creIdx;
    dodBYTE a, r;
    uint8_t vftOnce;
    RowCol rc;

    dungeon.DROW.row = 31;
    dungeon.DROW.col = 31;
    do {
        mazIdx = dungeon_RC2IDX(dungeon.DROW.row, dungeon.DROW.col);
        if (dungeon.MAZLND[mazIdx] == 0xFF) {
            /* unoccupiable cell: solid white block (MAPP20 DECB path);
             * visitable cells stay background black (CLRB path) */
            for (r = 0; r < 6u; ++r) {
                plat_draw_line((int16_t)(dungeon.DROW.col * 8),
                               (int16_t)(dungeon.DROW.row * 6 + r),
                               (int16_t)(dungeon.DROW.col * 8 + 8),
                               (int16_t)(dungeon.DROW.row * 6 + r),
                               0, 0);
            }
        }
        --dungeon.DROW.col;
        if (dungeon.DROW.col == 0xFF) {
            --dungeon.DROW.row;
            dungeon.DROW.col = 31;
        }
    } while (dungeon.DROW.row != 0xFF);

    if (viewer.showSeerMap) {
        /* Mark Objects (skip owned ones) */
        object.OFINDF = 0;
        for (;;) {
            objIdx = object_FNDOBJ();
            if (objIdx == DOD_NONE) {
                break;
            }
            if (object.OCBLND[objIdx].P_OCOWN != 0) {
                continue;
            }
            MARK4(0x00, 0x08,
                  object.OCBLND[objIdx].P_OCROW,
                  object.OCBLND[objIdx].P_OCCOL);
        }

        /* Mark Creatures */
        for (creIdx = 0; creIdx < 32u; ++creIdx) {
            if (creature.CCBLND[creIdx].P_CCUSE == 0) {
                continue;
            }
            MARK4(0x10, 0x54,
                  creature.CCBLND[creIdx].P_CCROW,
                  creature.CCBLND[creIdx].P_CCCOL);
        }
    }

    /* Mark Player ("X" marks the spot) */
    MARK4(0x24, 0x18, player.PROW, player.PCOL);

    /* Mark Vertical Features: both 0xFF-fenced halves of VFTTAB
     * (the original BSRs MAPP60 then falls through into it again) */
    vftIdx = dungeon.VFTPTR;
    vftOnce = 0;
    for (;;) {
        a = dungeon.VFTTAB[vftIdx++];
        if (a == 0xFF) {
            if (!vftOnce) {
                vftOnce = 1;
                continue;
            }
            break;
        }
        rc.row = dungeon.VFTTAB[vftIdx++];
        rc.col = dungeon.VFTTAB[vftIdx++];
        MARK4(0x3C, 0x24, rc.row, rc.col);
    }
}
