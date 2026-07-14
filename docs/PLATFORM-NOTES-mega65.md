# MEGA65 backend notes

Status: **core codegen proven, backend not yet started.**

## Done (2026-07-14)

- `make -C tests ab-mos`: the whole module-level A/B harness (dungeon,
  parser, object, creature, scheduler, player math) compiled with
  `mos-sim-clang -Os` and run on a simulated 6502 produces CRCs
  identical to the host build.  The core's game logic is proven under
  llvm-mos codegen before any backend code exists.
- Portability catch while getting there: `int`/`unsigned` are 16-bit
  on mos (the harness's CRC32 was silently truncated - fixed with
  uint32_t + PRIX32; the CORE was already stdint-clean).  Watch for
  the same in any future test that assumes 32-bit int.
- mos-sim's libc has no file I/O (`fopen` in test_rng/test_draw/
  test_scale links fail with __sysremove/__osmaperrno).  To run those
  under mos-sim, embed the goldens as C arrays (not yet done; ab-mos
  covers far more of the core anyway).

## Toolchain facts

- llvm-mos at `~/retro-computing/mega65/dev/llvm-mos`
  (`mos-mega65-clang`, `mos-sim-clang`, `mos-sim`); mega65-libc cloned
  next to it; xemu built at
  `~/retro-computing/mega65/emulators/xemu/build/bin/xmega65.native`
  - **still needs a MEGA65 ROM** (open-roms or Dave's own) before any
  emulator work.

## Plan (from the approved project plan, unchanged)

- Video: VIC-II 320x200 hires bitmap, 256x192 window at origin (32,4);
  192-entry rowbase LUT for the 8x8 cell interleave; C rasterizer is
  expected to suffice at 40 MHz (draw_ref compiles as-is; asm only if
  visibly slow).  Double buffer via two 8K bitmaps + 0xD018/0xDD00
  flip.  VIC-IV unlock knock 0xD02F <- 'G','S'; 40 MHz via libc.
- Timing: raster-compare jiffy source; PAL 50 Hz reuses the 6/5
  accumulator pattern (see spectrum-next/isr.asm), NTSC 60 Hz = 1:1.
  Polling fallback if the llvm-mos IRQ attribute misbehaves.
- Input: hardware ASCII key queue at 0xD610 IS plat_poll_key (and,
  unlike the Next, it queues in hardware - no ISR scan needed).
- Sound: MEGA65 Audio DMA channels (0xD720+ block: 28-bit address,
  frequency, per-channel volume) - and volume DOES map from creature
  RANGE here, unlike the Covox.  Reuse tools/gen_sfxbin.py blob
  (11025 Hz u8); load from .d81 via hyppo/libc file API into attic/
  chip RAM.
- Build: mos-mega65-clang -Os -flto -> .prg; .d81 via m65tools or
  cc1541; test loop in xemu; real HW via m65 -F -r.
- Verification order (mirror the Next): boots + title fade in xemu ->
  keyboard -> full game -> fixed-scene diff vs desktop (dump the
  bitmap through xemu's debug interface or a memory monitor) ->
  sound -> real hardware.
