/* viewer.h - VIEWER.ASM / VCTLST.ASM / COMTXT.ASM: rendering + text.
 * Entry points other modules call; the render implementation lives in
 * viewer_room.c / viewer_list.c / viewer_text.c / viewer_fade.c. */
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

/* text block (original TXB, byte cursor into a char grid) */
typedef struct {
    uint8_t *area;
    dodSHORT caret;
    dodSHORT len;
    dodBYTE  top;
} TXB;

typedef struct {
    dodBYTE VXSCAL;      /* current x/y scale factors (radix-7) */
    dodBYTE VYSCAL;
    dodBYTE VCTFAD;      /* current fade level; 0xFF = invisible */
    dodBYTE RANGE;       /* distance of object being drawn */
    dodBYTE VLIGHT;      /* light level from torches */
    dodBYTE UPDATE;      /* screen-dirty counter */
    dodBYTE TXBFLG;      /* examine-mode flag */
    dodBYTE display_mode; /* MODE_MAP / MODE_EXAMINE / MODE_3D */
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
void viewer_OUTCHR(dodBYTE c);       /* one display-code char */
void viewer_OUTSTR(const dodBYTE *s);      /* 0xFF-terminated codes */
void viewer_OUTSTI(const uint8_t *packed); /* 5-bit packed string */
void viewer_PROMPT(void);            /* draw the '?_' prompt */
void viewer_CLRSCR(void);            /* CLEAR.ASM */
void viewer_SETFAD(void);            /* compute VCTFAD from light-range */
void viewer_SETSCL(void);            /* scale from RANGE (VIEWER.ASM) */

#endif /* DOD_VIEWER_H */
