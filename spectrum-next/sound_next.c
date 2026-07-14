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
 * prescalar = 875000 / 11025 = 79.  Volume is not scalable on the
 * Covox: SFX play at fixed level for now (documented limitation).
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

static uint8_t snd_loaded;        /* blob present in RAM */
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

void plat_sound_play(uint8_t sound_id, uint8_t volume)
{
    uint32_t off;

    (void)volume;                 /* Covox has no level control */
    if (!snd_loaded || sound_id >= SND_COUNT) {
        return;
    }
    snd_active = 0;               /* fence the ISR before re-arming */
    dma_stop();

    off = SFX_INDEX[sound_id].off;
    cur_page = (uint8_t)(SFX_PAGE0 + (off >> 13));
    cur_inpage = (uint16_t)(off & 0x1FFFu);
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
