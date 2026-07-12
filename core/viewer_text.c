/* viewer_text.c - COMTXT.ASM / STATUS.ASM / PEXAM.ASM: the text pipeline.
 *
 * Unlike the C++ port (which stored ASCII in the TXB grids and drew a
 * GL-quad vector font), this uses the ORIGINAL mechanism: the grids hold
 * 5-bit DISPLAY CODES (I_SP=0, letters 1..26, $1B-$1E punctuation,
 * $20-$23 specials/hearts) and every deposit blits the SWCHAR.ASM glyph
 * through plat_blit_glyph, exactly like TXTDPB writing 7 font bytes into
 * the display.  Inverse text (P.TXINV, per TXB) XORs the cell after the
 * blit (TXTDPB's "EORA P.TXINV,U").
 *
 * The char grids remain the authoritative state; viewer_drawArea re-blits
 * a whole TXB after a scroll, screen clear, or mode switch.  Where the
 * port's ASCII conversions ('c|64', '?', '<'...) appear, the display-code
 * identity replaces them (dod_to_ascii and the vector font are dropped).
 */
#include "viewer.h"
#include "platform.h"
#include "parser.h"
#include "player.h"
#include "object.h"
#include "creature.h"
#include "tables_font.h"
#include "tables_text.h"

/* Blit one grid cell's glyph (COMTXT.ASM TXTDPB): codes < $20 come from
 * SWCTAB (FONT_NORMAL), $20..$23 from SPCTAB (FONT_SPECIAL).  Rows are
 * pre-shifted; the 8th cell scanline keeps the background. */
static void blit_cell(const TXB *a, dodSHORT pos)
{
    dodBYTE col = (dodBYTE)(pos & 31u);
    dodBYTE row = (dodBYTE)(a->top + pos / 32u);
    dodBYTE c = a->area[pos];

    if (c < FONT_NORMAL_COUNT) {
        plat_blit_glyph(col, row, FONT_NORMAL[c]);
    } else if (c >= FONT_SPECIAL_BASE
               && c < FONT_SPECIAL_BASE + FONT_SPECIAL_COUNT) {
        plat_blit_glyph(col, row, FONT_SPECIAL[c - FONT_SPECIAL_BASE]);
    } else {
        return;
    }
    if (a->TXINV != 0) {
        plat_invert_region(col, row, 1);
    }
}

/* port Viewer::clearArea: grid <- spaces, caret home (no blit - the
 * next drawArea/draw_game repaints, as in the port) */
void viewer_clearArea(TXB *a)
{
    dodSHORT ctr;
    for (ctr = 0; ctr < a->len; ++ctr) {
        a->area[ctr] = I_SP;
    }
    a->caret = 0;
}

/* port Viewer::drawArea: re-blit the whole grid */
void viewer_drawArea(TXB *a)
{
    dodSHORT ctr;
    for (ctr = 0; ctr < a->len; ++ctr) {
        blit_cell(a, ctr);
    }
}

/* CLEAR.ASM CLRPRX + CLRSTX: clear the text and status blocks (grid and
 * display), homing their cursors */
void viewer_CLRSCR(void)
{
    viewer_clearArea(&viewer.TXTPRI);
    viewer_drawArea(&viewer.TXTPRI);
    viewer_clearArea(&viewer.TXTSTS);
    viewer_drawArea(&viewer.TXTSTS);
}

/* COMTXT.ASM TXTXXX: character handling (backspace / CR / printable).
 * Port semantics kept: backspace clamps at 0 (the ASM wrapped to the
 * end); unmapped codes deposit nothing but still advance the cursor. */
void viewer_TXTXXX(dodBYTE c)
{
    TXB *u = viewer.TXB_U;

    if (c == I_BS) {
        /* backspace */
        if (u->caret > 0) {
            u->caret -= 1;
        }
        return;
    }
    if (c == I_CR) {
        /* carriage return: down one line, back to line start */
        u->caret += 32;
        u->caret = (dodSHORT)(32u * (u->caret / 32u));
        return;
    }

    /* printable display codes deposit as-is (grid holds display codes;
     * the port's per-char ASCII mapping collapses to the identity) */
    if (c <= I_DOT || (c >= I_SHL && c <= I_LHR)) {
        if (u->caret < u->len) {      /* guard the port's +1 slack byte */
            u->area[u->caret] = c;
            blit_cell(u, u->caret);
        }
    }
    ++u->caret;
}

/* COMTXT.ASM TXTSCR: scroll the block up one line, blank the last line,
 * cursor to the start of the last line; then repaint the display (the
 * original moved the display bytes directly) */
void viewer_TXTSCR(void)
{
    TXB *u = viewer.TXB_U;
    dodSHORT ctr;

    for (ctr = 0; ctr < (dodSHORT)(u->len - 32u); ++ctr) {
        u->area[ctr] = u->area[ctr + 32u];
    }
    for (ctr = (dodSHORT)(u->len - 32u); ctr < u->len; ++ctr) {
        u->area[ctr] = 0;
    }
    u->caret = (dodSHORT)(u->len - 32u);
    if (player.PTORCH != DOD_NONE && u->len > 128u) {
        viewer.tcaret -= 32;         /* keep the torch highlight aligned */
    }
    viewer_drawArea(u);
}

/* OUTCHR: one display-code char to the current TXB (TXTPRI unless an
 * examine/status block is latched via TXBFLG), scrolling at the end of
 * the block - except the status line - and dirtying the screen */
void viewer_OUTCHR(dodBYTE c)
{
    if (viewer.TXBFLG == 0) {
        viewer.TXB_U = &viewer.TXTPRI;
    }

    viewer_TXTXXX(c);
    if (viewer.TXB_U->caret == viewer.TXB_U->len
        && viewer.TXB_U->top != STAT_ROW) {
        viewer_TXTSCR();
    }
    --viewer.UPDATE;
}

/* OUTSTR: 0xFF-terminated display-code string */
void viewer_OUTSTR(const dodBYTE *s)
{
    dodSHORT ctr = 0;
    while (s[ctr] != I_NULL) {
        viewer_OUTCHR(s[ctr]);
        ++ctr;
    }
}

/* OUTSTI: expand a 5-bit packed string, then print it (the id/count byte
 * lands in STRING[0..1]; text starts at STRING[1] per the port) */
void viewer_OUTSTI(const uint8_t *packed)
{
    int16_t c;
    parser_EXPAND(packed, &c, 0);
    viewer_OUTSTR(&parser.STRING[1]);
}

/* the '?_' command prompt */
void viewer_PROMPT(void)
{
    viewer_OUTSTR(M_PROM1);
}

/* STATUS.ASM STATUX: left/right hand names on the status line.  The
 * port writes the grid directly (no TXB cursor); the heart cells 15/16
 * are left alone.  P.TXINV always contrasts with the screen. */
void viewer_STATUS(void)
{
    dodBYTE ctr, len, offset;

    viewer.TXTSTS.TXINV = (dodBYTE)~viewer.INVFLG;

    for (ctr = 0; ctr < 15u; ++ctr) {
        viewer.statArea[ctr] = I_SP;
        viewer.statArea[ctr + 17u] = I_SP;
    }

    /* left hand */
    object_OBJNAM(player.PLHAND);
    ctr = 0;
    while (parser.TOKEN[ctr] != I_NULL) {
        viewer.statArea[ctr] = parser.TOKEN[ctr];
        ++ctr;
    }

    /* right hand, right-justified */
    object_OBJNAM(player.PRHAND);
    ctr = 0;
    while (parser.TOKEN[ctr] != I_NULL) {
        ++ctr;
    }
    len = ctr;
    ctr = (dodBYTE)(32u - len);
    offset = ctr;
    while (ctr < 32u) {
        viewer.statArea[ctr] = parser.TOKEN[ctr - offset];
        ++ctr;
    }
}

/* PEXAM.ASM PCRLF: conditional carriage return */
void viewer_PCRLF(void)
{
    viewer_OUTCHR(I_CR);
    viewer.NEWLIN = 0;
}

/* PEXAM.ASM PRTOBJ: print one object name in the examine two-column
 * layout; highlite latches the lit torch's caret/length for the
 * inverse-video highlight */
void viewer_PRTOBJ(int8_t X, uint8_t highlite)
{
    object_OBJNAM(X);
    if (highlite) {
        dodBYTE n = 0;
        viewer.tcaret = viewer.TXB_U->caret;
        while (parser.TOKEN[n] != I_NULL) {
            ++n;
        }
        viewer.tlen = n;
    }
    viewer_OUTSTR(parser.TOKEN);
    viewer.NEWLIN = (dodBYTE)~viewer.NEWLIN;
    if (viewer.NEWLIN != 0) {
        viewer.TXB_U->caret += 16;
        viewer.TXB_U->caret = (dodSHORT)((viewer.TXB_U->caret / 16u) * 16u);
    } else {
        viewer_PCRLF();
    }
}

/* PEXAM.ASM EXAMIN: compose the examine screen into examArea */
void viewer_EXAMIN(void)
{
    RowCol rc;
    int8_t idx;
    dodBYTE ctr;

    viewer.TXB_U = &viewer.TXTEXA;
    --viewer.TXBFLG;
    viewer.NEWLIN = 0;
    viewer.TXB_U->caret = 10;
    viewer_OUTSTI(TXT_EXAM1);

    /* check for a creature in our cell */
    rc.row = player.PROW;
    rc.col = player.PCOL;
    if (creature_CFIND2(rc) != DOD_NONE) {
        viewer.TXB_U->caret += 11;
        viewer_OUTSTI(TXT_EXAM2);
    }

    /* objects on the floor */
    object.OFINDF = 0;
    do {
        idx = object_OFIND(rc);
        if (idx != DOD_NONE) {
            viewer_PRTOBJ(idx, 0);
        }
    } while (idx != DOD_NONE);

    if (viewer.NEWLIN != 0) {
        viewer_PCRLF();
    }

    /* separator: a full line of '!' */
    ctr = 32;
    do {
        viewer_OUTCHR(I_EXCL);
        --ctr;
    } while (ctr != 0);

    viewer.TXB_U->caret += 12;
    viewer_OUTSTI(TXT_EXAM3);

    /* bag contents; the lit torch gets the highlight */
    idx = player.BAGPTR;
    while (idx != DOD_NONE) {
        viewer_PRTOBJ(idx, (uint8_t)(idx == player.PTORCH));
        idx = object.OCBLND[idx].P_OCPTR;
    }

    viewer.TXBFLG = 0;
}

/* port Viewer::drawTorchHighlite: inverse-video the lit torch's name on
 * the examine screen.  The name is already blitted from the grid; the
 * XOR of the full cells equals the port's filled-quad + bg-text quads. */
void viewer_drawTorchHighlite(void)
{
    dodBYTE x1 = (dodBYTE)(viewer.tcaret % 32u);
    dodBYTE y1 = (dodBYTE)(viewer.tcaret / 32u);   /* TXTEXA top is 0 */
    plat_invert_region(x1, y1, viewer.tlen);
}
