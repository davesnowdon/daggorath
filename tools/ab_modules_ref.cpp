/* A/B reference: the port's Parser/Dungeon code verbatim, same dump. */
#include "ab_modules_ref_body.hpp"

static unsigned crc = 0xFFFFFFFFu;
static void h8(unsigned v)
{
    unsigned i;
#ifdef AB_TRACE   /* build both sides -DAB_TRACE to diff raw byte streams */
    printf("%02X\n", v & 0xFF);
#endif
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

    /* OBJECT: CreateAll table, FNDOBJ/OFIND traversal, seeded OBIRTHs */
    crc = 0xFFFFFFFFu;
    {
        int k;
        game.ShieldFix = false;
        game.VisionScroll = false;
        game.LEVEL = 0;
        object.Reset();
        object.CreateAll();
        h8((unsigned)(object.OCBPTR & 0xFF));
        h8((unsigned)(object.OFINDF & 0xFF));
        h8((unsigned)(object.OFINDP & 0xFF));
        h8(object.OBJTYP); h8(object.OBJCLS); h8(object.SPEFLG);
        for (i = 0; i < 72; ++i) {
            OCB *o = &object.OCBLND[i];
            h8((unsigned)(o->P_OCPTR & 0xFF));
            h8(o->P_OCROW); h8(o->P_OCCOL); h8(o->P_OCLVL); h8(o->P_OCOWN);
            h8(o->P_OCXX0 & 0xFF); h8((o->P_OCXX0 >> 8) & 0xFF);
            h8(o->P_OCXX1 & 0xFF); h8((o->P_OCXX1 >> 8) & 0xFF);
            h8(o->P_OCXX2 & 0xFF); h8((o->P_OCXX2 >> 8) & 0xFF);
            h8(o->obj_id); h8(o->obj_type); h8(o->obj_reveal_lvl);
            h8(o->P_OCMGO); h8(o->P_OCPHO);
        }
        /* FNDOBJ traversal per level */
        for (lvl = 0; lvl < 5; ++lvl) {
            int idx;
            game.LEVEL = (dodBYTE)lvl;
            object.OFINDF = 0;
            do {
                idx = object.FNDOBJ();
                h8((unsigned)(idx & 0xFF));
            } while (idx != -1);
            h8((unsigned)(object.OFINDP & 0xFF));
        }
        /* OFIND on planted floor objects */
        object.OCBLND[2].P_OCOWN = 0;
        object.OCBLND[2].P_OCROW = 3;  object.OCBLND[2].P_OCCOL = 4;
        object.OCBLND[10].P_OCOWN = 0;
        object.OCBLND[10].P_OCROW = 3; object.OCBLND[10].P_OCCOL = 4;
        object.OCBLND[30].P_OCOWN = 0;
        object.OCBLND[30].P_OCROW = 7; object.OCBLND[30].P_OCCOL = 9;
        for (lvl = 0; lvl < 5; ++lvl) {
            RowCol rc1((dodBYTE)3, (dodBYTE)4);
            RowCol rc2((dodBYTE)7, (dodBYTE)9);
            game.LEVEL = (dodBYTE)lvl;
            object.OFINDF = 0;
            h8((unsigned)(object.OFIND(rc1) & 0xFF));
            object.OFINDF = 0;
            h8((unsigned)(object.OFIND(rc2) & 0xFF));
        }
        /* deterministic extra OBIRTHs + placements (CreateAll made 63
         * objects; 8 more keeps OCBPTR within the 72-entry OCBLND) */
        rng.setSEED(0x12, 0x34, 0x56);
        for (k = 0; k < 8; ++k) {
            dodBYTE typ, olvl;
            int x;
            typ = (dodBYTE)(rng.RANDOM() % 18);
            olvl = (dodBYTE)(rng.RANDOM() % 5);
            x = object.OBIRTH(typ, olvl);
            object.OCBLND[x].P_OCROW = (dodBYTE)(rng.RANDOM() & 31);
            object.OCBLND[x].P_OCCOL = (dodBYTE)(rng.RANDOM() & 31);
            h8((unsigned)(x & 0xFF));
        }
        h8((unsigned)(object.OCBPTR & 0xFF));
        for (i = 0; i < 72; ++i) {
            OCB *o = &object.OCBLND[i];
            h8((unsigned)(o->P_OCPTR & 0xFF));
            h8(o->P_OCROW); h8(o->P_OCCOL); h8(o->P_OCLVL); h8(o->P_OCOWN);
            h8(o->P_OCXX0 & 0xFF); h8((o->P_OCXX0 >> 8) & 0xFF);
            h8(o->P_OCXX1 & 0xFF); h8((o->P_OCXX1 >> 8) & 0xFF);
            h8(o->P_OCXX2 & 0xFF); h8((o->P_OCXX2 >> 8) & 0xFF);
            h8(o->obj_id); h8(o->obj_type); h8(o->obj_reveal_lvl);
            h8(o->P_OCMGO); h8(o->P_OCPHO);
        }
    }
    printf("OBJECT  %08X\n", crc);

    /* CREATUR: NEWLVL per level; CCBs, object links, scheduler TCBs.
     * Port frequencies are milliseconds: CRC (ms*6)/100 = jiffies. */
    crc = 0xFFFFFFFFu;
    {
        game.RandomMaze = false;
        game.IsDemo = false;
        game.ShieldFix = false;
        game.VisionScroll = false;
        object.Reset();
        object.CreateAll();
        creature.Reset();
        for (lvl = 0; lvl < 5; ++lvl) {
            game.LEVEL = (dodBYTE)lvl;
            player.PROW = 0x10;
            player.PCOL = 0x0B;
            scheduler.curTime = 0;
            creature.NEWLVL();
            for (i = 0; i < 32; ++i) {
                CCB *cc = &creature.CCBLND[i];
                unsigned tmv = (unsigned)((cc->P_CCTMV * 6) / 100);
                unsigned tat = (unsigned)((cc->P_CCTAT * 6) / 100);
                h8(cc->P_CCPOW & 0xFF); h8((cc->P_CCPOW >> 8) & 0xFF);
                h8(cc->P_CCMGO); h8(cc->P_CCMGD);
                h8(cc->P_CCPHO); h8(cc->P_CCPHD);
                h8(tmv & 0xFF); h8((tmv >> 8) & 0xFF);
                h8(tat & 0xFF); h8((tat >> 8) & 0xFF);
                h8((unsigned)(cc->P_CCOBJ & 0xFF));
                h8(cc->P_CCDAM & 0xFF); h8((cc->P_CCDAM >> 8) & 0xFF);
                h8(cc->P_CCUSE); h8(cc->creature_id); h8(cc->P_CCDIR);
                h8(cc->P_CCROW); h8(cc->P_CCCOL);
            }
            h8((unsigned)(creature.CMXPTR & 0xFF));
            for (i = 0; i < 72; ++i) {
                h8((unsigned)(object.OCBLND[i].P_OCPTR & 0xFF));
                h8(object.OCBLND[i].P_OCOWN);
            }
            for (i = 0; i < scheduler.TCBPTR; ++i) {
                unsigned f = (unsigned)((scheduler.TCBLND[i].frequency * 6)
                                        / 100);
                h8((unsigned)(scheduler.TCBLND[i].type & 0xFF));
                h8((unsigned)(scheduler.TCBLND[i].data & 0xFF));
                h8(f & 0xFF); h8((f >> 8) & 0xFF);
            }
            h8((unsigned)(scheduler.TCBPTR & 0xFF));
        }
        for (i = 0; i < 60; ++i) h8(creature.CMXLND[i]);
    }
    printf("CREATUR %08X\n", crc);

    /* PLRMATH: DAMAGE over seeded pseudo-random tuples + HEARTR sweep */
    crc = 0xFFFFFFFFu;
    {
        dodSHORT DD = 0;
        int k, pw, dm;
        rng.setSEED(0xAB, 0xCD, 0xEF);
        for (k = 0; k < 2000; ++k) {
            dodBYTE r1, r2, r3, r4, r5, r6, r7, r8;
            int AP, AMO, APO, DP, DMD, DPD;
            bool ret;
            r1 = rng.RANDOM(); r2 = rng.RANDOM();
            r3 = rng.RANDOM(); r4 = rng.RANDOM();
            r5 = rng.RANDOM(); r6 = rng.RANDOM();
            r7 = rng.RANDOM(); r8 = rng.RANDOM();
            AP = ((r1 & 0x1F) << 8) | r2;
            AMO = r3; APO = r4;
            DP = ((r5 & 0x1F) << 8) | r6;
            DMD = r7; DPD = r8;
            ret = player.DAMAGE(AP, AMO, APO, DP, DMD, DPD, &DD);
            h8(ret ? 1u : 0u);
            h8(DD & 0xFF); h8((DD >> 8) & 0xFF);
        }
        for (pw = 1; pw <= 500; pw += 7) {
            for (dm = 0; dm <= 300; dm += 11) {
                player.PLRBLK.P_ATPOW = (dodSHORT)pw;
                player.PLRBLK.P_ATDAM = (dodSHORT)dm;
                player.HUPDAT_heartr();
                h8(player.HEARTR);
            }
        }
    }
    printf("PLRMATH %08X\n", crc);
    return 0;
}
