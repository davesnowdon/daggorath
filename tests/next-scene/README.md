# Fixed-scene pixel diff: Spectrum Next vs desktop shim

Procedure (semi-automated; needs ZEsarUX + the built .nex):

1. Desktop reference (deterministic replay, fresh-game start scene):
       printf '100 13\n' > /tmp/abort.keys
       desktop/daggorath --headless --replay /tmp/abort.keys \
           --screenshot 2500 desk-start.pgm --exit-after 2600
2. Next side: boot daggorath.nex in ZEsarUX with
   --enable-remoteprotocol, send ENTER (send-keys-ascii 200 13) during
   the title fade, wait for the prompt, then dump the draw buffer:
   ZRCP `read-memory 16384 6144` (present() copies it verbatim to the
   visible bank 5, and the scene is static).
3. `python3 ula_diff.py desk-start.pgm next-ula.bin` - packs the PGM
   into ULA layout and byte-compares, masking the two heart glyph
   cells (status row cols 15-16: their small/large state depends on
   heartbeat phase, not rendering).

Result 2026-07-14 (commit e78e64e): 0 differing bytes -
FIXED-SCENE PIXEL-IDENTICAL.  Chain: Next ULA = desktop fb =
core/draw_ref.c = VECTOR.ASM.
