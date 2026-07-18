# Spectrum Next backend notes

Status: **complete** - asm rasterizer (draw_z80n.asm, 13.4x), asm RNG,
IM2 ISR, zxnDMA Covox sound with volume tiers, esxDOS saves with the
envelope header, verified in ZEsarUX 13.0 by the scripted harnesses
(tests/next-scene / next-sound / next-save; `make check-all`).  The
"first playable" milestone below stands as history; sections tagged
(first-light) describe that baseline where later sections supersede it.

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
  0xFE directly with per-key edge detection into a 16-slot ring
  (drop-newest when full).
  CAPS+0 maps to backspace.  ZRCP's `send-keys-event` uses ASCII codes
  <128 and is the non-blocking way to inject keys when testing.
- ZEsarUX automation: `--enable-remoteprotocol` (port 10000); useful
  commands: `read-memory <dec-addr> <len>`, `get-registers`,
  `evaluate IN(65278)`, `send-keys-ascii <ms> <codes...>` (blocks ZRCP
  for the duration), `send-keys-event <ascii> <1|0>`.  Addresses are
  decimal.  tests/next-scene/zrcp.py is the shared socket client.

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
   IDENTICAL).  (The untested C-DDA fallback that lived behind
   DRAW_C_FALLBACK was deleted 2026-07-17; core/draw_ref.c is the
   reference.)  PIXELAD/SETAE are deliberately NOT used: z88dk-ticks executes
   them as NOPs, which would have blinded the verification.
4. **50/60 Hz**: REG_PERIPHERAL_1 read at init; the ISR counts 1
   jiffy/frame at 60 Hz instead of the 6/5 pattern.
5. **Save/load**: ZSAVE/ZLOAD -> `daggorath.sav` via esxDOS.  Since
   2026-07-18 the file carries the 8-byte save envelope ["D" "S" ver
   flags len16 fletcher16] validated on load - a truncated, corrupted
   or different-build file is rejected (CMDERR) instead of restored as
   garbage.  The payload stays the compiler-ABI struct dump, so saves
   are per-machine by design (the scheduler is deliberately NOT saved:
   that is the PC-Port reference's own semantics, mirrored).

Expected zsdcc warnings on the core (faithful 8-bit arithmetic, NOT
bugs to fix): `core/dungeon.c:40` constant-conversion overflow and
`core/object.c:356` signed/unsigned comparison.

## Automated harnesses (2026-07-17)

Everything above is now re-runnable (see `make check-all` at the repo
root): tests/next-scene (fixed-scene diff, scripted via ZRCP -
zrcp.py), tests/next-save (byte-exact save/load round trip against the
host-dir esxDOS file), tests/next-sound (--aofile RAW capture
cross-correlated with the PCM masters; boot beeps skipped so a mute
game can't pass on OS noise).  A failed ZSAVE now flashes the border
red, and a missing daggorath.sfx holds it yellow ~1s at startup.

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
desktop's volume/255 midline gain.  Tier 8 (volume == 255 exactly, by
tier=(v+1)>>5: all full-volume plays and creature range 1 - volumes
224-254 land in tier 7 and ARE scaled) streams the sample; tiers 0-7
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
   byte-identical to the old code).  (Now automated: tests/next-sound
   cross-correlates the capture against the PCM masters.)
3. make check green + fixed scene re-verified PIXEL-IDENTICAL on the
   new binary.

Two observations from that work (both pre-existing, confirmed
identical on the Phase-2 binary):
- ZEsarUX renders the DMA-paced Covox stream as short ~75 ms bursts
  per 8K chunk (the zxnDMA prescalar does not pace its audio mixer),
  so aofile captures show burst envelopes rather than continuous
  tone.  Levels and sequencing are still faithful; real hardware
  paces by the prescalar.
- **The "PREPARE!" pause was ~30 s of level-generation CPU time**, not
  a timing bug: creature_NEWLVL -> dungeon_DGNGEN spins rng_RANDOM,
  and the C rng_RANDOM does 8x8 per-bit passes through lsl()/rol()
  FUNCTION CALLS (~90 calls per random byte under zsdcc - PC-sampled
  during the pause: rng_RANDOM/_lsl/_rol dominate; the jiffy counter
  ticks a verified 59.9/s throughout, and core_wait_jiffies itself is
  correct).  RESOLVED by rng_z80.asm (below).  Fade buzz steps still
  run ~1.3x nominal for related reasons (draw + pump cost on top of
  each 18-jiffy wait).

## Asm RNG (post-Phase-4)

rng_z80.asm replaces the C rng_RANDOM on the Next (core/rng.c compiles
it out under -DDOD_RNG_ASM; the C stays the normative reference).  The
whole 8x8-bit C inner loop collapses: the tap count's parity IS the
Z80 P/V flag after `AND 0xE1`, and the three rol()s are three chained
`RL` instructions - ~650 T-states per call (~23 us) vs several
thousand for the C.  The final rng.carry is stored as 0/1 because the
carry byte is part of dumped/saved state.  8-bit zsdcc returns go in L
(verified against 4.5.0 codegen).

Verified: tests/z80rng/run.sh - the REAL rng_z80.asm linked with the
REAL core/rng.c (with the define, mirroring production) under
z88dk-ticks vs the normative C with gcc: Z80RNG IDENTICAL over 20
seeds x 256 calls including SEED/carry state bytes and the degenerate
all-zero seed.  End-to-end: keyless boot trace shows the PREPARE +
level-generation phase collapsing from ~33 s to ~2-3 s, and the fixed
start scene still converges 0/6144 bytes vs the desktop golden (same
dungeon, byte for byte).

## Still open

- Real-hardware SD boot (Dave's Next: both files in one SD folder,
  launch the .nex from the Browser - see release/README-next.txt).
- CSpect second opinion (optional).

## Build & run

    make -C spectrum-next            # daggorath.nex (needs Phase-0 toolchain)
    make -C spectrum-next run        # smartload into ZEsarUX
    make release                     # (repo root) copy the playable set
