# Wait-site audit: port busy-waits -> core_wait_jiffies

The C++ port embeds `ticks1 = SDL_GetTicks(); do { CLOCK-if-due; ... }
while (curTime < ticks1 + DELAY_MS)` loops inside command handlers.
Each becomes `core_wait_jiffies(J)` (or the `_abortable` variant when the
loop body checks `game.AUTFLG && !game.demoRestart` and returns early).

Fidelity note: these waits tick ONLY the CLOCK task (heartbeat/keyboard),
never the full scheduler - creatures do not move during animations.
`core_wait_jiffies` reproduces exactly that (see core/sched.c).
`scheduler.EscCheck()` calls inside the loops are shell UI (menus) and are
dropped from core; the desktop shim handles ESC itself.

## Delay constants

| Port constant | ms  | jiffies | core name        | original source |
|---------------|-----|---------|------------------|-----------------|
| faint fade    | 750 | 45      | WAIT_FAINT_STEP  | HUPDAT faint walk |
| wizDelay      | 500 | 30      | WAIT_WIZARD      | wizard reveal   |
| moveDelay     | 25  | 2       | WAIT_MOVE        | PSTEP full step |
| moveDelay/2   | 12  | 1       | WAIT_HALF_MOVE   | PSTEP half step |
| turnDelay     | 20  | 1       | WAIT_TURN        | PTURN steps     |

(ms -> jiffies at 60 Hz, rounded to nearest with minimum 1; the sub-jiffy
17/20/25 ms values were already jiffy-order in the original.)

## Sites (player.cpp @ gondur/BlatantlyX lineage)

| Port line | context | wait | notes |
|-----------|---------|------|-------|
| 362-373   | HSLOW faint dim loop | WAIT_FAINT_STEP | per RLIGHT step until 248 |
| 391-402   | HSLOW faint recover  | WAIT_FAINT_STEP | until RLIGHT==OLIGHT |
| 632,684,726 | PLAYER cmd dispatch CLOCK-if-due | (inline CLOCK, no wait) | folded into core_pump cadence |
| 743-748   | sound-drain wait | snd_wait_done() | while Mix_Playing |
| 921-931   | PATTK swing anim | WAIT_* | delay constant at site |
| 951-961   | PATTK recover    | WAIT_* | " |
| 1138,1190 | PCLIMB anims | WAIT_* | " |
| 1153-1158, 1205-1210 | PCLIMB ladder steps | WAIT_* | ticks2 variant |
| 1250-1262 | PMOVE half-step fwd | WAIT_HALF_MOVE | abortable (demo) |
| 1270-1282 | PMOVE full-step | WAIT_MOVE | abortable |
| 1291-1304 | PMOVE half-step back | WAIT_HALF_MOVE | abortable |
| 1312-1324 | PMOVE back full | WAIT_MOVE | abortable |
| 1642-1646 | PTURN steps | WAIT_TURN | abortable |
| 1728-1841 (6x) | PUSE/PINCAN/wizard sequences | WAIT_WIZARD etc. | per-site constants |
| 1941      | PZSAVE/LOAD feedback | site constant | |

dodgame.cpp intro loops and viewer fade loops get the same treatment in
game.c / viewer_fade.c.

Exact per-site constants are read from the port source during player.c
extraction; any site whose constant differs from this table gets the
port's value and a row added here.
