# Spectrum Next backend notes

Status: **first playable** (all-C backend, verified in ZEsarUX 13.0).
Full lifecycle confirmed on the emulated Next: title fade -> demo ->
key abort -> real game -> classic opening commands -> creature combat
-> faint -> death fade -> key dismiss -> restart.

## Memory map (first-light layout)

| range         | contents                                          |
|---------------|---------------------------------------------------|
| 0x4000-0x5AFF | ULA bitmap + attributes (drawn direct, no shadow) |
| 0x6000-~0xF3xx| CRT + code + rodata + data + BSS (~37 KB code)    |
| ...-0xFDFC    | stack (REGISTER_SP = 0xFDFC, grows down)          |
| 0xFDFD-0xFDFF | IM2 jump stub (JP frame_isr)                      |
| 0xFE00-0xFF00 | IM2 vector table (257 x 0xFD)                     |

The classic 0x8000 org does not fit (CODE overflows 64K); 0x6000 works
because the screen ends at 0x5B00.  When sound samples arrive they go
in 8K MMU pages, not the main map.

## Hard-won lessons

- **Do not run on the ROM IM1 handler.**  The `sdcc_iy` CLIB owns IY,
  but the NextZXOS ROM ISR assumes IY points at the sysvars: every
  frame interrupt scribbled near 0xFFFF (where the stack was), causing
  unreproducible hangs and lost keys.  The backend installs its own
  IM2 handler (isr.asm) with the vector table above.
- **The frame ISR owns the jiffy clock**: +1,+1,+1,+1,+2 per 50 Hz
  frame = exactly 60 jiffies/50 frames (measured ~59-60/s in
  emulation).  Counting in the ISR instead of around HALT means a slow
  full-screen redraw cannot lose time.  `plat_jiffies()` reads the
  16-bit counter under DI.
- **z88dk's `in_inkey()` was unreliable here** (and predates the IM1
  corruption being fixed); the backend scans the 8x5 matrix at port
  0xFE directly with per-key edge detection and an 8-entry queue.
  CAPS+0 maps to backspace.  ZRCP's `send-keys-event` uses ASCII codes
  <128 and is the non-blocking way to inject keys when testing.
- ZEsarUX automation: `--enable-remoteprotocol` (port 10000); useful
  commands: `read-memory <dec-addr> <len>`, `get-registers`,
  `evaluate IN(65278)`, `send-keys-ascii <ms> <codes...>` (blocks ZRCP
  for the duration), `send-keys-event <ascii> <1|0>`.  Addresses are
  decimal.  `tools/../scratch zrcp.py`-style socket client beats nc.

## Resolved since first light

1. **Sound**: 26 SFX (u8, 11025 Hz) pack into `daggorath.sfx`
   (tools/gen_sfxbin.py), esxDOS-loaded at init into 8K pages 32+ and
   streamed to the Covox DAC (0xFFDF) by the zxnDMA (prescalar 79 =
   875000/11025; em00k's register program).  Multi-page samples are
   re-armed per chunk from the frame ISR through an MMU slot-1 window.
   Verified in ZEsarUX via `--aofile` capture.
2. **Double buffering**: 0x4000 maps bank 7's low page (draw buffer);
   present copies the bitmap to bank 5 through a temporary MMU slot-0
   mapping.
3. **Z80n rasterizer** (`draw_z80n.asm`): 13.4x the zsdcc C version,
   proven pixel-identical to core/draw_ref.c over a 2716-line corpus
   run under z88dk-ticks (`tests/z80draw/run.sh`, must print Z80DRAW
   IDENTICAL).  Build with `-DDRAW_C_FALLBACK` to swap the C DDA back
   in.  PIXELAD/SETAE are deliberately NOT used: z88dk-ticks executes
   them as NOPs, which would have blinded the verification.
4. **50/60 Hz**: REG_PERIPHERAL_1 read at init; the ISR counts 1
   jiffy/frame at 60 Hz instead of the 6/5 pattern.
5. **Save/load**: ZSAVE/ZLOAD -> `daggorath.sav` via esxDOS.

## Resolved in Phase 4

- Fixed-scene diff vs the desktop shim: 0/6144 bytes differ
  (tests/next-scene/, Phase 2 close-out).
- Clock stopwatch: 3612 jiffies over 61.2 wall seconds in ZEsarUX
  (59.0/s) - the 6/5 frame->jiffy ratio is exact by construction;
  the residual is emulator wall-clock tolerance, not a ratio error.
- Release packaging: `make release` copies daggorath.nex +
  daggorath.sfx + README/CONTROLS to
  ~/retro-computing/spectrum-next/games/Daggorath/.

## Covox volume tiers (post-Phase-4)

The Covox has no level control, so volume is CPU-scaled:
plat_sound_play quantizes the core's 0..255 volume to 9 tiers of the
desktop's volume/255 midline gain.  Tier 8 (>= 224: all full-volume
plays and creature range 1) streams the original sample; tiers 0-7
rebuild a 256-byte LUT (gain tier*32/256, at 0x5B00 in the draw-page
tail after the rasterizer's tables) and copy the sample through
snd_scale.asm (69 T/byte; worst case = the 22050-byte buzz, ~54 ms at
28 MHz) into 3 bounce pages after the blob, cached by (id, tier) so
the fade's 300 ms buzz re-issues and repeats at the same range are
free.  The 8 creature volumes (255,223,...,31 = tier*32-1) map one
tier each; the fade ramp (step*16) crosses a tier every other step.
Scaling maps source pages at MMU1 and bounce pages at MMU0 - safe
because the frame ISR only touches MMU1 while a sound is active and
playback is stopped first (and no fetch happens below 0x4000, so
divMMC never automaps).

Verified three ways:
1. tests/z80scale/run.sh - the REAL snd_scale.asm + the zsdcc-compiled
   LUT builder under z88dk-ticks vs a gcc reference: Z80SCALE
   IDENTICAL (all 8 tiers, odd offsets, varied lengths, len==0 no-op).
2. ZEsarUX --aofile before/after: old build plays the title buzz flat
   at full level; new build ramps 0 -> full in tier steps and mirrors
   back down on the fade-out, kaboom level unchanged (tier-8 path is
   byte-identical to the old code).
3. make check green + fixed scene re-verified PIXEL-IDENTICAL on the
   new binary.

Two ZEsarUX observations from that work (both pre-existing, confirmed
identical on the Phase-2 binary):
- The emulator renders the DMA-paced Covox stream as short ~75 ms
  bursts per 8K chunk (the zxnDMA prescalar does not pace its audio
  mixer), so aofile captures show burst envelopes rather than
  continuous tone.  Levels and sequencing are still faithful; real
  hardware paces by the prescalar.
- core_wait_jiffies phases stretch (the 2.5 s PREPARE hold measures
  ~30 s wall) even though the jiffy counter ticks at a verified
  59.9/s; scheduler-driven gameplay paces correctly.  Not diagnosed;
  worth timing on real hardware.

## Still open

- Real-hardware SD boot (Dave's Next: both files in one SD folder,
  launch the .nex from the Browser - see release/README-next.txt).
- The stretched core_wait_jiffies observation above (emulation only
  so far; check the PREPARE hold on real hardware).
- CSpect second opinion (optional).

## Build & run

    make -C spectrum-next            # daggorath.nex (needs Phase-0 toolchain)
    make -C spectrum-next run        # smartload into ZEsarUX
    make release                     # (repo root) copy the playable set
