/* sound_next.c - zxnDMA sample playback to the Covox DAC.
 *
 * The 26 SFX (unsigned 8-bit, 11025 Hz, tools/gen_sfxbin.py blob) load
 * at init from "daggorath.sfx" via esxDOS into 8K MMU pages SFX_PAGE0+
 * (pages 32+ = 16K banks 16+, free for .nex apps; needs the standard
 * 1MB Next).  Playback streams chunks with the zxnDMA (port 0x6B):
 * the current page is mapped at MMU slot 1 (0x2000, otherwise unused
 * ROM) and the frame ISR re-arms the next chunk when the DMA reports
 * end-of-block - so multi-page samples play through CPU-bound game
 * code.  One channel, play preempts, like the CoCo DAC.
 *
 * DMA program per em00k's zxnext-simple-dma (known-good):
 *   $7D addrL addrH lenL lenH   WR0 A->B + source + length
 *   $54 $02                     WR1 port A memory, ++, cycle 2
 *   $68 $22 prescalar           WR2 port B IO, fixed, cycle 2 + rate
 *   $CD $DF $FF                 WR4 burst mode, dest port 0xFFDF
 *   $82                         WR5 stop on end of block
 *   $CF $B3 $87                 WR6 load, force ready, enable
 * prescalar = 875000 / 11025 = 79.
 *
 * Covox volume tiers: the DAC has no level control, so volume is done
 * by scaling the PCM bytes on the CPU.  plat_sound_play quantizes the
 * core's 0..255 volume to 9 tiers of the desktop reference's
 * volume/255 midline gain: tier 8 (volume >= 224 - every full-volume
 * play and creature range 1) streams the original sample untouched;
 * tiers 0-7 rebuild a 256-byte lookup table (gain tier*32/256) and
 * copy the sample through snd_scale_chunk (snd_scale.asm) into 3
 * bounce pages right after the blob, cached by (id, tier) so restarts
 * and repeats at the same range cost nothing.  The 8 discrete creature
 * volumes 255,223,...,31 (core/sound.c) map one tier each, and the
 * fade buzz ramp (0,16,...,240,255) crosses a tier every other step.
 */
#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <stdint.h>

#include "platform.h"
#include "sound_ids.h"
#include "sfx_index.h"

#define SFX_PAGE0    32u          /* first 8K page holding the blob */
#define SFX_FILE     "daggorath.sfx"
#define DMA_PRESCALAR 79u         /* 875000 / 11025 */
#define MMU1_WINDOW  ((uint8_t *)0x2000)
#define MMU0_WINDOW  ((uint8_t *)0x0000)

/* volume-scaled bounce copy: 3 pages right after the blob (pages
 * 57-59 with the current blob; still well inside the standard 1MB
 * Next).  24576 bytes >= the largest attenuable sample (buzz, 22050;
 * kaboom/bang/hearts only ever play at full volume). */
#define SFX_PAGES    ((SFX_BLOB_LEN + 8191u) / 8192u)
#define BOUNCE_PAGE0 ((uint8_t)(SFX_PAGE0 + SFX_PAGES))
#define BOUNCE_PAGES 3u
#define BOUNCE_LEN   (BOUNCE_PAGES * 8192u)
#define SND_LUT      ((uint8_t *)0x5B00) /* keep in sync with snd_scale.asm */

extern void snd_scale_chunk(const uint8_t *src, uint8_t *dst, uint16_t len);

static uint8_t snd_loaded;        /* blob present in RAM */
static uint8_t bounce_id = 0xFFu; /* (sound, tier) cached in the bounce */
static uint8_t bounce_tier = 0xFFu;
static uint8_t lut_tier = 0xFFu;  /* tier the LUT is currently built for */
static volatile uint8_t snd_active;
static uint8_t cur_page;          /* page of the chunk now playing */
static uint16_t cur_inpage;       /* offset of NEXT chunk in its page */
static uint32_t cur_remaining;    /* bytes left after current chunk */
static uint16_t cur_chunklen;

static void dma_out(uint8_t b)
{
    IO_6B = b;
}

static void dma_stop(void)
{
    dma_out(0x83);                /* disable */
}

/* Arm one chunk: map its page at 0x2000 and start the DMA. */
static void dma_arm(void)
{
    uint16_t addr, len;

    len = (uint16_t)(8192u - cur_inpage);
    if ((uint32_t)len > cur_remaining) {
        len = (uint16_t)cur_remaining;
    }
    cur_chunklen = len;
    cur_remaining -= len;

    ZXN_NEXTREGA(0x51, cur_page); /* MMU1 <- sample page (runtime value) */
    addr = (uint16_t)(0x2000u + cur_inpage);

    dma_out(0x83);                /* disable while programming */
    dma_out(0x7D);                /* WR0: A->B, addr+len follow */
    dma_out((uint8_t)(addr & 0xFFu));
    dma_out((uint8_t)(addr >> 8));
    dma_out((uint8_t)(len & 0xFFu));
    dma_out((uint8_t)(len >> 8));
    dma_out(0x54);                /* WR1: A memory, increment, cycle */
    dma_out(0x02);
    dma_out(0x68);                /* WR2: B IO, fixed, cycle+prescalar */
    dma_out(0x22);
    dma_out(DMA_PRESCALAR);
    dma_out(0xCD);                /* WR4: burst, dest port follows */
    dma_out(0xDF);                /* Covox 0xFFDF */
    dma_out(0xFF);
    dma_out(0x82);                /* WR5: stop on end of block */
    dma_out(0xCF);                /* load */
    dma_out(0xB3);                /* force ready */
    dma_out(0x87);                /* enable */

    /* next chunk starts at the top of the following page */
    ++cur_page;
    cur_inpage = 0;
}

/* Frame-ISR hook (isr.asm): advance to the next chunk when the DMA
 * finished the current one. */
void snd_isr_tick(void)
{
    uint8_t status;

    if (!snd_active) {
        return;
    }
    dma_out(0xBF);                /* read status */
    status = IO_6B;
    if ((status & 0x20u) != 0) {  /* E=1: block not finished yet */
        return;
    }
    if (cur_remaining != 0) {
        dma_arm();
    } else {
        dma_stop();
        snd_active = 0;
    }
}

/* Rebuild the volume LUT for a tier: out = 128 + (in-128)*m/256 with
 * m = tier*32.  The desktop reference scales by volume/255; tier t
 * covers volumes t*32-1 .. t*32+30, so this lands within half a tier
 * of every volume in the band and hits the 8 creature volumes
 * (255,223,...,31 = tier*32-1) essentially exactly.  The table lives
 * in the always-mapped free tail of the draw page (snd_scale.asm). */
static void build_lut(uint8_t tier)
{
    uint16_t i;
    int16_t m = (int16_t)((uint16_t)tier << 5);

    if (lut_tier == tier) {
        return;
    }
    for (i = 0; i != 256u; ++i) {
        SND_LUT[i] = (uint8_t)(128 + ((((int16_t)i - 128) * m) >> 8));
    }
    lut_tier = tier;
}

/* Scale a whole sample through the LUT into the bounce pages: source
 * pages stream through the MMU1 window, bounce pages through the MMU0
 * window (the same temporary slot-0 trick plat_present uses; no
 * instruction fetch happens below 0x4000, so divMMC never automaps).
 * Interrupt-safe: the frame ISR only remaps MMU1 while a sound is
 * active, and the caller has already stopped playback. */
static void scale_into_bounce(uint8_t sound_id)
{
    uint32_t off = SFX_INDEX[sound_id].off;
    uint32_t left = SFX_INDEX[sound_id].len;
    uint8_t spage = (uint8_t)(SFX_PAGE0 + (off >> 13));
    uint16_t soff = (uint16_t)(off & 0x1FFFu);
    uint8_t dpage = BOUNCE_PAGE0;
    uint16_t doff = 0;

    while (left != 0) {
        uint16_t n = (uint16_t)(8192u - soff);
        uint16_t dleft = (uint16_t)(8192u - doff);
        if (n > dleft) {
            n = dleft;
        }
        if ((uint32_t)n > left) {
            n = (uint16_t)left;
        }
        ZXN_NEXTREGA(0x51, spage);    /* MMU1 <- source page */
        ZXN_NEXTREGA(0x50, dpage);    /* MMU0 <- bounce page */
        snd_scale_chunk(MMU1_WINDOW + soff, MMU0_WINDOW + doff, n);
        left -= n;
        soff = (uint16_t)(soff + n);
        if (soff == 8192u) {
            soff = 0;
            ++spage;
        }
        doff = (uint16_t)(doff + n);
        if (doff == 8192u) {
            doff = 0;
            ++dpage;
        }
    }
    ZXN_NEXTREG(0x50, 0xFF);          /* MMU0 back to ROM */
}

void plat_sound_play(uint8_t sound_id, uint8_t volume)
{
    uint8_t tier;

    if (!snd_loaded || sound_id >= SND_COUNT) {
        return;
    }
    snd_active = 0;               /* fence the ISR before re-arming */
    dma_stop();

    /* quantize to a volume tier; see the header block */
    tier = (uint8_t)(((uint16_t)volume + 1u) >> 5);
    if (tier >= 8u || SFX_INDEX[sound_id].len > BOUNCE_LEN) {
        uint32_t off = SFX_INDEX[sound_id].off;
        cur_page = (uint8_t)(SFX_PAGE0 + (off >> 13));
        cur_inpage = (uint16_t)(off & 0x1FFFu);
    } else {
        if (bounce_id != sound_id || bounce_tier != tier) {
            build_lut(tier);
            scale_into_bounce(sound_id);
            bounce_id = sound_id;
            bounce_tier = tier;
        }
        cur_page = BOUNCE_PAGE0;
        cur_inpage = 0;
    }
    cur_remaining = SFX_INDEX[sound_id].len;
    dma_arm();
    snd_active = 1;
}

void plat_sound_stop(void)
{
    snd_active = 0;
    dma_stop();
}

uint8_t plat_sound_playing(void)
{
    return snd_active;
}

/* Load the blob into pages SFX_PAGE0.. through the MMU1 window.
 * Called from plat_init; failure just leaves the game silent. */
void snd_load_blob(void)
{
    uint8_t h, page;
    uint32_t left;

    h = esx_f_open(SFX_FILE, ESX_MODE_R | ESX_MODE_OPEN_EXIST);
    if (h == 0xFFu) {
        return;
    }
    page = SFX_PAGE0;
    left = SFX_BLOB_LEN;
    while (left != 0) {
        uint16_t n = (left > 8192u) ? 8192u : (uint16_t)left;
        ZXN_NEXTREGA(0x51, page);
        if ((uint16_t)esx_f_read(h, MMU1_WINDOW, n) != n) {
            esx_f_close(h);
            return;               /* short read: stay silent */
        }
        left -= n;
        ++page;
    }
    esx_f_close(h);
    ZXN_NEXTREG(0x51, 0xFF);      /* MMU1 back to ROM */
    snd_loaded = 1;
}
