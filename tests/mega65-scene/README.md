# Fixed-scene pixel diff: MEGA65 vs desktop shim

Same scene and reference as [tests/next-scene](../next-scene/) (the
fresh-game start scene, `desk-start.pgm`), compared against the MEGA65
bitmap instead of the ULA.

Procedure (needs xemu running with `-uartmon SOCK -rom <open-roms>`;
see mega65/m65mon.py):

1. `python3 mega65/m65mon.py SOCK inject mega65/daggorath.prg`
2. ~2.5 s into the title fade press RETURN: `m65mon.py SOCK cmd @key:1`
3. wait ~35 s (PREPARE + dungeon generation + first prompt), then dump
   the live bitmap: 8000 bytes from $18000 via monitor `M` commands
   (plat_present() copies the shadow fb there verbatim; the scene is
   static) into `m65-scene.bin`
4. `python3 m65_diff.py ../next-scene/desk-start.pgm m65-scene.bin`
   - packs the PGM into the C64-bitmap cell interleave (256x192 window
   at cell column 4) and byte-compares, masking the two heart glyph
   cells (their small/large state is heartbeat phase, not rendering).
   `--mask-cursor` also masks the prompt cursor cell if the blink
   phase happens to differ from the reference.

Result 2026-07-14 (commit 08aaae3): 0 differing bytes -
FIXED-SCENE PIXEL-IDENTICAL (no cursor mask needed).  Chain:
MEGA65 bitmap = desktop fb = core/draw_ref.c = VECTOR.ASM.
