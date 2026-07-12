/* viewer.h - VIEWER.ASM / VCTLST.ASM / COMTXT.ASM: rendering + text.
 * Entry points other modules call; the render implementation lives in
 * viewer.c / viewer_room.c / viewer_list.c / viewer_text.c / viewer_fade.c. */
#ifndef DOD_VIEWER_H
#define DOD_VIEWER_H

#include "dod_types.h"

#define TEXT_COLS 32
#define STAT_ROW 19          /* status line row */
#define PROMPT_ROWS 4        /* scrolling prompt area rows 20..23 */

/* heart glyph display codes (SPCTAB, SWCHAR.ASM $20..$23) */
#define CHR_HEART_SM_L 0x20
#define CHR_HEART_SM_R 0x21
#define CHR_HEART_LG_L 0x22
#define CHR_HEART_LG_R 0x23

/* display modes (port Viewer enum, same order) */
enum {
    MODE_MAP = 0,
    MODE_3D,
    MODE_EXAMINE,
    MODE_TITLE
};

/* full-screen fade sequences (port Viewer::ShowFade modes) */
enum {
    FADE_BEGIN = 1,
    FADE_MIDDLE,
    FADE_DEATH,
    FADE_VICTORY
};

/* fade pacing, jiffies (port ms fields at 60 Hz:
 * buzzStep 300 ms -> 18, midPause 2500 ms -> 150) */
#define FADE_BUZZ_STEP 18u
#define FADE_MID_PAUSE 150u

/* text block (original TXB, byte cursor into a char grid) */
typedef struct {
    uint8_t *area;
    dodSHORT caret;
    dodSHORT len;
    dodBYTE  top;
    dodBYTE  TXINV;      /* P.TXINV (COMTXT.ASM): 0xFF = blit inverted */
} TXB;

typedef struct {
    dodBYTE VXSCAL;      /* current x/y scale factors (radix-7) */
    dodBYTE VYSCAL;
    dodSHORT VCNTRX;     /* view centroid (ASCALX/ASCALY reference) */
    dodSHORT VCNTRY;
    dodBYTE VCTFAD;      /* current fade level; 0xFF = invisible */
    dodBYTE RANGE;       /* distance of object being drawn */
    dodBYTE RLIGHT;      /* regular (torch) light level */
    dodBYTE MLIGHT;      /* magic light level */
    dodBYTE OLIGHT;      /* saved light level (faint fades) */
    dodBYTE MAGFLG;      /* next SETFAD uses magic light */
    dodBYTE HLFSTP;      /* half-step forward (SETSCL) */
    dodBYTE BAKSTP;      /* half-step backward (SETSCL) */
    dodBYTE INVFLG;      /* VDGINV: inverse (clearing) draw mode */
    dodBYTE UPDATE;      /* screen-dirty counter */
    dodBYTE NEWLUK;      /* TODO(impl): delayed-look countdown; creature.c
                            clears/decrements it (CRETUR.ASM NEWLUK) */
    dodBYTE TXBFLG;      /* examine-mode flag */
    dodBYTE showSeerMap; /* seer (1) vs vision (0) scroll map (original
                            MAPFLG); set by player_PUSE, read by MAPPER */
    dodBYTE display_mode; /* MODE_MAP / MODE_3D / MODE_EXAMINE / MODE_TITLE */
    dodBYTE NEWLIN;      /* EXAMIN two-column toggle (PEXAM) */
    dodSHORT tcaret;     /* lit-torch name caret in examArea (port) */
    dodBYTE tlen;        /* lit-torch name length in chars (port int) */
    uint8_t textArea[TEXT_COLS * PROMPT_ROWS];
    uint8_t statArea[TEXT_COLS];
    uint8_t examArea[TEXT_COLS * 19];
    TXB TXTPRI, TXTSTS, TXTEXA;
    TXB *TXB_U;
} viewer_state;

extern viewer_state viewer;

void viewer_Reset(void);
void viewer_draw_game(void);         /* redraw when UPDATE is behind */
int8_t viewer_LUKNEW(void);          /* refresh-display task (COMPLR.ASM) */
void viewer_PUPDAT(void);            /* screen-update task (PUPDAT.ASM) */
void viewer_STATUS(void);            /* status line (STATUS.ASM) */
void viewer_MAPPER(void);            /* top-view map (MAPPER.ASM) */
void viewer_VIEWER(void);            /* 3D maze compositor (VIEWER.ASM) */
void viewer_EXAMIN(void);            /* examine-screen text (PEXAM) */
void viewer_PCRLF(void);             /* conditional CR (PEXAM) */
void viewer_PRTOBJ(int8_t X, uint8_t highlite); /* one object name */
void viewer_OUTCHR(dodBYTE c);       /* one display-code char */
void viewer_OUTSTR(const dodBYTE *s);      /* 0xFF-terminated codes */
void viewer_OUTSTI(const uint8_t *packed); /* 5-bit packed string */
void viewer_PROMPT(void);            /* draw the '?_' prompt */
void viewer_CLRSCR(void);            /* CLEAR.ASM CLRPRX+CLRSTX */
void viewer_TXTXXX(dodBYTE c);       /* COMTXT.ASM deposit/BS/CR */
void viewer_TXTSCR(void);            /* COMTXT.ASM scroll up one line */
/* clear one text area: grid <- spaces, caret <- 0 (port clearArea) */
void viewer_clearArea(TXB *a);
/* re-blit one whole text area to the back buffer (port drawArea);
 * player_ShowTurn composes frames from it */
void viewer_drawArea(TXB *a);
void viewer_drawTorchHighlite(void); /* invert lit-torch name (examine) */
void viewer_clear_screen(void);      /* CLEAR.ASM: clear w/ VDGINV fill */
/* Blocking full-screen fades (port Viewer::ShowFade) with the port's
 * key-abort rules.  Returns nonzero when the fade ran to completion -
 * for FADE_BEGIN that means "no key pressed", which starts the demo. */
uint8_t viewer_ShowFade(dodBYTE fadeMode);
/* Step form of the fade for callers that own the loop:
 *   viewer_fade_start(FADE_x);
 *   while (!viewer_draw_fade()) { if (key) { viewer_fade_abort(); break; } }
 * For FADE_DEATH/FADE_VICTORY the done-flag means "final frame showing";
 * keep stepping until a key arrives, then viewer_fade_abort().
 * Buzz/kaboom audio runs inside (snd_play(SND_BUZZ/SND_KABOOM)/snd_stop). */
void viewer_fade_start(dodBYTE fadeMode);
uint8_t viewer_draw_fade(void);      /* one step; returns done-flag */
void viewer_fade_abort(void);        /* key abort: silence + clear TXTPRI */
/* attract/status messages; each OUTSTIs its tables_text.h TXT_ blob
 * (port display* member functions) */
void viewer_displayPrepare(void);        /* TXT_PREPARE -> examArea */
void viewer_displayCopyright(void);      /* TXT_COPYRIGHT -> TXTSTS */
void viewer_displayWelcomeMessage(void); /* TXT_WELCOME1/2 -> TXTPRI */
void viewer_displayDeath(void);          /* TXT_DEATH */
void viewer_displayWinner(void);         /* TXT_WINNER1/2 */
void viewer_displayEnough(void);         /* TXT_ENOUGH1/2 */
void viewer_SETFAD(void);            /* compute VCTFAD from light-range */
void viewer_SETSCL(void);            /* scale from RANGE (VIEWER.ASM) */
/* level-parity video invert (port Viewer::setVidInv);
 * creature_NEWLVL calls it with (game.LEVEL % 2) */
void viewer_setVidInv(uint8_t inv);

/* VCTLST.ASM ASCALX/ASCALY: radix-7 scale of one byte coordinate about
 * the centroid: 8-bit distance from the centroid's LOW byte, unsigned
 * 8x8 multiply with sign fixup, arithmetic shift right 7 (floor /128 -
 * NOT the port's /127 float), plus the 16-bit centroid. */
int16_t viewer_SCALE(dodBYTE coord, dodBYTE scale, dodSHORT centr);
/* Draw one expanded shape table (tables_vec.c layout) with the current
 * VXSCAL/VYSCAL/VCNTRX/VCNTRY/VCTFAD/INVFLG. */
void viewer_drawVectorList(const uint8_t *vla);

#endif /* DOD_VIEWER_H */
