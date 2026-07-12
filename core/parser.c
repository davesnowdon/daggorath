/* parser.c - PARSER.ASM / TOKEN.ASM / EXPAND.ASM: command parser.
 *
 * Direct extraction of the C++ port's Parser class (parser.cpp), gotos and
 * all: byte-level behavioral identity with the port is the acceptance test.
 *
 * ms->jiffy conversions: none (this module has no timing).
 */
#include "parser.h"
#include "viewer.h"

parser_state parser;

/* "CANT" error string, 5-bit packed (private to CMDERR) */
static const dodBYTE CERR[3] = {
    0x17, 0x7B, 0xD0
};

/* Command table: count byte, then per-verb packed entries (id + text).
 * Decoded from the port constructor's LoadFromHex hex blob. */
const dodBYTE CMDTAB[69] = {
    0x0F, 0x30, 0x03, 0x4A, 0x04, 0x6B, 0x28, 0x06, 0xC4, 0xB4, 0x40, 0x20,
    0x09, 0x27, 0xC0, 0x38, 0x0B, 0x80, 0xB5, 0x2E, 0x28, 0x18, 0x0E, 0x5A,
    0x00, 0x30, 0x12, 0xE1, 0x85, 0xD4, 0x20, 0x18, 0xF7, 0xAC, 0x20, 0x1A,
    0xFB, 0x14, 0x20, 0x21, 0x56, 0x30, 0x30, 0x24, 0x5B, 0x14, 0x2C, 0x20,
    0x27, 0x47, 0xDC, 0x20, 0x29, 0x59, 0x38, 0x18, 0x2B, 0x32, 0x80, 0x28,
    0x34, 0xC7, 0x84, 0x80, 0x28, 0x35, 0x30, 0xD8, 0xA0
};

/* Direction table (LEFT/RIGHT/BACK/AROUND/UP/DOWN) */
const dodBYTE DIRTAB[26] = {
    0x06, 0x20, 0x18, 0x53, 0x50, 0x28, 0x24, 0x93, 0xA2, 0x80, 0x20, 0x04,
    0x11, 0xAC, 0x30, 0x03, 0x27, 0xD5, 0xC4, 0x10, 0x2B, 0x00, 0x20, 0x08,
    0xFB, 0xB8
};

/* prompt / cursor / erase display strings (built in the port constructor) */
const dodBYTE M_PROM1[5] = { I_CR, I_DOT, I_BAR, I_BS, I_NULL };
const dodBYTE M_CURS[3]  = { I_BAR, I_BS, I_NULL };
const dodBYTE M_ERAS[6]  = { I_SP, I_BS, I_BS, I_BAR, I_BS, I_NULL };

void parser_Reset(void)
{
    dodBYTE ctr;

    parser.LINPTR = 0;
    parser.PARFLG = 0;
    parser.PARCNT = 0;
    parser.VERIFY = 0;
    parser.FULFLG = 0;
    parser.KBDHDR = 0;
    parser.KBDTAL = 0;
    parser.BUFFLG = 0;
    parser.LINEND = 0;
    parser.TOKEND = 0;
    for (ctr = 0; ctr < 33; ++ctr)
    {
        parser.KBDBUF[ctr] = 0;
        parser.LINBUF[ctr] = 0;
        parser.TOKEN[ctr] = 0;
        parser.OBJSTR[ctr] = 0;
        parser.STRING[ctr] = 0;
    }
    parser.STRING[33] = 0;
    parser.STRING[34] = 0;
    for (ctr = 0; ctr < 11; ++ctr)
    {
        parser.SWCHAR[ctr] = 0;
    }
}

/* Put a character into the DoD keyboard ring buffer */
void parser_KBDPUT(dodBYTE c)
{
    parser.KBDBUF[parser.KBDTAL] = c;
    ++parser.KBDTAL;
    parser.KBDTAL &= 31;
}

/* Get a character from the DoD keyboard ring buffer (0 = empty) */
dodBYTE parser_KBDGET(void)
{
    dodBYTE c = 0;
    if (parser.KBDHDR == parser.KBDTAL)
        return c;
    c = parser.KBDBUF[parser.KBDHDR];
    ++parser.KBDHDR;
    parser.KBDHDR &= 31;
    return c;
}

/* The rest of these routines are direct ports from the source,
 * including all the GOTOs (they mirror the 6809 control flow). */

/* PARSER.ASM: match TOKEN against a packed table; returns 1 on a match
 * (*A = entry index, *B = entry id byte), 0 on empty token, -1 on error. */
int8_t parser_PARSER(const dodBYTE *pTABLE, dodBYTE *A, dodBYTE *B,
                     uint8_t norm)
{
    uint8_t tok;
    dodBYTE U, Y;
    int16_t Xup;
    dodBYTE retA = 0, retB = 0;

    if (norm)
    {
        *A = 0;
        *B = 0;
        tok = parser_GETTOK();
        if (tok == 0)
        {
            return 0;
        }
    }
    else
    {
        *A = 0;
    }

    parser.PARFLG = 0;
    parser.FULFLG = 0;
    *B = *pTABLE;
    ++pTABLE;
    parser.PARCNT = *B;

PARS10:
    U = 0;
    parser_EXPAND(pTABLE, &Xup, 0);
    pTABLE += Xup;
    Y = 2;

PARS12:
    *B = parser.TOKEN[U++];
    if (*B == 0xFF)
    {
        goto PARS20;
    }
    if (*B != parser.STRING[Y++])
    {
        goto PARS30;
    }
    if (parser.STRING[Y] != I_NULL && parser.STRING[Y] != 0)
    {
        goto PARS12;
    }
    if (parser.TOKEN[U] != 0xFF && parser.TOKEN[U] != 0)
    {
        goto PARS30;
    }
    --parser.FULFLG;

PARS20:
    if (parser.PARFLG != 0)
    {
        goto PARS90;
    }
    ++parser.PARFLG;
    *B = parser.STRING[1];
    retA = *A;
    retB = *B;

PARS30:
    ++*A;
    --parser.PARCNT;
    if (parser.PARCNT != 0)
    {
        goto PARS10;
    }

    if (parser.PARFLG != 0)
    {
        *A = retA;
        *B = retB;
        return 1;
    }

PARS90:
    *A = 0xFF;
    *B = 0xFF;
    return -1;
}

/* TOKEN.ASM: copy the next word from LINBUF into TOKEN (0xFF-terminated);
 * returns 0 when the line is exhausted. */
uint8_t parser_GETTOK(void)
{
    dodBYTE  U = 0;
    dodSHORT X = parser.LINPTR;
    dodBYTE  A;

    do
    {
        A = parser.LINBUF[X++];
    } while (A == 0);
    goto GTOK22;

GTOK20:
    A = parser.LINBUF[X++];

GTOK22:
    if (A == 0 || A == 0xFF)
    {
        goto GTOK30;
    }
    parser.TOKEN[U++] = A;
    if (U < 32)
    {
        goto GTOK20;
    }

GTOK30:
    parser.TOKEN[U++] = 0xFF;
    parser.LINPTR = X;

    if (parser.TOKEN[0] == 0xFF)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

/* EXPAND.ASM: unpack a 5-bit packed string at X.  U == 0 means expand into
 * parser.STRING (id byte at [1], text from [2], 0xFF terminator), using
 * STRING[0] as the GETFIV phase byte; otherwise U-1 holds the phase byte.
 * *Xup returns the source bytes consumed AFTER the leading count value
 * (quirk kept from the port: the count byte's advance is not counted, and
 * a trailing partial byte adds one). */
void parser_EXPAND(const dodBYTE *X, int16_t *Xup, dodBYTE *U)
{
    dodBYTE *Y;
    dodBYTE  A, B;
    int16_t  Xup2;

    *Xup = 0;

    if (U != 0)
    {
        Y = (U - 1);
    }
    else
    {
        Y = &parser.STRING[0];
        U = Y + 1;
    }
    *Y = 0;
    B = parser_GETFIV(X, &Xup2, Y);
    X += Xup2;
    A = B;

EXPAN10:
    B = parser_GETFIV(X, &Xup2, Y);
    X += Xup2;
    *Xup += Xup2;
    *U = B;
    ++U;
    --A;
    if (A != 0xFF)
    {
        goto EXPAN10;
    }
    *U = A;

    if ((*Y) != 0)
    {
        ++X;
        ++*Xup;
    }
}

/* EXPAND.ASM: extract the next 5-bit value; *zeroY is the phase (0..7)
 * selecting the bit alignment, advanced mod 8 on each call. */
dodBYTE parser_GETFIV(const dodBYTE *X, int16_t *Xup, dodBYTE *zeroY)
{
    dodBYTE A, B = 0;

    *Xup = 0;

    A = *zeroY;

    switch (A)
    {
    case 0:
        B = *X;
        B = (B >> 3);
        break;
    case 1:
        A = *X;
        ++X;
        ++*Xup;
        B = *X;
        parser_ASRD(&A, &B, 6);
        break;
    case 2:
        B = *X;
        B = (B >> 1);
        break;
    case 3:
        A = *X;
        ++X;
        ++*Xup;
        B = *X;
        parser_ASRD(&A, &B, 4);
        break;
    case 4:
        A = *X;
        ++X;
        ++*Xup;
        B = *X;
        parser_ASRD(&A, &B, 7);
        break;
    case 5:
        B = *X;
        B = (B >> 2);
        break;
    case 6:
        A = *X;
        ++X;
        ++*Xup;
        B = *X;
        parser_ASRD(&A, &B, 5);
        break;
    case 7:
        B = *X;
        ++X;
        ++*Xup;
        break;
    }

    A = *zeroY;
    ++A;
    A = (A & 7);
    *zeroY = A;

    return (B & 0x1F);
}

/* 6809 ASRD: arithmetic shift right of the 16-bit D = A:B pair, num times,
 * replicating the sign bit (the port ORs the saved sign back each step). */
void parser_ASRD(dodBYTE *A, dodBYTE *B, int8_t num)
{
    int16_t D = (int16_t)(((int16_t)*A << 8) + *B);
    int16_t sign = (int16_t)(D & 0x8000);

    while (num--)
        D = (D >> 1) | sign;

    *A = (dodBYTE)(D >> 8);
    *B = (dodBYTE)D;
}

/* print the "CANT" error */
void parser_CMDERR(void)
{
    viewer_OUTSTI(CERR);
}

/* parse a LEFT/RIGHT direction argument; -1 (with CMDERR) otherwise */
int8_t parser_PARHND(void)
{
    int8_t  res;
    dodBYTE A, B;

    res = parser_PARSER(DIRTAB, &A, &B, 1);
    if (res != 1)
    {
        parser_CMDERR();
        return -1;
    }
    if (A == 0 || A == 1)
    {
        return (int8_t)A;
    }
    else
    {
        parser_CMDERR();
        return -1;
    }
}
