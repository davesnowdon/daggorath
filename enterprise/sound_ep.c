/* sound_ep.c - Dave 6-bit DAC PCM playback for the Enterprise 64/128.
 *
 * Model (all hardware facts probe-verified, PHASE4-DAVE-PROBE.md):
 * A7h = 0x58 once at init (tone-0 as interrupt source + both channels in
 * DAC mode; oscillators keep running and keep interrupting).  A sample
 * play retunes tone 0 to the sample's rate (period 33 = 7352.94 Hz for
 * the 7350 Hz masters, 67 = 3676.47 Hz for half-rate data - no
 * double-servicing, long sounds cost half the interrupts) and arms the
 * sound interrupt; isr_ep.asm then feeds one byte per interrupt through
 * the current volume LUT to the L+R DACs.  One channel, play preempts,
 * like the CoCo DAC.
 *
 * The loader (loader.s) reads DAGGOR1.SFX (stock EP128: 3 allocated
 * segments + F8/FE tails) or DAGGOR2.SFX (expanded RAM: 8 allocated
 * segments, everything full-rate) and leaves a handoff block at 0x00F0
 * in the game's page-0 segment: profile byte, segment count, then the
 * allocated segment numbers.  sfx_index_ep.h (generated) maps each sound
 * to (bin, window address, 128-byte blocks, period); ep_snd_init resolves
 * bins to real segment numbers once.
 *
 * Volume: 9 tiers (like the Next port) through ONE 256->6-bit LUT in the
 * fixed, 256-aligned FE-segment slot 0x9700-0x97FF (the Makefile asserts
 * BSS ends below; the FE blob tail at 0x9800 and the stack sit above).
 * The LUT is rebuilt only when the tier changes (~32 ms, cached - the
 * Next port re-scales whole samples per (id,tier) change, far more).
 * Tier t maps in -> ((128 + (in-128)*t*32/256) >> 2), so tier 8
 * (volume >= 224) is the identity in>>2 and the 8 discrete creature
 * volumes 255,223,...,31 land one tier each. */
#include <stdint.h>

#include "platform.h"
#include "sound_ids.h"
#include "sfx_index_ep.h"

/* isr_ep.asm */
extern volatile uint8_t snd_active;
extern uint8_t snd_lut[];              /* ld a,(NNNN): [2] = LUT page byte */
extern volatile uint8_t snd_seg;
extern volatile uint16_t snd_waddr;
extern volatile uint8_t snd_blocks;
extern volatile uint8_t snd_period;
extern void ep_snd_start(void);
extern void ep_snd_stop(void);

/* loader handoff block (page-0 segment, below the game image at 0x0100) */
#define SND_CONV   ((const volatile uint8_t *)0x00F0)
#define LUT_BASE   0x9700u             /* one 256-byte LUT, 256-aligned */

static uint8_t snd_profile;            /* 0 = silent, 1 = stock, 2 = full */
static uint8_t lut_tier = 0xFF;        /* tier the LUT is currently built for */

/* per-sound placement resolved to real segment numbers */
static struct {
    uint8_t seg;
    uint16_t waddr;
    uint8_t blocks;
    uint8_t period;
} snd_dir[SND_COUNT];

static void build_lut(uint8_t tier)
{
    uint8_t *lut = (uint8_t *)LUT_BASE;
    int16_t m = (int16_t)((uint16_t)tier << 5);
    uint16_t i;

    if (lut_tier == tier) {
        return;
    }
    for (i = 0; i != 256u; ++i) {
        int16_t v = (int16_t)(128 + ((((int16_t)i - 128) * m) >> 8));
        lut[i] = (uint8_t)((uint8_t)v >> 2);
    }
    lut_tier = tier;
}

/* A7h once: tone 0 selected as the sound-interrupt source (b6:5 = 10),
 * both DACs on (b4:3), all tone channels running (b2:0 = 0).  The DACs
 * output the last A8h/ACh value from here on - park them at midline. */
static void snd_dave_init(void) __naked
{
    __asm
        ld   a, #0x58
        out  (0xa7), a
        ld   a, #0x20
        out  (0xa8), a
        out  (0xac), a
        ret
    __endasm;
}

void ep_snd_init(void)
{
    const sfx_ep_row_t *rows;
    uint8_t nsegs, i;

    snd_profile = SND_CONV[0];
    if (snd_profile == 1u) {
        rows = SFX1;
        nsegs = SFX1_NSEGS;
    } else if (snd_profile == 2u) {
        rows = SFX2;
        nsegs = SFX2_NSEGS;
    } else {
        snd_profile = 0;
        return;                        /* no blob: stay silent (Phase-3) */
    }
    if (SND_CONV[1] < nsegs) {
        snd_profile = 0;
        return;
    }
    for (i = 0; i != SND_COUNT; ++i) {
        uint8_t bin = rows[i].bin;
        /* bins 0..N-1: loader-allocated segments; 0xF8 = the loader's own
         * segment (handoff byte 10 - only F8 on a stock 128 by accident of
         * boot order); 0xFE = the game's page-2 segment, fixed. */
        snd_dir[i].seg = (bin < nsegs) ? SND_CONV[2u + bin]
                       : (bin == 0xF8u) ? SND_CONV[10]
                       : bin;
        snd_dir[i].waddr = rows[i].waddr;
        snd_dir[i].blocks = rows[i].blocks;
        snd_dir[i].period = rows[i].period;
    }
    snd_lut[2] = (uint8_t)(LUT_BASE >> 8);   /* ISR's LUT page, set once */
    build_lut(8);                       /* full volume: the common case */
    snd_dave_init();
}

void plat_sound_play(uint8_t sound_id, uint8_t volume)
{
    uint8_t tier;

    if (snd_profile == 0u || sound_id >= SND_COUNT) {
        return;
    }
    tier = (uint8_t)(((uint16_t)volume + 1u) >> 5);
    if (tier > 8u) {
        tier = 8u;
    }
    build_lut(tier);                   /* no-op when the tier is unchanged */
    snd_seg = snd_dir[sound_id].seg;
    snd_waddr = snd_dir[sound_id].waddr;
    snd_blocks = snd_dir[sound_id].blocks;
    snd_period = snd_dir[sound_id].period;
    ep_snd_start();                       /* retune + load shadow state + arm */
}

void plat_sound_stop(void)
{
    if (snd_profile != 0u) {
        ep_snd_stop();
    }
}

uint8_t plat_sound_playing(void)
{
    return snd_active;
}
