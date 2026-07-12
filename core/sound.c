/* sound.c - core sound facade: one channel with preemption over
 * plat_sound_*, plus the port's distance attenuation for creatures. */
#include "sound.h"
#include "sched.h"
#include "platform.h"

/* Port: Mix_Volume(ch, (MIX_MAX_VOLUME/8) * (9 - range)) on a 0..128
 * scale; ours is 0..255.  range is clamped to 1..8 like the caller's
 * distance measure. */
#define SND_RANGE_MAX 8u

void snd_play(uint8_t sound_id, uint8_t volume)
{
    plat_sound_play(sound_id, volume);
}

void snd_creature(uint8_t sound_id, uint8_t range)
{
    uint8_t vol;
    if (range < 1u) {
        range = 1u;
    }
    if (range > SND_RANGE_MAX) {
        range = SND_RANGE_MAX;
    }
    vol = (uint8_t)((255u * (9u - range)) / 8u);
    plat_sound_play(sound_id, vol);
}

void snd_stop(void)
{
    plat_sound_stop();
}

uint8_t snd_playing(void)
{
    return plat_sound_playing();
}

void snd_wait_done(void)
{
    while (plat_sound_playing()) {
        sched.curTime = plat_jiffies();
        if ((jiffy_t)(sched.curTime - sched.TCBLND[0].next_time) < 0x8000u) {
            sched_CLOCK();
        }
        plat_yield();
    }
}
