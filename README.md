# Dungeons of Daggorath — Spectrum Next & MEGA65 port

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
| `desktop/` | SDL2 reference backend + verification tooling (`--pattern`, `--record/--replay`, `--turbo`, `--headless`, `--screenshot`) |
| `spectrum-next/` | z88dk (zsdcc) + Z80n backend → `daggorath.nex` *(Phase 2)* |
| `mega65/` | llvm-mos backend → `daggorath.prg`/`.d81` *(Phase 3)* |
| `tools/` | table generators (dual-source verified), golden generators, wav2raw |
| `tests/` | host unit tests vs goldens from the C++ port's own compiled code |
| `assets/` | PCM sound effects converted from the port's WAVs |
| `docs/` | extraction conventions, wait-site audit, platform notes |

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

## Build & test (desktop)

```sh
cd tests && make golden && make     # unit tests (goldens need the C++ port source present)
cd desktop && make && ./daggorath --pattern   # render verified tables
```

## Toolchains (Phase 0, all set up)

- Next: z88dk v2.4 **built from source** (incl. zsdcc/SDCC 4.5.0 with the
  z80n port) at `~/retro-computing/spectrum-next/dev/toolchains/`,
  sjasmplus 1.23.1, ZEsarUX 13.0 rebuilt.
- MEGA65: llvm-mos-sdk v23.0.1 (`mos-mega65-clang`, `mos-sim`),
  mega65-libc, xemu built from source at `~/retro-computing/mega65/`.

## Provenance

Game code/data derive from the original released by rights-holder
Douglas J. Morgan (grant of license archived in
`../../disassembly/daggorath/original-source/`); the C++ port lineage is
credited in its sources. This port keeps the authentic mode default;
platform enhancements are opt-in toggles (Phase 4).
