/* sched.h - COMMON.ASM: the task scheduler, restored to 60 Hz jiffy
 * deadlines (the C++ port ran it on SDL millisecond wall-clock).
 *
 * Two execution tiers, matching the port exactly:
 *   core_pump()          one full round-robin pass over all due tasks -
 *                        the body of the port's SCHED() main loop.
 *   core_wait_jiffies(n) the blocking-animation wait: ticks ONLY the
 *                        CLOCK task (heartbeat + keyboard) while waiting,
 *                        exactly like the port's inline SDL_GetTicks
 *                        loops that call CLOCK() - creatures do NOT move
 *                        during these waits.
 */
#ifndef DOD_SCHED_H
#define DOD_SCHED_H

#include "dod_types.h"

#define TCB_COUNT 38

/* task types (port sched.h enum, same order) */
enum {
    TID_CLOCK = 0,
    TID_PLAYER,
    TID_REFRESH_DISP,
    TID_HRTSLOW,
    TID_TORCHBURN,
    TID_CRTREGEN,
    TID_CRTMOVE,
    TID_DEATH
};

/* task periods in jiffies; the port's ms values were jiffy*16.67
 * (17ms=1, 300ms=18, 5000ms=300, 300000ms=18000) */
#define FRQ_CLOCK      1u
#define FRQ_PLAYER     1u
#define FRQ_REFRESH    18u
#define FRQ_TORCH      300u
#define FRQ_CRTREGEN   18000u
/* elapsed-time clamp in CLOCK ("reality check"), jiffies */
#define CLOCK_ELAPSED_CLAMP 126u

/* core_pump status results */
enum {
    PUMP_RUN = 0,
    PUMP_DEATH,
    PUMP_WIN,
    PUMP_LOAD_ABANDON,
    PUMP_TASK_EXIT      /* a task returned nonzero (level change etc.) */
};

typedef struct {
    Task    TCBLND[TCB_COUNT];
    int8_t  TCBPTR;
    jiffy_t curTime;      /* refreshed by pump/wait; tasks read this */
    jiffy_t elapsedTime;  /* CLOCK's clamped delta */
    dodBYTE ZFLAG;        /* 0xFF = load-abandon, else save request */
    uint8_t pauseFlag;
} sched_state;

extern sched_state sched;

void sched_Reset(void);
void sched_SYSTCB(void);            /* build the initial TCB table */
int8_t sched_GETTCB(void);          /* allocate a task slot */
void sched_CLOCK(void);             /* heartbeat + keyboard, 1/jiffy */

uint8_t core_pump(void);            /* returns PUMP_* */
void core_wait_jiffies(jiffy_t n);
/* Same, but returns 1 immediately if the demo gets aborted mid-wait
 * (port waits that check game.AUTFLG && !game.demoRestart and return). */
uint8_t core_wait_jiffies_abortable(jiffy_t n);
uint8_t core_pump_depth(void);      /* re-entrancy watermark */

/* Port wait-delay constants (ms) -> jiffies; see docs/wait-site-audit.md */
#define WAIT_FAINT_STEP 45u   /* 750 ms */
#define WAIT_WIZARD     30u   /* 500 ms (wizDelay) */
#define WAIT_TURN       1u    /* 20 ms (turnDelay) - sub-jiffy, ceil to 1 */
#define WAIT_MOVE       2u    /* 25 ms (moveDelay) - ~1.5 jiffies */
#define WAIT_HALF_MOVE  1u    /* moveDelay/2 */

#endif /* DOD_SCHED_H */
