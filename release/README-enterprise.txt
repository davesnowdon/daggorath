DUNGEONS OF DAGGORATH for the Enterprise 64/128
=================================================

Files
  LOADER.COM        the loader (an EXOS type-5 module; start THIS one)
  GAME.BIN          the game image the loader spreads across segments
  DAGGOR1.SFX       sound bank for a stock 128K machine
  DAGGOR2.SFX       sound bank for machines with expanded RAM
                    (everything at full sample rate; picked
                    automatically when >= 8 spare segments exist)
  daggorath-ep.img  all of the above on a 720K FAT12 floppy image
                    (for emulators; the loose files are for SD cards)
  CONTROLS.txt      command crib (full manual: the original Tandy PDF)

Requirements
  An Enterprise 128 (or 64 with a RAM expansion: the game itself needs
  the four 64K-block segments plus RAM for the loader).  EXDOS or an
  EXDOS-compatible filesystem ROM - the SD Adapter (Premium) works.

Running
  Real hardware + SD adapter: copy the four loose files into one folder
  of a FAT-formatted SD card, insert the card BEFORE power-on, then
  from IS-BASIC:
      LOAD "F:LOADER.COM"
  (use your adapter's drive letter; the F1 file browser works too).
  ep128emu: mount daggorath-ep.img as floppy A and
      LOAD "A:LOADER.COM"
  The loader reads GAME.BIN and the best-fitting sound bank, then
  starts the game.  If no .SFX file fits or is present, the game simply
  runs silent.

Saves
  ZSAVE writes DAGGOR.SAV next to the game (one slot); ZLOAD restores
  it - both work mid-game.  The screen blinks during the disk access:
  the game briefly resurrects EXOS (whose memory it stole for the
  video buffers) and rebuilds the display afterwards.  This is normal.

Notes
  Audio is played sample-by-sample through the Dave chip's 6-bit DACs
  from an interrupt - the CoCo fed its DAC from the CPU the same way.
  Distance volume (creature sounds fading with range, the title buzz
  ramp) is a lookup-table conversion on every byte.  On a stock 128
  the long creature voices are stored at reduced sample rate to fit;
  with expanded RAM everything plays at full rate.  Rendering is
  pixel-identical to the 1982 original (the line rasterizer is an
  exact port of the cartridge's VECTOR.ASM, dot-fades included).
