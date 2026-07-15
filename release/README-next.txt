DUNGEONS OF DAGGORATH for the ZX Spectrum Next
================================================

Files
  daggorath.nex   the game (Z80n, 28 MHz, needs a Spectrum Next or
                  N-go core; runs under NextZXOS)
  daggorath.sfx   sound sample bank - MUST sit in the same folder as
                  the .nex (loaded via esxDOS at startup; the game
                  runs silently without it)
  CONTROLS.txt    command crib (full manual: the original Tandy PDF)

Running
  Copy both files anywhere on the SD card and launch daggorath.nex
  from the NextZXOS Browser.  50 Hz and 60 Hz display modes both run
  at the correct authentic speed (60 Hz game clock either way).

Saves
  ZSAVE writes daggorath.sav next to the .nex (one slot); ZLOAD
  restores it.

Notes
  Audio is played through the Covox/Next DAC by the zxnDMA - no CPU
  cost, but no volume control either, so creature sounds don't fade
  with distance on this platform.  Rendering is pixel-identical to
  the 1982 original (the line rasterizer is an exact port of the
  cartridge's VECTOR.ASM, dot-fades included).
