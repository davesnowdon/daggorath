# Fixed-scene pixel diff: Spectrum Next vs desktop shim

**Automated: `./run.sh`** (needs ZEsarUX + the z88dk toolchain).

What it does:

1. Desktop reference `desk-start.pgm` (checked in; regenerate with the
   deterministic replay if the core's start scene ever changes):
       printf '100 13\n' > /tmp/abort.keys
       desktop/daggorath --headless --replay /tmp/abort.keys \
           --screenshot 2500 desk-start.pgm --exit-after 2600
2. Boots `daggorath.nex` in ZEsarUX (`../next-lib.sh`: TBBlue machine,
   host-dir esxDOS, ZRCP remote protocol), sends ENTER during the title
   fade via ZRCP `send-keys-ascii` (`zrcp.py`), then polls the draw
   buffer (`read-memory 16384 6144` - present() copies it verbatim to
   the visible bank 5) until it converges on the golden.
3. `ula_diff.py` packs the PGM into ULA layout and byte-compares,
   masking the two heart glyph cells (status row cols 15-16: their
   small/large state depends on heartbeat phase, not rendering).

Poll-until-converged, not a fixed sleep: game phases take variable wall
time under emulation (title fade dominates; PREPARE is seconds since
rng_z80.asm - see docs/PLATFORM-NOTES-next.md).  A too-early dump
differs only in leftover "PREPARE!" text cells and the next poll
retries.

`zrcp.py` is the shared ZRCP client (also used by tests/next-sound and
tests/next-save).

Results:
- 2026-07-14 (commit e78e64e, manual procedure): 0 differing bytes.
- 2026-07-15 (Covox-volume-tier build): 0 differing bytes.
- 2026-07-17 (first scripted run, post keyboard-guard/feedback build):
  0 differing bytes - FIXED-SCENE PIXEL-IDENTICAL.
  Chain: Next ULA = desktop fb = core/draw_ref.c = VECTOR.ASM.
