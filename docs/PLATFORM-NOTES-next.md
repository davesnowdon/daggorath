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
   Verified in ZEsarUX via `--aofile` capture.  LIMITATION: the Covox
   has no level control, so creature-distance volume is flat for now.
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

## Still open

- Volume scaling (pre-scaled sample tiers, or a CPU scale into a
  bounce page at play time).
- Fixed-scene screenshot diff vs the desktop shim; 60 s heartbeat
  stopwatch check; CSpect second opinion; real-hardware SD boot
  (copy daggorath.nex + daggorath.sfx into the same SD folder).

## Build & run

    make -C spectrum-next            # daggorath.nex (needs Phase-0 toolchain)
    make -C spectrum-next run        # smartload into ZEsarUX

Verification next steps: fixed-scene screenshot diff vs the desktop
shim, 60 s heartbeat-count timing check, CSpect second opinion, real
hardware from SD.
