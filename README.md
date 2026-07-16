# Dungeons of Daggorath — Spectrum Next, MEGA65 & Enterprise 128 port

A portable-C99 re-core of *Dungeons of Daggorath* (1982, DynaMicro), built
from two verified sources:

- the **original 6809 assembly** (rights-holder-released; assembles
  byte-identical to the 8 KB cartridge — see `../../disassembly/daggorath/`),
- the **C++ SDL2 port** (Hunerlach lineage), whose game logic descends
  from a mechanical translation of that assembly.

The portable core keeps the original's data and math *exactly*: the
24-bit LFSR RNG (identical dungeons), the packed 5-bit text and font,
the byte vector shape tables, radix-7 (`>>7`) scaling, the VECTOR.ASM
dot-period fades, and the 60 Hz jiffy scheduler. Backends implement 13
`plat_*` functions (see `core/platform.h`).

## Layout

| dir | contents |
|---|---|
| `core/` | portable game core (C99, no OS/float/heap; strict `-Werror -pedantic`) |
| `desktop/` | SDL2 reference backend + verification tooling (`--pattern`, `--record/--replay`, `--turbo`, `--headless`, `--screenshot`, `--dump-state`) |
| `spectrum-next/` | z88dk (zsdcc) + Z80n backend → `daggorath.nex` + `daggorath.sfx` — **complete** (28 MHz, Z80n asm rasterizer + asm RNG, zxnDMA→Covox sound w/ CPU-scaled distance volume, IM2 jiffies, esxDOS saves) |
| `mega65/` | llvm-mos backend → `daggorath.prg`/`.d81` + `DAGGOR65.SFX`/`.SAV` — **complete** (40 MHz, VIC-IV bitmap, Audio DMA w/ distance volume, hyppo saves; `m65mon.py` drives xemu) |
| `enterprise/` | z88dk (zsdcc) + Z80 backend → `loader.com` + `game.bin` + `DAGGOR1/2.SFX` + `daggorath-ep.img` — **complete** (4 MHz, Nick LPIXEL double-buffer, plain-Z80 asm rasterizer + asm RNG, Dave 6-bit DAC PCM w/ LUT distance volume, IM1 jiffies, EXOS-dance saves) |
| `tools/` | table generators (dual-source verified), golden generators, wav2raw, gen_sfxbin, gen_sfxep, make_d81, make_fat12 |
| `tests/` | unit tests, module A/B, scenario goldens, coverage, `ab-mos`, z80 asm identity tests (`z80draw/`, `z80draw-ep/`, `z80scale/`, `z80rng/`), fixed-scene diffs (`next-scene/`, `mega65-scene/`, `ep-scene/`), EP sound/save/hardware checks (`ep-sound/`, `ep-save/`, `ep-probe/`) |
| `assets/` | PCM sound effects converted from the port's WAVs |
| `docs/` | extraction conventions, wait-site audit, A/B protocol, platform notes |
| `release/` | player-facing READMEs + command crib for `make release` |

## Verification approach

1. **Table generators** parse BOTH the C++ port's hex tables AND the
   original ASM, expand the original vector-list VM (SVORG/SVECT deltas,
   V$JMP/V$JSR chains) and fail on mismatch. All 37 shape tables verified;
   one genuine port transcription bug found and corrected to the ROM value
   (ceiling line x: 209 → 210).
2. **Unit tests** (`tests/`) compare the core against goldens generated
   from the port's own compiled classes (RNG streams) and against
   hand-derived 6809 semantics (rasterizer, scaling, fades).
3. **A/B protocol**: recorded input scripts replayed through both this
   core (desktop backend) and the original C++ port, comparing state
   dumps at jiffy checkpoints and screenshots.

Known deliberate divergences from the C++ port (the 6809 is our
authority, not the port): radix-7 `>>7` scaling instead of float `/127`;
dot-period fades instead of gray lines; original 5-bit glyph font instead
of the port's vector font; ceiling-line ROM fix.

## Build & test

```sh
make                # desktop + Next (.nex) + MEGA65 (.prg/.d81) + Enterprise (.img)
make check          # the full gate: unit tests, module A/B, scenario goldens
make release        # copy playable sets to the per-system folders
```

### Prerequisites

| target | needs |
|---|---|
| `make desktop` | gcc, SDL2 dev headers |
| `make next` | z88dk with zsdcc/z80n (default path below; override `TOOLCHAIN=` in `spectrum-next/Makefile`), python3 |
| `make mega65` | llvm-mos-sdk + mega65-libc (override `LLVM_MOS=`/`LIBC65=` in `mega65/Makefile`), python3 |
| `make enterprise` | the same z88dk as `make next` plus sjasmplus (paths in `enterprise/Makefile` / `enterprise/build-loader.sh`), python3 |
| `make check` | gcc, python3, SDL2 (scenario replays run the desktop shim) |

The committed `assets/raw-*` PCM sets are the sound inputs — nothing
external is needed to build (their provenance: `assets/sfx-src/README.md`).
One verification layer is optional: module A/B (`tests/Makefile`
`ab-modules`) diffs against the **C++ PC-Port compiled verbatim** and
expects its checkout at `../../../native-port/DungeonsOfDaggorath/`;
without it that diff is skipped with a warning (the rest of the gate
still runs).  The z80 asm identity tests (`tests/z80draw/run.sh`,
`tests/z80scale/run.sh`, `tests/z80rng/run.sh`) and `tests ab-mos`
additionally need the Next and MEGA65 toolchains respectively
(`tests/z80draw-ep/` reuses the Next toolchain under plain `-mz80`);
emulator-based checks (`tests/next-scene/`, `tests/mega65-scene/`,
`tests/ep-scene/`, `tests/ep-sound/`, `tests/ep-save/`) need
ZEsarUX / xemu / ep128emu.

Per-platform details (memory maps, hardware lessons, emulator
automation): `docs/PLATFORM-NOTES-next.md`, `docs/PLATFORM-NOTES-mega65.md`,
`docs/PLATFORM-NOTES-ep.md`; the verification story is
`docs/ab-protocol.md`.  All three backends render the fixed reference
scene pixel-identical to the desktop core (`tests/next-scene/`,
`tests/mega65-scene/`, `tests/ep-scene/`), whose rasterizer is the
verified VECTOR.ASM port.

## Toolchains (Phase 0, all set up)

- Next: z88dk v2.4 **built from source** (incl. zsdcc/SDCC 4.5.0 with the
  z80n port) at `~/retro-computing/spectrum-next/dev/toolchains/`,
  sjasmplus 1.23.1, ZEsarUX 13.0 rebuilt.
- MEGA65: llvm-mos-sdk v23.0.1 (`mos-mega65-clang`, `mos-sim`),
  mega65-libc, xemu built from source at `~/retro-computing/mega65/`.
- Enterprise: the Next's z88dk (`+enterprise` target) + sjasmplus;
  ep128emu 2.0.11.2 (prebuilt, FLTK 1.3.11 built into `~/.local`) at
  `~/retro-computing/elan-enterprise/`.

## Provenance & license

See [LICENSE.md](LICENSE.md): the game is (c) 1982 Douglas J. Morgan /
DynaMicro, distributed under his archived grant of license (vendored
there verbatim, with the full lineage: original 6809 source → C++
PC-Port → this re-core).  The new port code is free to use, modify and
distribute under grant-mirroring terms.  Honouring the grant's
"preserve the game unaltered" condition is why authentic behaviour is
this port's default and platform enhancements must be opt-in.
