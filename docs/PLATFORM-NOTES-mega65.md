# MEGA65 backend notes

Status: **playable with sound, pixel-verified in xemu** (all C).
Full flow confirmed on the emulated MEGA65 (open-roms): title fade +
buzz -> attract demo (shield/sword/attack) -> key abort -> real game ->
classic opening (P R T / U R lights the dungeon) -> heartbeat +
creature sounds with distance volume.  Fixed start scene is
pixel-identical to the desktop shim (tests/mega65-scene, 0/8000
bytes).  Not yet on real hardware.

## Memory map

| range              | contents                                        |
|--------------------|-------------------------------------------------|
| $0002-$0090        | llvm-mos zero page (imaginary regs, zp data)    |
| $0200-$1FFF        | soft stack (repointed to grow down from $2000)  |
| $0314/15           | IRQ RAM vector -> irq.s handler                 |
| $2001-$CAxx        | PRG: code + rodata + data + BSS (6K shadow fb)  |
| $12000-$17FFF      | SFX region B (24K)                              |
| $18000-$19FFF      | VIC bitmap (8K, CHARPTR)                        |
| $1A000-$1A3E7      | colour matrix (SCRNPTR), $10 = white on black   |
| $20000-$3FFFF      | ROM-in-RAM copy - NEVER overwrite (kernal IRQ   |
|                    | stub and hyppo helpers live here)               |
| $40000-$5FFFF      | SFX region A (128K)                             |

The llvm-mos link region is $2001-$CFFF with the soft stack at $D000 -
which BSS reaches once the core + framebuffer are in.  main() repoints
the soft stack pointer (__rc0/__rc1) to $2000 before anything runs.

## Video

Legacy VIC-II hires bitmap mode (BMM), VIC-IV-relocated: CHARPTR ->
$18000, SCRNPTR -> $1A000 (set AFTER disabling hot registers $D05D.7;
legacy D011/D016/D018 writes first, while hot regs still recompute
linestep/chrcount).  The core draws into a bank-0 shadow framebuffer
holding just the 256x192 window in the same 8x8 cell interleave
(rowbase[y] = (y>>3)*256 + (y&7)); plat_present() is 24 row-block
DMAgic copies into the 320-wide bitmap at cell column 4.  ~0.5 ms at
40 MHz, single buffer, no tearing seen.

## Hard-won lessons

- **The DMAgic list struct must be `volatile`.**  The hardware reads
  the list from RAM when the trigger register is poked; the trigger
  POKEs are volatile but plain stores into the list are not, so LTO
  deferred/merged them and the init fills fired a stale list.
  Symptom: the colour matrix silently kept the ROM boot screen's $20s
  (red-ish text instead of white) and bank 1 got sprayed - invisible
  in monochrome memory dumps until row 24 showed dotted garbage.
- **$D615 virtual keys take MATRIX SCANCODES, not ASCII** (write
  scancode = press, $7F = release; RETURN = $01).  The $D610 hardware
  ASCII queue is the read side and buffers keys during CPU-bound
  stretches - no ISR scan needed, unlike the Next.
- **IRQ via the $0314 RAM vector**: both open-roms and the closed ROM
  push A,X,Y then JMP ($0314); our handler (irq.s) acks $D019, counts
  the jiffy (6/5 accumulator, NTSC 1:1 per $D06F.7) and unwinds those
  pushes itself with PLA/TAY/PLA/TAX/PLA/RTI - the ROM's own IRQ work
  never runs.  CIA1/CIA2 interrupt sources are disabled at init.
- **Any substitute IRQ handler must ACK the sources** (LDA $DC0D, and
  $FF -> $D019): an un-acked interrupt re-fires forever and the CPU
  lives in the ROM's IRQ entry (~zero progress, PC hovering around
  $E57x with open-roms).
- **llvm-mos crt0 calls the KERNAL**: the mega65 platform libc chains
  a charset-shift init (LDA #$0E, JSR $FFD2) before main - the C64
  KERNAL environment must be functional (IRQs live) until plat_init
  takes over.  Don't enter _start with I set.
- **int is 16-bit** (found in the ab-mos step): the core is
  stdint-clean, but watch host-tool habits in new test code.
- Audio DMA: the top-address compare is 16-bit, so **a sample must
  not cross a 64K page** - the loader first-fits samples into the
  free regions, page-hopping as needed.  xemu fetches sample data
  from chip RAM only (384K), matching real hardware (24-bit channel
  addresses can't reach attic RAM).
- mos-sim's libc has no file I/O (that's fine: `make -C tests ab-mos`
  needs none; anything else should embed its data).

## Sound

Audio DMA channel 0, one-shot per plat_sound_play (single-voice
preemption like the CoCo DAC): current-address regs $D72A-C get the
sample start, top $D727-8 its 16-bit end, step $D724-6 = $0BE5
(7350 Hz at 40.5 MHz), volume $D729 straight from the core - so
snd_creature()'s distance attenuation WORKS here, unlike the Covox.
Control $A2 = enable | unsigned->signed | 8-bit; the channel sets bit
3 when it hits the limit, which is plat_sound_playing()'s answer.

The blob (DAGGOR65.SFX, 135481 bytes = 26 sounds, u8 7350 Hz,
tools/gen_sfxbin.py rate arg; raws = wav2raw factor 3) is streamed at
init via hyppo file reads (mega65-libc's llvm fileio.s) through the
framebuffer (doubles as bounce buffer before first draw) into the SFX
regions.  It must sit in the SD card's FAT root - hyppo does not read
the mounted D81.

## Verification so far (mirrors the Next ladder)

1. `make -C tests ab-mos`: whole module A/B harness CRC-identical on
   simulated 6502 (llvm-mos codegen proven before the backend).
2. Raster IRQ measured ~60 jiffies/s (PAL 6/5 accumulator).
3. Classic opening lights the dungeon with correct distance-faded
   wireframe; status-line progression matches the desktop s2 golden.
4. tests/mega65-scene: fixed start scene 0/8000 bytes vs desktop -
   scripted since 2026-07-17 (`./run.sh`, headless xemu + uartmon).
5. Title buzz, heartbeat (both 25-byte heart samples alternating) and
   creature sounds verified through the channel registers in xemu -
   automated as tests/mega65-sound (snd_ok, blob placement bytes,
   channel-0 armed with the exact 7350 Hz phase step).
6. Attract demo runs unattended (shield/sword/attack sequence).

7. Save games (Phase 4): ZSAVE runs open/write512/close through
   hyppo's writefile trap (hyppo_write.s - the mirror of read512,
   overwrite-in-place of the pre-sized DAGGOR65.SAV); a ZLOAD
   round-trip in xemu restored the saved torch-lit game.  Automated as
   tests/mega65-save (0xAA-poisoned round trip + cross-boot seeded
   load, byte-exact).  Since 2026-07-18 the slot carries the 8-byte
   save envelope ["D" "S" ver flags len16 fletcher16] in sector 0
   (header + first 504 payload bytes via the bounce); a truncated,
   corrupted or different-build file is rejected (CMDERR), and the
   final partial sector goes through the zeroed bounce so no stray BSS
   bytes leak into the slot.  A failed ZSAVE flashes the border red; a
   missing/short DAGGOR65.SFX holds it yellow ~1s at startup (snd_ok
   is map-visible for monitor diagnosis).
8. Clock stopwatch (Phase 4): 3639 jiffies over 61.2 wall seconds in
   PAL xemu (59.4/s) and 59.94/s in NTSC mode (-videostd 1), i.e.
   both variants of the frame->jiffy conversion are right; the
   residual is emulator wall-clock tolerance, not a ratio error.

Still open: real-hardware boot (Dave's MEGA65 - see release/
README-mega65.txt for the SD layout).  Future enhancement ideas kept
deliberately out of V1 (authentic mode only): per-creature colour via
the matrix, stereo panning across the four Audio DMA channels,
phosphor-colour palette toggle.

## xemu automation (mega65/m65mon.py)

xemu (built at mega65/emulators/xemu) needs a ROM: open-roms'
prebuilt `bin/mega65.rom` (cloned at mega65/dev/open-roms) via
`-rom`, plus `-sdimg DIR -virtsd` for an in-memory FAT fed from a
host directory (put DAGGOR65.SFX there).  First boot per xemu start
runs the MEGA65 on-boarding utility: one RETURN (`cmd @key:1`)
accepts it; the FPGA-reconfig dialog auto-answers Yes in -headless.
open-roms then sits on its boot banner - useless for -prg injection,
which is why m65mon.py exists:

    m65mon.py SOCK inject daggorath.prg    # upload+verify+run via -uartmon
    m65mon.py SOCK cmd m19000 @key:1 ...   # monitor cmds, keys, dumps

It parks $0314 on an acking stub before uploading (see lessons), and
bootstraps SP before jumping at the SYS address.  Screen grabs =
`M` dumps of $18000 de-interleaved ((y>>3)*320+(x>>3)*8+(y&7)).
With a real MEGA65.ROM in ~/.local/share/xemu-lgb/mega65/ the stock
`-prg` auto-typing flow would work too.

## Build & run

    make -C mega65                  # daggorath.prg (+ DAGGOR65.SFX)
    make -C mega65 daggorath.d81    # distributable image (self-verified)
    make -C mega65 run              # xemu (interactive window)

Real hardware: copy DAGGOR65.SFX into the SD card's FAT root, then
either mount daggorath.d81 and RUN"DAGGORATH", or load the PRG
directly from the freezer's file browser.
