/* sound.h - core-side sound facade over plat_sound_*.
 * One channel with preemption, like the CoCo's single DAC. */
#ifndef DOD_SOUND_H
#define DOD_SOUND_H

#include "dod_types.h"
#include "sound_ids.h"

#define SND_VOL_MAX 255u

void snd_play(uint8_t sound_id, uint8_t volume);
/* Creature noises attenuate with maze range like SOUNDS.ASM. */
void snd_creature(uint8_t sound_id, uint8_t range);
void snd_stop(void);
uint8_t snd_playing(void);
/* Block until the current sound ends, ticking only the CLOCK task -
 * the port's `while (Mix_Playing(ch)) { CLOCK-if-due }` idiom. */
void snd_wait_done(void);

#endif /* DOD_SOUND_H */
