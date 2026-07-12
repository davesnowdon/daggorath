/* sched.c - COMMON.ASM scheduler on 60 Hz jiffies.
 *
 * The port's Scheduler::SCHED() do-while body becomes core_pump(); its
 * inline blocking waits (which tick only CLOCK) become core_wait_jiffies().
 * Tasks self-reschedule by writing their own TCBLND[i].next_time, exactly
 * like the port.
 */
#include "sched.h"
#include "parser.h"
#include "player.h"
#include "creature.h"
#include "viewer.h"
#include "game.h"
#include "sound.h"
#include "platform.h"

sched_state sched;

static uint8_t pump_depth;
static uint8_t pump_depth_max;

void sched_Reset(void)
{
    uint8_t i;
    for (i = 0; i < TCB_COUNT; ++i) {
        sched.TCBLND[i].type = DOD_NONE;
        sched.TCBLND[i].data = DOD_NONE;
        sched.TCBLND[i].frequency = 0;
        sched.TCBLND[i].next_time = 0;
        sched.TCBLND[i].prev_time = 0;
    }
    sched.TCBPTR = 0;
    sched.curTime = 0;
    sched.elapsedTime = 0;
    sched.ZFLAG = 0;
    sched.pauseFlag = 0;
    pump_depth = 0;
    pump_depth_max = 0;
}

int8_t sched_GETTCB(void)
{
    ++sched.TCBPTR;
    return (int8_t)(sched.TCBPTR - 1);
}

void sched_SYSTCB(void)
{
    sched_Reset();

    sched.TCBLND[0].type = TID_CLOCK;
    sched.TCBLND[0].frequency = FRQ_CLOCK;
    (void)sched_GETTCB();

    sched.TCBLND[1].type = TID_PLAYER;
    sched.TCBLND[1].frequency = FRQ_PLAYER;
    (void)sched_GETTCB();

    sched.TCBLND[2].type = TID_REFRESH_DISP;
    sched.TCBLND[2].frequency = FRQ_REFRESH;
    (void)sched_GETTCB();

    sched.TCBLND[3].type = TID_HRTSLOW;   /* HSLOW self-schedules */
    (void)sched_GETTCB();

    sched.TCBLND[4].type = TID_TORCHBURN;
    sched.TCBLND[4].frequency = FRQ_TORCH;
    (void)sched_GETTCB();

    sched.TCBLND[5].type = TID_CRTREGEN;
    sched.TCBLND[5].frequency = FRQ_CRTREGEN;
    (void)sched_GETTCB();
}

/* ASCII from plat_poll_key -> DoD display code (port keyHandler subset:
 * the game only consumes letters, space, return, backspace). */
static int16_t ascii_to_dod(int16_t k)
{
    if (k >= 'A' && k <= 'Z') {
        return (int16_t)(k - 'A' + 1);
    }
    if (k >= 'a' && k <= 'z') {
        return (int16_t)(k - 'a' + 1);
    }
    if (k == ' ') {
        return I_SP;
    }
    if (k == '\r' || k == '\n') {
        return I_CR;
    }
    if (k == '\b' || k == 0x7F) {
        return I_BS;
    }
    return -1;
}

/* pump platform keys into the parser's keyboard ring */
static void kbd_poll(void)
{
    int16_t k;
    while ((k = plat_poll_key()) >= 0) {
        int16_t c = ascii_to_dod(k);
        if (c >= 0) {
            parser_KBDPUT((dodBYTE)c);
        }
    }
}

/* any key pending?  (port Scheduler::keyCheck) */
static uint8_t key_pending(void)
{
    kbd_poll();
    return (uint8_t)(player.KBDHDR != player.KBDTAL);
}

/* Scheduler::CLOCK - heartbeat + input, runs once per due jiffy. */
void sched_CLOCK(void)
{
    sched.TCBLND[0].next_time = (jiffy_t)(sched.curTime
                                          + sched.TCBLND[0].frequency);
    sched.elapsedTime = (jiffy_t)(sched.curTime - sched.TCBLND[0].prev_time);
    if (sched.elapsedTime > CLOCK_ELAPSED_CLAMP) {
        sched.elapsedTime = CLOCK_ELAPSED_CLAMP;
    }

    if (sched.elapsedTime >= 1) {
        sched.TCBLND[0].prev_time = sched.curTime;
        if (player.HBEATF != 0) {
            player.HEARTC = (dodBYTE)(player.HEARTC - sched.elapsedTime);
            if ((player.HEARTC & 0x80u) != 0) {
                player.HEARTC = 0;
            }
            if (player.HEARTC == 0) {
                player.HEARTC = player.HEARTR;
                /* hrtSound[HEARTS+1]: HEARTS 0xFF -> lub, 0 -> dub.
                 * (The port blocks until the 3 ms sample ends; that is
                 * sub-jiffy, so a fire-and-forget play is state-neutral.) */
                snd_play((uint8_t)((dodBYTE)(player.HEARTS + 1) == 0
                                   ? SND_HEART1 : SND_HEART2), SND_VOL_MAX);
                if (player.HEARTF != 0) {
                    if ((player.HEARTS & 0x80u) != 0) {
                        /* small heart glyphs (SPCTAB $20,$21) */
                        viewer.statArea[15] = CHR_HEART_SM_L;
                        viewer.statArea[16] = CHR_HEART_SM_R;
                        player.HEARTS = 0;
                    } else {
                        /* large heart glyphs (SPCTAB $22,$23) */
                        viewer.statArea[15] = CHR_HEART_LG_L;
                        viewer.statArea[16] = CHR_HEART_LG_R;
                        player.HEARTS = 0xFF;
                    }
                    if (!player.turning) {
                        --viewer.UPDATE;
                        viewer_draw_game();
                    }
                }
            }
        }
    }

    if (player.FAINT == 0) {
        if (game.AUTFLG) {
            if (key_pending()) {       /* abort demo on keypress */
                game.hasWon = 1;
                game.demoRestart = 0;
            }
        } else {
            kbd_poll();
        }
    }
}

static void run_task(uint8_t idx, int8_t *result)
{
    switch (sched.TCBLND[idx].type) {
    case TID_CLOCK:
        sched_CLOCK();
        break;
    case TID_PLAYER:
        *result = player_PLAYER();
        break;
    case TID_REFRESH_DISP:
        *result = viewer_LUKNEW();
        break;
    case TID_HRTSLOW:
        *result = player_HSLOW();
        break;
    case TID_TORCHBURN:
        *result = player_BURNER();
        break;
    case TID_CRTREGEN:
        *result = creature_CREGEN();
        break;
    case TID_CRTMOVE:
        *result = creature_CMOVE(idx, sched.TCBLND[idx].data);
        break;
    default:
        break;
    }
}

/* One pass of the port's SCHED() do-while: visit every allocated slot
 * once, run the due ones, then evaluate the exit conditions. */
uint8_t core_pump(void)
{
    int8_t result = 0;
    uint8_t ctr;

    ++pump_depth;
    if (pump_depth > pump_depth_max) {
        pump_depth_max = pump_depth;
    }

    for (ctr = 0; ctr <= (uint8_t)sched.TCBPTR && ctr < TCB_COUNT; ++ctr) {
        sched.curTime = plat_jiffies();
        if ((jiffy_t)(sched.curTime - sched.TCBLND[ctr].next_time) < 0x8000u) {
            run_task(ctr, &result);
        }
    }
    --pump_depth;

    if (sched.ZFLAG != 0) {
        if (sched.ZFLAG == 0xFF) {
            return PUMP_LOAD_ABANDON;
        }
        /* save request handled by the shell via plat_save_state */
        sched.ZFLAG = 0;
    }
    if (PPOW < PDAM) {
        return PUMP_DEATH;
    }
    if (game.hasWon) {
        return PUMP_WIN;
    }
    return (result != 0) ? PUMP_TASK_EXIT : PUMP_RUN;
}

/* The blocking-animation wait: tick ONLY the CLOCK task while the jiffies
 * elapse, exactly like the port's inline waits that call CLOCK(). */
void core_wait_jiffies(jiffy_t n)
{
    jiffy_t start = plat_jiffies();

    ++pump_depth;
    if (pump_depth > pump_depth_max) {
        pump_depth_max = pump_depth;
    }
    do {
        sched.curTime = plat_jiffies();
        if ((jiffy_t)(sched.curTime - sched.TCBLND[0].next_time) < 0x8000u) {
            sched_CLOCK();
        }
        plat_yield();
    } while ((jiffy_t)(plat_jiffies() - start) < n);
    --pump_depth;
}

uint8_t core_wait_jiffies_abortable(jiffy_t n)
{
    jiffy_t start = plat_jiffies();

    ++pump_depth;
    if (pump_depth > pump_depth_max) {
        pump_depth_max = pump_depth;
    }
    do {
        sched.curTime = plat_jiffies();
        if ((jiffy_t)(sched.curTime - sched.TCBLND[0].next_time) < 0x8000u) {
            sched_CLOCK();
            if (game.AUTFLG && !game.demoRestart) {
                --pump_depth;
                return 1;
            }
        }
        plat_yield();
    } while ((jiffy_t)(plat_jiffies() - start) < n);
    --pump_depth;
    return 0;
}

uint8_t core_pump_depth(void)
{
    return pump_depth_max;
}
