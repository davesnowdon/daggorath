DUNGEONS OF DAGGORATH for the MEGA65
=====================================

Files
  daggorath.d81   disk image holding the game program (DAGGORATH)
  daggorath.prg   the same program as a bare PRG (45GS02, 40 MHz)
  DAGGOR65.SFX    sound sample bank - MUST go in the SD card's FAT
                  root (hyppo loads it at startup; the game runs
                  silently without it)
  DAGGOR65.SAV    empty pre-sized save slot - also goes in the SD
                  card's FAT root (hyppo can only overwrite existing
                  files, so ZSAVE needs it there to work)
  CONTROLS.txt    command crib (full manual: the original Tandy PDF)

Running
  1. Copy DAGGOR65.SFX and DAGGOR65.SAV into the ROOT of the SD card.
  2. Either mount daggorath.d81 (F8 freezer/browser or MOUNT) and
       RUN "DAGGORATH"
     or load daggorath.prg directly from the freezer's file browser.
  PAL and NTSC both run at the correct authentic speed.

Saves
  ZSAVE overwrites DAGGOR65.SAV in place (one slot); ZLOAD restores
  it.  If the file is missing from the SD root, ZSAVE reports an
  error and the game carries on.

Notes
  Sound plays through the MEGA65's Audio DMA with true per-sound
  volume, so creatures really do get louder as they close in - listen
  for them.  Rendering is pixel-identical to the 1982 original (the
  line rasterizer is an exact port of the cartridge's VECTOR.ASM,
  dot-fades included).
