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

## Deliberate first-light simplifications (to revisit)

1. **Sound stubbed** (`plat_sound_playing()` = 0, so `snd_wait_done`
   returns instantly).  Plan: samples in 8K pages + zxnDMA to the DAC.
2. **Single-buffered ULA drawing** - full-screen redraws are visible
   (the core redraws the whole 3D view per update).  Plan: draw to the
   bank-7 shadow screen and flip 0x7FFD bit 3 in vblank.
3. **C rasterizer** (exact draw_ref DDA with 24.8 math) - correct but
   slow; a room redraw takes visible fractions of a second at 28 MHz.
   Plan: Z80n draw.asm (PIXELAD/SETAE/PIXELDN, MUL D,E) verified
   against draw_ref pixel-for-pixel.
4. **50 Hz assumed** - 60 Hz HDMI modes will run ~20% fast until the
   video-timing nextreg is consulted.
5. **No save/load** (PLAT_ERR_UNSUPPORTED) - esxDOS wiring later.

## Build & run

    make -C spectrum-next            # daggorath.nex (needs Phase-0 toolchain)
    make -C spectrum-next run        # smartload into ZEsarUX

Verification next steps: fixed-scene screenshot diff vs the desktop
shim, 60 s heartbeat-count timing check, CSpect second opinion, real
hardware from SD.
