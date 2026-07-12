/* viewer_fade.c - the wizard fade sequences (port Viewer::ShowFade /
 * draw_fade / death_fade) and the attract/status messages.
 *
 * The port's blocking ShowFade and its while(true){keyCheck; draw_fade}
 * shells are restructured into a step machine: viewer_fade_start() sets
 * up the sequence, each viewer_draw_fade() call performs one buzz step
 * (or one phase transition) and returns the done-flag, and the CALLER
 * owns the loop and the key checks (viewer_ShowFade below is the
 * blocking wrapper with the port's key-abort rules).
 *
 * ms -> jiffy conversions (60 Hz):
 *   | port field | ms   | jiffies | constant (viewer.h) |
 *   |------------|------|---------|---------------------|
 *   | buzzStep   |  300 |      18 | FADE_BUZZ_STEP      |
 *   | midPause   | 2500 |     150 | FADE_MID_PAUSE      |
 *   (prepPause 2500 ms -> 150 lives in game.c as WAIT_PREPARE)
 *
 * Sound: Mix_PlayChannel(fadChannel, buzz, -1) + Mix_Volume become
 * snd_play(SND_BUZZ, vol) re-issued each step (single-channel facade;
 * the buzz is a continuous tone, so the retrigger is inaudible);
 * kaboom + Mix_Playing drain become snd_play(SND_KABOOM)/snd_wait_done.
 * The port's volume ((32-VCTFAD)/2)*(volume/16) with MIX max 128 maps
 * to step*16 on the 0..255 scale.
 *
 * Key-abort granularity: the port polls keys inside its buzzStep/kaboom
 * waits; here keys are gathered by CLOCK during core_wait_jiffies /
 * snd_wait_done and the caller sees them after the step, so an abort can
 * lag by up to one step (18 jiffies).  Stale keys buffered before the
 * fade do not abort it (KBDTAL snapshot = the port's event-queue drain).
 */
#include "viewer.h"
#include "platform.h"
#include "parser.h"
#include "sched.h"
#include "sound.h"
#include "tables_text.h"
#include "tables_vec.h"

/* ---- attract/status messages (port display* members) ------------------ */

/* copyright banner into the status block (drawn inverse-video) */
void viewer_displayCopyright(void)
{
    viewer.TXB_U = &viewer.TXTSTS;
    --viewer.TXBFLG;
    viewer_OUTSTI(TXT_COPYRIGHT);
    viewer.TXBFLG = 0;
}

void viewer_displayWelcomeMessage(void)
{
    viewer_OUTSTI(TXT_WELCOME1);
    viewer_OUTSTI(TXT_WELCOME2);
}

void viewer_displayDeath(void)
{
    viewer_OUTSTI(TXT_DEATH);
}

void viewer_displayWinner(void)
{
    viewer_OUTSTI(TXT_WINNER1);
    viewer_OUTSTI(TXT_WINNER2);
}

void viewer_displayEnough(void)
{
    viewer_OUTSTI(TXT_ENOUGH1);
    viewer_OUTSTI(TXT_ENOUGH2);
}

/* "PREPARE!" centered on the (title-mode) examine grid */
void viewer_displayPrepare(void)
{
    viewer_clearArea(&viewer.TXTEXA);
    viewer.TXB_U = &viewer.TXTEXA;
    viewer.TXB_U->caret = (32 * 9) + 12;
    --viewer.TXBFLG;
    viewer_OUTSTI(TXT_PREPARE);
    viewer.TXBFLG = 0;
}

/* ---- wizard fade state machine ---------------------------------------- */

static const uint8_t *fadeWiz = W1_VLA;
static dodBYTE fadeMode;
static int8_t fadeVal;        /* -2 fading in, 0 message hold, +2 out */
static uint8_t fadeDone;
static dodBYTE fadeKbdTal;    /* KBDTAL snapshot: fresh-key detection */

/* port: Mix_Volume(ch, ((32 - VCTFAD) / 2) * (volumeLevel / 16)) */
static uint8_t buzz_vol(void)
{
    dodBYTE step = (dodBYTE)((dodBYTE)(32u - viewer.VCTFAD) / 2u);
    return (step >= 16u) ? (uint8_t)SND_VOL_MAX : (uint8_t)(step * 16u);
}

/* one composed frame: status line, wizard at the current VCTFAD, and
 * optionally the text-area message (port's clear/drawArea/drawVectorList
 * /swap block) */
static void fade_frame(uint8_t withMessage)
{
    viewer_clear_screen();
    viewer_drawArea(&viewer.TXTSTS);
    viewer_drawVectorList(fadeWiz);
    if (withMessage) {
        viewer_drawArea(&viewer.TXTPRI);
    }
    plat_present();
}

/* has a key arrived since viewer_fade_start? (non-consuming; polls the
 * platform directly like the port's keyCheck - the CLOCK task won't
 * gather keys while the player is fainted) */
static uint8_t fade_key_fresh(void)
{
    sched_kbd_poll();
    return (uint8_t)(parser.KBDTAL != fadeKbdTal);
}

void viewer_fade_start(dodBYTE mode)
{
    fadeMode = mode;
    fadeWiz = (mode == FADE_VICTORY) ? W2_VLA : W1_VLA;

    viewer.VXSCAL = 0x80;
    viewer.VYSCAL = 0x80;
    viewer_clearArea(&viewer.TXTPRI);

    switch (mode) {
    case FADE_BEGIN:
        viewer_displayCopyright();
        viewer_displayWelcomeMessage();
        break;
    case FADE_MIDDLE:
        viewer_clearArea(&viewer.TXTSTS);
        viewer_displayEnough();
        break;
    case FADE_DEATH:
        viewer_clearArea(&viewer.TXTSTS);
        viewer_displayDeath();
        break;
    case FADE_VICTORY:
        viewer_clearArea(&viewer.TXTSTS);
        viewer_displayWinner();
        break;
    default:
        break;
    }

    fadeKbdTal = parser.KBDTAL;   /* port: drain the SDL event buffer */

    viewer.RANGE = 1;
    viewer_SETSCL();

    viewer.VCTFAD = 32;
    fadeVal = -2;
    fadeDone = 0;

    /* start buzz at zero volume */
    snd_play(SND_BUZZ, 0);
}

/* One step of the fade.  Returns the done-flag; for FADE_DEATH and
 * FADE_VICTORY "done" means the final frame is showing and the caller
 * should keep stepping until a key arrives. */
uint8_t viewer_draw_fade(void)
{
    if (fadeDone) {
        if (fadeMode >= FADE_DEATH) {
            fade_frame(1);        /* hold wizard + message on screen */
            /* keys arrive only via CLOCK's kbd_poll: tick one jiffy or
             * the hold loop can never see the key it is waiting for */
            core_wait_jiffies(1);
        }
        return 1;
    }

    if (fadeVal == -2) {
        if ((viewer.VCTFAD & 128u) == 0) {
            /* fade in: brighter buzz, denser wizard, one buzz step */
            snd_play(SND_BUZZ, buzz_vol());
            fade_frame(0);
            core_wait_jiffies(FADE_BUZZ_STEP);
            viewer.VCTFAD = (dodBYTE)(viewer.VCTFAD - 2u);
            return 0;
        }
        /* fade-in complete: crash, then show the message */
        viewer.VCTFAD = 0;
        snd_stop();
        snd_play(SND_KABOOM, SND_VOL_MAX);
        snd_wait_done();
        fade_frame(1);
        if (fadeMode >= FADE_DEATH) {
            snd_stop();
            fadeDone = 1;         /* death/victory end here */
            return 1;
        }
        fadeVal = 0;
        return 0;
    }

    if (fadeVal == 0) {
        /* hold wizard + message, then crash and buzz up for the exit */
        core_wait_jiffies(FADE_MID_PAUSE);
        fade_frame(0);            /* erase the message */
        snd_play(SND_KABOOM, SND_VOL_MAX);
        snd_wait_done();
        snd_play(SND_BUZZ, 0);    /* start buzz again */
        fadeVal = 2;
        return 0;
    }

    /* fadeVal == 2: fade back out */
    snd_play(SND_BUZZ, buzz_vol());
    fade_frame(0);
    core_wait_jiffies(FADE_BUZZ_STEP);
    viewer.VCTFAD = (dodBYTE)(viewer.VCTFAD + 2u);
    if (viewer.VCTFAD <= 32u) {
        return 0;
    }
    snd_stop();
    viewer_clearArea(&viewer.TXTPRI);
    fadeDone = 1;
    return 1;
}

/* key abort: silence the buzz and clear the message, as every port
 * abort path does before returning false */
void viewer_fade_abort(void)
{
    snd_stop();
    viewer_clearArea(&viewer.TXTPRI);
    fadeDone = 1;
}

/* Blocking wrapper with the port's key rules:
 *   FADE_BEGIN   - any fresh key aborts (returns 0 -> start real game)
 *   FADE_MIDDLE  - uninterruptible; returns 1 on completion
 *   FADE_DEATH / FADE_VICTORY - run to the final frame, then hold it
 *                  until a key arrives; always returns 0 */
uint8_t viewer_ShowFade(dodBYTE mode)
{
    viewer_fade_start(mode);
    for (;;) {
        uint8_t done = viewer_draw_fade();
        if (mode == FADE_BEGIN && fade_key_fresh()) {
            viewer_fade_abort();
            return 0;
        }
        if (done) {
            if (mode < FADE_DEATH) {
                return 1;
            }
            if (fade_key_fresh()) {
                viewer_fade_abort();
                return 0;
            }
        }
    }
}
