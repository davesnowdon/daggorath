/* platform.h - the ONLY interface the portable core may use to reach the
 * outside world.  Every backend (desktop SDL2, Spectrum Next, MEGA65)
 * implements exactly these functions.  The core never includes any OS or
 * library header besides <stdint.h>.
 *
 * Coordinate space: the original 256x192 pixel screen.
 * Time: 60 Hz jiffies (the CoCo's vertical-sync interrupt rate).
 */
#ifndef DOD_PLATFORM_H
#define DOD_PLATFORM_H

#include <stdint.h>

/* Jiffy counter: uint16 wraps ~18 min; ALWAYS compare with deltas:
 *   (jiffy_t)(now - then) < LIMIT
 * The longest game interval (creature regen, 5 min = 18000) < 32768. */
typedef uint16_t jiffy_t;

#define JIFFY_HZ 60u

enum {
    PLAT_OK = 0,
    PLAT_ERR_UNSUPPORTED = 1,
    PLAT_ERR_IO = 2
};

/* --- lifecycle --------------------------------------------------------- */
void    plat_init(void);
void    plat_shutdown(void);

/* --- video: draw into the BACK buffer, then present -------------------- */
void    plat_clear(void);
/* fade: index into the VECTOR.ASM dot-mask table; 0 = solid line.
 * Callers never pass the 0xFF "invisible" value - they skip the draw. */
void    plat_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                       uint8_t fade);
/* Character grid is 32 cols x 24 rows of 8x8 cells; rows[7] are the
 * pre-shifted bytes from tables_font.c (bit 7 leftmost pixel). */
void    plat_blit_glyph(uint8_t col, uint8_t row, const uint8_t rows[7]);
void    plat_present(void);

/* --- sound: one channel, play preempts, matching the CoCo's single DAC - */
void    plat_sound_play(uint8_t sound_id, uint8_t volume /* 0..255 */);
void    plat_sound_stop(void);
uint8_t plat_sound_playing(void);

/* --- input: ASCII (upper-case letters, digits, '\r', '\b'), -1 = none -- */
int16_t plat_poll_key(void);

/* --- timing ------------------------------------------------------------ */
jiffy_t plat_jiffies(void);
void    plat_yield(void);   /* cheap idle: pump host events / HALT */

/* --- persistence (optional; return PLAT_ERR_UNSUPPORTED if absent) ----- */
uint8_t plat_save_state(const void *buf, uint16_t len);
uint8_t plat_load_state(void *buf, uint16_t len);

#endif /* DOD_PLATFORM_H */
