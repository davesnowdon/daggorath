# Core extraction conventions (C++ port -> portable C99)

Source: `../../native-port/DungeonsOfDaggorath/src/*.cpp` (read-only).
Authority for original widths/constants: `../../disassembly/daggorath/original-source/*.ASM`.

## Rules

1. **C99 only.** Compiles with `gcc -std=c99 -Wall -Wextra -Werror -pedantic -c`.
   No includes besides `<stdint.h>`, `<stddef.h>` (if needed) and core headers.
   NO SDL, NO stdio, NO malloc, NO float/double anywhere in `core/`.
2. **Classes become structs + free functions.**
   `class Dungeon` -> `typedef struct { ... } dungeon_state;` plus
   `extern dungeon_state dungeon;` (defined in the .c). Methods become
   `void dungeon_DGNGEN(void)` etc. — keep the ORIGINAL routine names
   (they cross-reference the annotated 6809 disassembly).
3. **Narrow the types.** The port widened many 8/16-bit fields to `int`.
   Use `dodBYTE`/`dodSHORT`/`int8_t`/`jiffy_t` per `core/dod_types.h` and the
   original ASM widths. `-1` index sentinels -> `DOD_NONE` with `int8_t`.
4. **Time is jiffies (60 Hz).** Any millisecond constant inherited from the
   port must be replaced by the ORIGINAL jiffy count from the ASM source
   (DTABAS.ASM for creature move/attack rates, COMDAT.ASM for task rates).
   Document every conversion in a table in your module's header comment.
   `SDL_GetTicks()` -> `plat_jiffies()` with delta arithmetic:
   `(jiffy_t)(now - then) >= interval`.
5. **Sound**: `Mix_PlayChannel(ch, xxxSound[i], 0)` becomes
   `snd_play(SND_XXX, volume)`; creature sounds with distance/panning become
   `snd_creature(sound_id, range)`. Both declared in `core/sound.h` —
   implemented later by glue; just call them.
6. **Randomness**: `rng.RANDOM()` -> `rng_RANDOM()` (`core/rng.h`).
7. **Cross-module calls**: use the skeleton headers (`core/player.h`,
   `core/sched.h`, `core/viewer.h`, `core/game.h`, ...). If a needed
   prototype is missing, ADD it to the right header with a
   `/* TODO(impl): */` comment and list it in your final report.
8. **No busy-waits.** If the module contains `while (SDL_GetTicks() < x)`
   loops or `SDL_PollEvent` pumping (player.cpp/dodgame.cpp only), call
   `core_wait_jiffies(n)` / `core_pump()` from `core/sched.h` instead.
9. **Statics & data tables**: in-class initialized tables move to
   `static const` arrays in the .c file when private, or extern in .h when
   shared. Keep original table names (MOVTAB, CMXLND, ...).
10. **File size**: <800 lines per .c. Split as directed per module.
11. **Fidelity first**: do not "improve" logic, reorder comparisons, or fix
    apparent bugs — byte-level behavioral identity with the C++ port is the
    acceptance test (state dumps must match at jiffy granularity). When the
    port and the ASM disagree, match the PORT and flag it in your report.
12. **Comments**: keep the port's routine-level comments where they explain
    game logic; drop history/noise. Reference ASM modules like
    `/* CRETUR.ASM: creature move task */` at function level.

## Compile check (must pass before you finish)

    cd ports/daggorath/core && gcc -std=c99 -Wall -Wextra -Werror -pedantic -c <yourfiles>.c

## State-dump contract (for later A/B testing)

Every state struct must be a flat POD so `memcpy` snapshots work; no
pointers inside state structs — use int8_t indices (the original used
pointers-as-offsets; the port already stores indices for most).
