# Wait-site audit: port busy-waits -> core_wait_jiffies

The C++ port embeds `ticks1 = SDL_GetTicks(); do { CLOCK-if-due; ... }
while (curTime < ticks1 + DELAY_MS)` loops inside command handlers.
Each becomes ONE `core_wait_jiffies(J)` call (or the `_abortable`
variant when the loop body checks `game.AUTFLG && !game.demoRestart`
and returns early).  `Mix_Playing` drains become `snd_wait_done()`, or
the local `snd_wait_abortable()` helper (player_cmds.c, same shape as
creature.c's) when the port's drain carried the demo-abort early return.

Fidelity notes:
- These waits tick ONLY the CLOCK task (heartbeat/keyboard), never the
  full scheduler - creatures do not move during animations.
  `core_wait_jiffies` reproduces exactly that (see core/sched.c).
- `scheduler.EscCheck()` calls inside the loops are shell UI (menus) and
  are dropped from core; the desktop shim handles ESC itself.
- The port's `while(SDL_PollEvent(&event));` event-queue flushes
  (player.cpp 411-414, 749-752, 1147-1150, 1199-1202) are shell
  concerns and are dropped; the parser keyboard-ring flush in HUPDAT's
  faint (`parser.KBDHDR = KBDTAL = 0`) IS kept - it is game state.
- Four port sites were RAW `SDL_GetTicks` spins that did not tick CLOCK
  at all (PATTK wizard-image pause, PINCAN winner pauses x2 - flagged
  "raw" below).  They use `core_wait_jiffies`, which does tick CLOCK;
  the only effect is that the heartbeat keeps running through those
  0.5 s pauses.

## Delay constants

| Port constant | ms   | jiffies | core name       | where defined |
|---------------|------|---------|-----------------|---------------|
| faint fade    | 750  | 45      | WAIT_FAINT_STEP | sched.h |
| wizDelay      | 500  | 30      | WAIT_WIZARD     | sched.h |
| moveDelay     | 25   | 2       | WAIT_MOVE       | sched.h (UNUSED by player: every PMOVE wait in the port is moveDelay/2) |
| moveDelay/2   | 12   | 1       | WAIT_HALF_MOVE  | sched.h |
| turnDelay     | 20   | 1       | WAIT_TURN       | sched.h |
| prepPause     | 2500 | 150     | WAIT_PREP_PAUSE | player_move.c (= game.c's WAIT_PREPARE) |

(ms -> jiffies at 60 Hz, rounded to nearest with minimum 1; the
sub-jiffy 12/17/20/25 ms values were already jiffy-order in the
original.)

## Sites (player.cpp, FINAL - one row per site, verified against source)

| Port line | function (context)               | port ms / wait      | jiffies | core call                              | notes |
|-----------|----------------------------------|---------------------|---------|----------------------------------------|-------|
| 362-372   | HUPDAT faint dim step            | 750                 | 45      | `core_wait_jiffies(WAIT_FAINT_STEP)`   | per RLIGHT step until 248 (-8); EscCheck dropped |
| 391-401   | HUPDAT faint recover step        | 750                 | 45      | `core_wait_jiffies(WAIT_FAINT_STEP)`   | until RLIGHT==OLIGHT; EscCheck dropped |
| 622-633   | PATTK weapon-sound drain         | Mix_Playing         | -       | `snd_wait_abortable()`                 | demo-abort early return preserved |
| 674-685   | PATTK klink drain                | Mix_Playing         | -       | `snd_wait_abortable()`                 | demo-abort early return preserved |
| 716-727   | PATTK bang drain                 | Mix_Playing         | -       | `snd_wait_abortable()`                 | demo-abort early return preserved |
| 743-747   | PATTK wizard-image pause         | 500 (wizDelay)      | 30      | `core_wait_jiffies(WAIT_WIZARD)`       | port loop was RAW (no CLOCK) |
| 921-930   | PCLIMB up level-change screen    | 2500 (prepPause)    | 150     | `core_wait_jiffies(WAIT_PREP_PAUSE)`   | plain CLOCK loop, no EscCheck |
| 951-960   | PCLIMB down level-change screen  | 2500 (prepPause)    | 150     | `core_wait_jiffies(WAIT_PREP_PAUSE)`   | " |
| 1132-1139 | PINCAN left-ring sound drain     | Mix_Playing         | -       | `snd_wait_done()`                      | no abort check in port |
| 1153-1157 | PINCAN left winner pause         | 500 (wizDelay)      | 30      | `core_wait_jiffies(WAIT_WIZARD)`       | port loop was RAW (no CLOCK) |
| 1184-1191 | PINCAN right-ring sound drain    | Mix_Playing         | -       | `snd_wait_done()`                      | no abort check in port |
| 1205-1209 | PINCAN right winner pause        | 500 (wizDelay)      | 30      | `core_wait_jiffies(WAIT_WIZARD)`       | port loop was RAW (no CLOCK) |
| 1250-1263 | PMOVE fwd half-step anim         | 12 (moveDelay/2)    | 1       | `core_wait_jiffies_abortable(WAIT_HALF_MOVE)` | abortable; early return skips PSTEP+PDAM+HUPDAT+draw |
| 1270-1283 | PMOVE fwd post-step settle       | 12 (moveDelay/2)    | 1       | `core_wait_jiffies_abortable(WAIT_HALF_MOVE)` | abortable (NOT WAIT_MOVE - port uses moveDelay/2 here too) |
| 1291-1305 | PMOVE back half-step anim        | 12 (moveDelay/2)    | 1       | `core_wait_jiffies_abortable(WAIT_HALF_MOVE)` | abortable; early return skips PSTEP+PDAM+HUPDAT+draw |
| 1312-1325 | PMOVE back post-step settle      | 12 (moveDelay/2)    | 1       | `core_wait_jiffies_abortable(WAIT_HALF_MOVE)` | abortable (NOT WAIT_MOVE) |
| 1642-1668 | ShowTurn per-sweep-line wait     | 20 (turnDelay)      | 1       | `core_wait_jiffies_abortable(WAIT_TURN)` | 8 lines/turn (x2 for AROUND); abort restores turning=0 and returns; port redrew the identical frame per CLOCK tick - core draws once per line before the wait |
| 1722-1729 | PUSE torch sound drain           | Mix_Playing         | -       | `snd_wait_done()`                      | |
| 1743-1750 | PUSE thews-flask sound drain     | Mix_Playing         | -       | `snd_wait_done()`                      | |
| 1764-1771 | PUSE hale-flask sound drain      | Mix_Playing         | -       | `snd_wait_done()`                      | |
| 1787-1794 | PUSE abye-flask sound drain      | Mix_Playing         | -       | `snd_wait_done()`                      | |
| 1810-1817 | PUSE seer-scroll sound drain     | Mix_Playing         | -       | `snd_wait_done()`                      | |
| 1835-1842 | PUSE vision-scroll sound drain   | Mix_Playing         | -       | `snd_wait_done()`                      | |
| 1934-1942 | PSTEP thud (blocked move)        | Mix_Playing         | -       | `snd_wait_done()`                      | no abort check in port |

Sites listed in earlier drafts of this table that DO NOT EXIST in this
port lineage: "PCLIMB ladder-step ticks2 waits" (this source has only
the two prepPause screens), "PZSAVE/PZLOAD feedback wait" (PZTAPE
handlers contain no wait at all), and a separate "PLAYER cmd dispatch
CLOCK-if-due" site (that pumping is the ordinary task cadence, folded
into core_pump).

## Non-wait timing conversions (player module)

| Port                                   | core |
|----------------------------------------|------|
| HSLOW reschedule `curTime + HEARTR*17` ms | `sched.curTime + HEARTR` jiffies (17 ms = 1 jiffy) |
| PLAYER/BURNER reschedule `+ frequency` ms | `+ frequency` jiffies (values already converted in sched.h) |

## PZSAVE/PZLOAD timing note

The port serialized at the END of the scheduler pass (Scheduler::SAVE
when ZFLAG set) and applied a load AFTER pump exit (Scheduler::LOAD,
then game.LoadGame).  Core builds/applies the blob inside the handlers
(player_cmds.c) and keeps the ZFLAG signalling, so tasks later in the
same pump pass run against the just-loaded state, and a save snapshots
state a fraction of a pass earlier than the port did.  No jiffy waits
are involved either way.

dodgame.cpp intro loops and viewer fade loops got the same treatment in
game.c (WAIT_PREPARE 150 / WAIT_SEER_MAP 180 / WAIT_GAME 90) and
viewer_fade.c (FADE_BUZZ_STEP 18 / FADE_MID_PAUSE 150 in viewer.h).
