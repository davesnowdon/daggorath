/* A/B harness: drive the extracted C modules, dump comparable state. */
#include <stdio.h>
#include "dungeon.h"
#include "parser.h"
#include "game.h"
#include "player.h"
#include "rng.h"
#include "sched.h"
#include "viewer.h"

/* stub globals for modules that don't exist yet */
game_state game;
player_state player;
sched_state sched;
viewer_state viewer;

/* stub for parser_CMDERR's viewer call */
void viewer_OUTSTI(const uint8_t *packed) { (void)packed; }

static unsigned crc = 0xFFFFFFFFu;
static void h8(unsigned v)
{
    unsigned i;
    crc ^= v & 0xFF;
    for (i = 0; i < 8; ++i)
        crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
}

int main(void)
{
    int lvl, i, r, c;

    game.RandomMaze = 0;
    game.IsDemo = 0;
    sched.curTime = 0;
    player.PROW = 0x10;
    player.PCOL = 0x0B;

    dungeon_Reset();
    for (lvl = 0; lvl < 5; ++lvl) {
        game.LEVEL = (dodBYTE)lvl;
        dungeon_DGNGEN();
        for (i = 0; i < 1024; ++i) h8(dungeon.MAZLND[i]);
        for (i = 0; i < 42; ++i) h8(dungeon.VFTTAB[i]);
        dungeon_CalcVFI();
        h8(dungeon.VFTPTR & 0xFF); h8(dungeon.VFTPTR >> 8);
        for (r = 0; r < 32; ++r)
            for (c = 0; c < 32; ++c) {
                RowCol rc; rc.row = (dodBYTE)r; rc.col = (dodBYTE)c;
                h8(dungeon_VFIND(rc));
                h8(dungeon_STEPOK((dodBYTE)r, (dodBYTE)c, (dodBYTE)(r & 3)));
            }
        player.PROW = (dodBYTE)(r ? 5 : 5); player.PCOL = 7;
        for (i = 0; i < 4; ++i) h8(dungeon_TryMove((dodBYTE)i));
    }
    printf("DUNGEON %08X\n", crc);

    /* parser: match every CMDTAB/DIRTAB word (full + prefixes) */
    crc = 0xFFFFFFFFu;
    {
        const dodBYTE *tabs[2] = { CMDTAB, DIRTAB };
        int t;
        for (t = 0; t < 2; ++t) {
            const dodBYTE *tab = tabs[t];
            int n = tab[0];
            const dodBYTE *p = tab + 1;
            int e;
            for (e = 0; e < n; ++e) {
                int16_t xup;
                int len, cut;
                parser_EXPAND(p, &xup, 0);
                p += xup;
                for (len = 0; parser.STRING[2 + len] != I_NULL &&
                              parser.STRING[2 + len] != 0; ++len) ;
                for (cut = 1; cut <= len; ++cut) {
                    dodBYTE A = 0, B = 0;
                    int8_t res;
                    dodBYTE word[33];
                    int k;
                    for (k = 0; k < cut; ++k) word[k] = parser.STRING[2 + k];
                    parser_Reset();
                    for (k = 0; k < cut; ++k) parser.LINBUF[k] = word[k];
                    parser.LINBUF[cut] = I_NULL;
                    parser.LINBUF[cut + 1] = I_NULL;
                    parser.LINPTR = 0;
                    res = parser_PARSER(tab, &A, &B, 1);
                    h8((unsigned)(res & 0xFF)); h8(A); h8(B);
                    h8(parser.PARFLG); h8(parser.FULFLG);
                    for (k = 0; k < 33; ++k) h8(parser.TOKEN[k]);
                }
            }
        }
        /* no-match + empty-line cases */
        {
            dodBYTE A = 9, B = 9; int8_t res; int k;
            parser_Reset();
            parser.LINBUF[0] = 24; parser.LINBUF[1] = 25; parser.LINBUF[2] = 26;
            parser.LINBUF[3] = I_NULL; parser.LINBUF[4] = I_NULL;
            res = parser_PARSER(CMDTAB, &A, &B, 1);
            h8((unsigned)(res & 0xFF)); h8(A); h8(B);
            parser_Reset();
            parser.LINBUF[0] = I_NULL; parser.LINBUF[1] = I_NULL;
            res = parser_PARSER(CMDTAB, &A, &B, 1);
            h8((unsigned)(res & 0xFF)); h8(A); h8(B);
            /* kbd ring wrap */
            for (k = 0; k < 40; ++k) parser_KBDPUT((dodBYTE)(k + 1));
            for (k = 0; k < 40; ++k) h8(parser_KBDGET());
            /* PARHND on LEFT / UP / garbage */
            parser_Reset();
            parser.LINBUF[0] = 12; parser.LINBUF[1] = I_NULL; /* L */
            parser.LINBUF[2] = I_NULL;
            h8((unsigned)(parser_PARHND() & 0xFF));
            parser_Reset();
            parser.LINBUF[0] = 21; parser.LINBUF[1] = 16; /* U P */
            parser.LINBUF[2] = I_NULL; parser.LINBUF[3] = I_NULL;
            h8((unsigned)(parser_PARHND() & 0xFF));
        }
    }
    printf("PARSER  %08X\n", crc);
    return 0;
}
