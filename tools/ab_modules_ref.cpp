/* A/B reference: the port's Parser/Dungeon code verbatim, same dump. */
#include "ab_modules_ref_body.hpp"

static unsigned crc = 0xFFFFFFFFu;
static void h8(unsigned v)
{
    unsigned i;
    crc ^= v & 0xFF;
    for (i = 0; i < 8; ++i)
        crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
}

int main()
{
    int lvl, i, r, c;

    game.RandomMaze = false;
    game.IsDemo = false;
    scheduler.curTime = 0;
    player.PROW = 0x10;
    player.PCOL = 0x0B;

    /* global dungeon was default-constructed: same as dungeon_Reset() */
    for (lvl = 0; lvl < 5; ++lvl) {
        game.LEVEL = (dodBYTE)lvl;
        dungeon.DGNGEN();
        for (i = 0; i < 1024; ++i) h8(dungeon.MAZLND[i]);
        for (i = 0; i < 42; ++i) h8(dungeon.VFTTAB[i]);
        dungeon.CalcVFI();
        h8(dungeon.VFTPTR & 0xFF); h8((dungeon.VFTPTR >> 8) & 0xFF);
        for (r = 0; r < 32; ++r)
            for (c = 0; c < 32; ++c) {
                RowCol rc((dodBYTE)r, (dodBYTE)c);
                h8(dungeon.VFIND(rc));
                h8(dungeon.STEPOK((dodBYTE)r, (dodBYTE)c, (dodBYTE)(r & 3)) ? 1 : 0);
            }
        player.PROW = 5; player.PCOL = 7;
        for (i = 0; i < 4; ++i) h8(dungeon.TryMove((dodBYTE)i) ? 1 : 0);
    }
    printf("DUNGEON %08X\n", crc);

    crc = 0xFFFFFFFFu;
    {
        dodBYTE *tabs[2] = { parser.CMDTAB, parser.DIRTAB };
        int t;
        for (t = 0; t < 2; ++t) {
            dodBYTE *tab = tabs[t];
            int n = tab[0];
            dodBYTE *p = tab + 1;
            int e;
            for (e = 0; e < n; ++e) {
                int xup;
                int len, cut;
                parser.EXPAND(p, &xup, 0);
                p += xup;
                for (len = 0; parser.STRING[2 + len] != Parser::I_NULL &&
                              parser.STRING[2 + len] != 0; ++len) ;
                for (cut = 1; cut <= len; ++cut) {
                    dodBYTE A = 0, B = 0;
                    int res;
                    dodBYTE word[33];
                    int k;
                    for (k = 0; k < cut; ++k) word[k] = parser.STRING[2 + k];
                    parser.Reset();
                    for (k = 0; k < cut; ++k) parser.LINBUF[k] = word[k];
                    parser.LINBUF[cut] = Parser::I_NULL;
                    parser.LINBUF[cut + 1] = Parser::I_NULL;
                    parser.LINPTR = 0;
                    res = parser.PARSER(tab, A, B, true);
                    h8((unsigned)(res & 0xFF)); h8(A); h8(B);
                    h8(parser.PARFLG); h8(parser.FULFLG);
                    for (k = 0; k < 33; ++k) h8(parser.TOKEN[k]);
                }
            }
        }
        {
            dodBYTE A = 9, B = 9; int res; int k;
            parser.Reset();
            parser.LINBUF[0] = 24; parser.LINBUF[1] = 25; parser.LINBUF[2] = 26;
            parser.LINBUF[3] = Parser::I_NULL; parser.LINBUF[4] = Parser::I_NULL;
            res = parser.PARSER(parser.CMDTAB, A, B, true);
            h8((unsigned)(res & 0xFF)); h8(A); h8(B);
            parser.Reset();
            parser.LINBUF[0] = Parser::I_NULL; parser.LINBUF[1] = Parser::I_NULL;
            res = parser.PARSER(parser.CMDTAB, A, B, true);
            h8((unsigned)(res & 0xFF)); h8(A); h8(B);
            for (k = 0; k < 40; ++k) parser.KBDPUT((dodBYTE)(k + 1));
            for (k = 0; k < 40; ++k) h8(parser.KBDGET());
            parser.Reset();
            parser.LINBUF[0] = 12; parser.LINBUF[1] = Parser::I_NULL;
            parser.LINBUF[2] = Parser::I_NULL;
            h8((unsigned)(parser.PARHND() & 0xFF));
            parser.Reset();
            parser.LINBUF[0] = 21; parser.LINBUF[1] = 16;
            parser.LINBUF[2] = Parser::I_NULL; parser.LINBUF[3] = Parser::I_NULL;
            h8((unsigned)(parser.PARHND() & 0xFF));
        }
    }
    printf("PARSER  %08X\n", crc);
    return 0;
}
