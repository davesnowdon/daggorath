# Fixed-scene pixel diff: MEGA65 vs desktop shim

**Automated: `./run.sh`** (needs xemu + open-roms + llvm-mos).

Same scene and reference as [tests/next-scene](../next-scene/) (the
fresh-game start scene, `desk-start.pgm`), compared against the MEGA65
bitmap instead of the ULA.

What it does (the old manual procedure, scripted via ../mega65-lib.sh):

1. Boots xemu headless (`-headless -uartmon <sock> -sdimg <dir>
   -virtsd`, open-roms), accepts the on-boarding screen, injects
   `daggorath.prg` with `mega65/m65mon.py`.
2. RETURN during the title fade (`@key:1`), then polls the live bitmap
   (8000 bytes from $18000, `m65mon.py dump` - plat_present() copies
   the shadow fb there verbatim) until it converges on the golden.
3. `m65_diff.py` packs the PGM into the C64-bitmap cell interleave
   (256x192 window at cell column 4) and byte-compares, masking the two
   heart glyph cells.  `--mask-cursor` also masks the prompt cursor
   cell if the blink phase differs from the reference.

Results:
- 2026-07-14 (commit 08aaae3, manual procedure): 0 differing bytes.
- 2026-07-17 (first scripted run): 0 differing bytes -
  FIXED-SCENE PIXEL-IDENTICAL (no cursor mask needed).  Chain:
  MEGA65 bitmap = desktop fb = core/draw_ref.c = VECTOR.ASM.
