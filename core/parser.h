/* parser.h - PARSER.ASM / TOKEN.ASM / EXPAND.ASM: command parser.
 *
 * Keyboard ring buffer (KBDPUT/KBDGET), the 8-phase 5-bit packed-string
 * extractor (EXPAND/GETFIV/ASRD), the tokenizer (GETTOK) and the table
 * matcher (PARSER/PARHND/CMDERR).  Extracted from the C++ port's Parser
 * class (parser.cpp); routine names cross-reference the annotated 6809
 * disassembly.
 *
 * ms->jiffy conversions: none (this module has no timing).
 */
#ifndef DOD_PARSER_H
#define DOD_PARSER_H

#include "dod_types.h"

/* raw keyboard ASCII codes (as delivered by plat_poll_key) */
enum {
    C_BS = 0x08,
    C_CR = 0x0D,
    C_SP = 0x20
};

/* internal display/token character codes (COMTXT.ASM glyph indices) */
enum {
    I_SP   = 0x00,
    I_EXCL = 0x1B,
    I_BAR  = 0x1C,
    I_QUES = 0x1D,
    I_DOT  = 0x1E,
    I_CR   = 0x1F,
    I_SHL  = 0x20,
    I_SHR  = 0x21,
    I_LHL  = 0x22,
    I_LHR  = 0x23,
    I_BS   = 0x24,
    I_NULL = 0xFF   /* string terminator char */
};

/* verb ids: index results of PARSER() over CMDTAB */
enum {
    CMD_ATTACK = 0,
    CMD_CLIMB,
    CMD_DROP,
    CMD_EXAMINE,
    CMD_GET,
    CMD_INCANT,
    CMD_LOOK,
    CMD_MOVE,
    CMD_PULL,
    CMD_REVEAL,
    CMD_STOW,
    CMD_TURN,
    CMD_USE,
    CMD_ZLOAD,
    CMD_ZSAVE
};

/* direction ids: index results of PARSER() over DIRTAB */
enum {
    DIR_LEFT = 0,
    DIR_RIGHT,
    DIR_BACK,
    DIR_AROUND,
    DIR_UP,
    DIR_DOWN
};

/* Parser state (CD.ASM widths: LINPTR/LINEND are RMB 2, the rest bytes;
 * buffer sizes keep the port's +1 slack over the original RMB 32). */
typedef struct {
    dodSHORT LINPTR;        /* index into LINBUF */
    dodBYTE  PARFLG;        /* successful match flag */
    dodBYTE  PARCNT;        /* loop counter */
    dodBYTE  VERIFY;        /* verify on/off flag */
    dodBYTE  FULFLG;        /* full word match */
    dodBYTE  KBDHDR;        /* ring "get" index */
    dodBYTE  KBDTAL;        /* ring "put" index */
    dodBYTE  BUFFLG;        /* line buffering flag */
    dodBYTE  KBDBUF[33];    /* keyboard circular buffer */
    dodBYTE  LINBUF[33];    /* line input buffer */
    dodSHORT LINEND;        /* line buffer end */
    dodBYTE  TOKEN[33];     /* token buffer */
    dodBYTE  TOKEND;        /* token buffer end */
    dodBYTE  STRING[35];    /* expanded string buffer */
    dodBYTE  SWCHAR[11];    /* S/W character expansion buffer */
    dodBYTE  OBJSTR[33];    /* object string names */
} parser_state;

extern parser_state parser;

/* shared 5-bit packed tables (decoded from the port's LoadFromHex blobs):
 * CMDTAB/DIRTAB are matched by PARSER(); player.c walks them for demo
 * playback; M_* are prompt/cursor/erase strings drawn by viewer/player. */
extern const dodBYTE CMDTAB[69];
extern const dodBYTE DIRTAB[26];
extern const dodBYTE M_PROM1[5];
extern const dodBYTE M_CURS[3];
extern const dodBYTE M_ERAS[6];

void    parser_Reset(void);
void    parser_KBDPUT(dodBYTE c);
dodBYTE parser_KBDGET(void);
void    parser_EXPAND(const dodBYTE *X, int16_t *Xup, dodBYTE *U);
dodBYTE parser_GETFIV(const dodBYTE *X, int16_t *Xup, dodBYTE *zeroY);
void    parser_ASRD(dodBYTE *A, dodBYTE *B, int8_t num);
uint8_t parser_GETTOK(void);
int8_t  parser_PARSER(const dodBYTE *pTABLE, dodBYTE *A, dodBYTE *B,
                      uint8_t norm);
void    parser_CMDERR(void);
int8_t  parser_PARHND(void);

#endif /* DOD_PARSER_H */
