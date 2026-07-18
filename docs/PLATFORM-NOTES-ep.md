# Enterprise 64/128 backend notes

Status: **complete** (video + input + timing + Dave DAC sound + saves,
verified in ep128emu 2.0.11.2 against the desktop reference: byte-exact
scene goldens, 60.0 jiffies/s stopwatch, WAV cross-correlation, byte-exact
save round trip).  Toolchain: the same z88dk/zsdcc as the Next port with
`+enterprise -subtype=com`, a bare crt0, and sjasmplus for the loader.

## Big picture

EXOS loads a type-5 `.com` into ONE 16K segment, so a ~500-byte
`loader.com` boots first: it allocates every free segment (EXOS fn 24),
reads `GAME.BIN` into segments FCh/FDh/FEh through the page-1 window,
loads the best-fitting PCM bank (`DAGGOR2.SFX` with >= 8 spare segments,
else `DAGGOR1.SFX`, else silent), writes a handoff block, stashes the
EXOS state (see Saves), kills EXOS, and jumps into the game via a
trampoline in the video segment.

## Memory map (Z80, steady state)

| page | segment | contents                                              |
|------|---------|--------------------------------------------------------|
| 0    | FCh     | 0038h IM1 JP, 00F0h loader handoff, game 0100h-3FFFh  |
| 1    | FDh     | game code 4000h-7FFFh (ISR maps sample segs here)     |
| 2    | FEh     | code tail/data/BSS, volume LUT 9700h, SFX tail 9800h- |
|      |         | BAFFh, stack BB00h-BF00h                              |
| 3    | FFh     | video: FB0 C000h, FB1 D800h, draw tables F000h,       |
|      |         | LPT F800h (Z80 address == Nick address in FFh)        |

Sample banks live in loader-allocated segments (F9h-FBh stock) plus the
loader-segment tail and the FEh tail; the ISR pages the playing sample's
segment into page 1 and restores FDh before RETI (the ISR itself is
linked below 4000h - build-asserted - because page 1 vanishes under it).

## Hard-won lessons

- **Type-5 modules load 16K max.**  EXOS `LOAD` fills only page 0; a
  34.5K image silently arrives truncated.  Hence the loader.
- **The EXOS page map at app time is P0=app, P1=app, P2=FFh, P3=ROM.**
  Two corollaries that each cost a debugging session: the system
  segment's CONTENT is at page 2 (a "stash EXOS state" copy that read
  page 3 stashed 10K of ROM), and **EXOS - calls AND its interrupt
  handler - requires the system segment FFh mapped at page 2** ("page 2
  will always be the system segment containing the stack" is a kernel
  invariant).  Any EXOS interaction with something else at page 2 dies
  on the first interrupt.
- **Nick PIXEL mode fetches two bytes per slot; LPIXEL (mode 0x1E) is
  the 1-byte/slot mode** that gives 256 full-width pixels from a
  32-byte stride.  A uniform test pattern cannot tell them apart.
- **Port B4h writes clear latches through disabled enables** (value is
  XOR 0x55 decoded): writing an enable bit as 0 also clears that
  source's latch, so two-source ISR acks must keep the other enable
  high with its reset bit low (0x13/0x31/0x30/0x10 in isr_ep.asm).
- **The tone interrupt runs at 250000/(n+1)** - the toggle rate, twice
  the audible frequency (probe-verified).  n=33 -> 7352.94 Hz for the
  7350 Hz masters; a 60 Hz idle tone is impossible (n would exceed 12
  bits), so the video interrupt stays the permanent jiffy clock and
  tone-0 interrupts exist only while a sample plays, retuned per sample
  (n=67 half, n=135 quarter rate).  The A7h b0 sync pulse ("hold at
  preset") forces the period reload.
- **The heartbeat needed the port's one guarded core change**
  (`DOD_HEART_STATUS_ONLY`): the core full-redraws per beat, which is
  0.2-0.5 s at 4 MHz.  Under the flag the beat re-blits only the status
  row, to the LIVE buffer (this backend double-buffers).
- **memset/LDIR with both ends in video RAM derails**; discrete stores
  work.  LDIR from normal RAM into video RAM is fine.
- **plat_present must loop on a frame flag**, not a bare HALT: sample
  interrupts wake HALT early.
- **Free-segment reality** (probe): EXOS fn 24 from an app allocates
  F9h-FEh cleanly and offers FFh shared - which must be freed at once
  or every later open fails .NORAM (channel RAM starvation).  F9h-FBh
  survive total overwrite with EXDOS intact.
- **VRAM stretch** is +18.5% on data reads during active display;
  non-video RAM is wait-free in fast mode (BFh = 0Ch).

## Saves: the EXOS dance (dance_ep.asm)

The game steals EXOS's system segment (FFh) for video, so ZSAVE/ZLOAD
resurrect EXOS around each file operation:

1. Boot-time: the loader copies FFh offsets 1800h-3FFFh (channel RAM,
   variables, system stack - overlapping only rebuildable video state)
   into its own segment's tail, right before killing EXOS.
2. Save/load: park Nick on an LPT copy inside FB0 (sync + IRQ records
   keep ticking), restore the stash into FFh, map the system segment to
   page 2 and the loader segment (live EXOS page-zero stub) to page 0,
   run on a small page-1 stack, bounce the blob through the loader
   segment's dead low RAM as page-0 addresses, EI and do EXOS fn 1/2 +
   6/8 + 3 on channel 9.
3. Re-stash (EXOS state evolved), restore the game mapping, rebuild the
   LPT + draw tables + hidden buffer, re-arm Dave, EI.

Jiffies pause during the window; the visible frame keeps showing minus a
112-byte strip until the next repaint (the parked LPT copy's LD1 is
forced to FB0 and the flip parity is canonicalized on exit, so this
holds for BOTH double-buffer parities).  `tests/ep-save/run.sh` proves
the round trip byte-exact - twice per run, at opposite parities.

A failed ZSAVE flashes the border red 3x (the frozen core discards the
save status; ZLOAD reports via CMDERR).  A missing/unreadable SFX blob
holds the border yellow ~1s at boot, then the game runs silent.

Since 2026-07-18 A:DAGGOR.SAV carries the 8-byte save envelope
["D" "S" ver flags len16 fletcher16] (dance stages it at LDR_BUF,
payload at LDR_BUF+8; plat_ep.c builds/validates it).  A truncated,
corrupted or different-build file is rejected (CMDERR) instead of
restored as garbage.  The payload stays the compiler-ABI struct dump -
saves are per-machine by design, and the scheduler is deliberately NOT
saved (the PC-Port reference's own semantics, mirrored).

## Loader failure signals (border colour = raw EXOS status, then hang)

The loader halts with DI + the EXOS status byte on port 81h (DI matters:
EXOS's own ISR repaints the border every frame).  Codes seen in
practice:

| Border byte | Meaning |
|---|---|
| `0xCF` (bright green-ish) | GAME.BIN missing / open failed (EXDOS .NOFIL family) |
| `0xE4` | GAME.BIN truncated - a read-block hit EOF (.EOF) |
| `0xF7` | .NORAM - EXOS out of channel RAM (a held .SHARE grant?) |
| anything else non-zero | look it up: EXOS fn 28 / kernel spec ch. 11 |

## Known limitations

- One save slot, fixed name `A:DAGGOR.SAV`, no volume identity check:
  saving with the "wrong" disk in drive A silently writes to it
  (EXDOS has no volume IDs to check).
- SFX profile is chosen at boot only (stock 3-bin DAGGOR1 vs
  expanded 8-bin DAGGOR2); hot-swapping disks after boot changes
  nothing.

## Verification

- `tests/z80draw-ep`: rasterizer identity vs core/draw_ref.c under
  z88dk-ticks `-mz80` (3303 lines, byte-exact; 14.9x vs compiled C).
- `tests/z80rng-ep`: RNG identity for the shared rng_z80.asm under the
  plain-Z80 EP toolchain flavour (the Next's z80n variant is covered by
  tests/z80rng).
- `tests/ep-scene`: attract-demo framebuffers byte-exact vs desktop
  goldens (quick-snapshot LPT-front extraction), + the 60.0 jiffies/s
  stopwatch - re-run green WITH sound playing.
- `tests/ep-sound`: WAV capture cross-correlates with the sample
  masters (both title-fade buzz ramps at r=0.67) + spectral rate check;
  runs twice - stock/DAGGOR1 and 384K/DAGGOR2, asserting the loader's
  profile byte from the handoff block.
- `tests/ep-probe`: the Dave-chip characterization (re-run this on real
  hardware before trusting the sound constants there).
- `tests/ep-save`: ZSAVE file == in-RAM blob (twice, opposite buffer
  parities); seeded-file ZLOAD round-trips byte-exact on a fresh boot.
- `tests/ep-loadfail`: missing and truncated GAME.BIN halt with the
  error border (NICK_STATE-chunk assertion), never boot a partial
  image.

All of the above run from the repo root via `make check-all`.
