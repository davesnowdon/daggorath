# License and provenance

*Dungeons of Daggorath* is (c) 1982 Douglas J. Morgan / DynaMicro, Inc.
The game exists in freely-distributable form because its rights holder
released it under the grant reproduced verbatim below.

## Lineage of this repository

1. **The original game** (6809 assembly, 8 KB Radio Shack cartridge,
   1982) — by Douglas J. Morgan, Phillip C. Landmeier, April Landmeier
   and Keith Kiyohara at DynaMicro.  Released under the grant below;
   the original source listing circulates with the grant attached.
2. **The C++ SDL PC-Port** ("Daggorath PC-Port", Hunerlach lineage) —
   a mechanical translation of that assembly to C++.  Its game logic,
   hex-encoded shape tables and WAV recordings of the CoCo DAC are the
   working inputs to this port.  Its license file restates the Morgan
   grant and keeps the original copyright with Morgan; SDL/SDL_Mixer
   (zlib license) apply only to that port's own build.
3. **This repository** — a portable-C99 re-core verified against BOTH
   ancestors (byte-identical assembly round-trip of the original;
   module-level A/B against the PC-Port; see `docs/ab-protocol.md`),
   plus new backends for the ZX Spectrum Next and MEGA65, tools and
   tests.  The committed PCM sets in `assets/raw-*` are rate-converted
   from the PC-Port's WAVs (see `assets/sfx-src/README.md`).

## Terms for this repository's code

The new material in this repository (the portable core extraction, the
`desktop/`, `spectrum-next/` and `mega65/` backends, `tools/` and
`tests/`) is provided in the same spirit as the grant below: you may
use, modify and distribute it freely, for any purpose, with the sole
request that you exercise every effort to preserve the game itself in
its original and unaltered form — which is why authentic behaviour is
this port's default and any platform enhancement must be opt-in.

No warranty of any kind is given; use at your own risk.

## Grant of license (verbatim)

> Grant of license to reproduce Dungeons of Daggorath
>
> My name is Douglas J. Morgan.  I was the president of DynaMicro,
> Inc. (since dissolved), the company which conceived, created and
> wrote Dungeons of Daggorath, a best selling Radio Shack Color
> Computer adventure game.
>
> I have examined the contract I signed with Radio Shack for their
> license of the game.  The contract provides that Radio Shack shall
> have an exclusive license to manufacture and produce the game, but
> that said exclusive license shall revert to a non-exclusive license
> should Radio Shack cease to produce and sell the game.  To the best
> of my knowledge, they have not produced the game for many years.
> Thus, it is my belief that the right to grant a license for the game
> has reverted to me.
>
> I hereby grant a non-exclusive permanent world-wide license to any
> and all Color Computer site administrators, emulator developers,
> programmers or any other person or persons who wish to develop,
> produce, duplicate, emulate, or distribute the game on the sole
> condition that they exercise every effort to preserve the game
> insofar as possible in its original and unaltered form.
>
> The game was a labor of love.  Additional credits to Phillip C.
> Landmeier - who was my partner and who originally conceived the
> vision of the game and was responsible for the (then) state of the
> art sounds and realism, to April Landmeier, his wife - the artist
> who drew all the creatures as well as all the artwork for the manual
> and game cover, and to Keith Kiyohara - a gifted programmer who
> helped write the original game and then contributed greatly to
> compressing a 16K game into 8K so that it could be carried and
> produced by Radio Shack.
>
> The game did very well for us.  I give it to the world with thanks
> to all who bought it, played it or enjoyed it.
>
> There is one existing copy of the original source code.  Anyone
> willing to pay for the copying of the listing (at Kinko's) and
> shipment to them, who intends to use it to enhance or improve the
> emulator versions of the game is welcome to it.
>
> Verification of this license grant or requests for the listing can
> be made by contacting Louis Jordan,   Thank you.
