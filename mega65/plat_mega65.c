/* plat_mega65.c - MEGA65 backend (first-light version: all C).
 *
 * Video    VIC-IV in legacy hires bitmap mode (BMM), with the fetches
 *          relocated into bank-1 chip RAM: bitmap at $18000 (CHARPTR),
 *          colour matrix at $1A000 (SCRNPTR, white-on-black, written
 *          once).  The core draws into an 8000-byte shadow framebuffer
 *          in bank 0 laid out EXACTLY like the hardware bitmap (8x8
 *          cell interleave, 320 bytes per cell row, the 256x192 window
 *          at cell column 4); plat_present() is a single DMAgic copy.
 *          The line DDA below must match core/draw_ref.c pixel for
 *          pixel - same algorithm, only the plot primitive differs.
 * Input    the hardware ASCII key queue at $D610: typing during
 *          CPU-bound stretches is buffered by hardware, so - unlike
 *          the Next - no ISR keyboard scan is needed.
 * Timing   raster IRQ hooked through the kernal's $0314 RAM vector;
 *          the handler (irq.s) counts jiffies with the same 6/5
 *          accumulator as the Next ISR (PAL: 60 jiffies / 50 frames;
 *          NTSC per $D06F.7 counts 1:1).
 * Sound    Audio DMA channel 0 (sound_mega65.c): the SFX blob streams
 *          from the SD through hyppo file reads into chip RAM at init.
 * Save     one slot, DAGGOR65.SAV in the SD FAT root, overwritten in
 *          place through hyppo's writefile trap (hyppo_write.s).
 */
#include <stdint.h>
#include <string.h>
#include <mega65.h>

#include "platform.h"

#define POKE(a, v) (*(volatile uint8_t *)(uint16_t)(a) = (uint8_t)(v))
#define PEEK(a)    (*(volatile uint8_t *)(uint16_t)(a))

/* 28-bit chip-RAM addresses of what the VIC-IV displays */
#define BITMAP_ADDR 0x18000UL     /* 8K bitmap (must be 8K-aligned)   */
#define MATRIX_ADDR 0x1A000UL     /* 40x25 colour matrix              */

/* Shadow framebuffer: the hardware bitmap's 8x8 cell interleave, but
 * only the 256-pixel window wide (32 cells; the bitmap's side margins
 * and its row 24 are blanked once at init and never touched again).
 * Pixel row y starts at rowbase[y] = (y>>3)*256 + (y&7); the byte for
 * pixel x is + (x & 0xF8) and the bit is 0x80 >> (x & 7).  A cell row
 * here (256 bytes) is contiguous in the bitmap too, so present is 24
 * row-block DMAs.  Before the first draw the buffer doubles as the
 * SFX loader's bounce buffer (bank 0 is too tight for both).
 * fb lives at an ABSOLUTE low-RAM address (layout.s): the link region
 * is nearly full, and crt0 does not zero it - plat_init must. */
extern uint8_t fb[6144];
static uint16_t rowbase[192];
uint8_t *const plat_init_bounce = fb;

/* ---- DMAgic: minimal enhanced-mode copy/fill --------------------------- */
/* One static enhanced-format list: two option bytes select the source
 * and destination megabyte, then a standard F018B 12-byte job. */
struct __attribute__((packed)) dma_list {
    uint8_t  opt_src_mb_tag;      /* 0x80 */
    uint8_t  src_mb;              /* source addr bits 20-27 */
    uint8_t  opt_dst_mb_tag;      /* 0x81 */
    uint8_t  dst_mb;              /* dest addr bits 20-27 */
    uint8_t  opt_end;             /* 0x00: end of options */
    uint8_t  cmd;                 /* 0 = copy, 3 = fill */
    uint16_t count;
    uint16_t src;                 /* fill: low byte = fill value */
    uint8_t  src_bank;            /* source addr bits 16-19 */
    uint16_t dst;
    uint8_t  dst_bank;
    uint8_t  cmd_msb;             /* F018B second command byte */
    uint16_t modulo;
};
_Static_assert(sizeof(struct dma_list) == 17, "DMA list must be packed");

/* volatile: the DMAgic reads the list from memory when the trigger
 * register is poked - without it the optimizer may defer or merge the
 * field stores across the (volatile) trigger POKEs, firing a stale
 * list.  Exactly that sprayed the ROM boot screen's $20s over bank 1
 * before this was volatile. */
static volatile struct dma_list dl;

static void dma_run(void)
{
    POKE(0xD704, 0x00);                          /* list is in MB 0    */
    POKE(0xD702, 0x00);                          /* list bank          */
    POKE(0xD701, (uint16_t)(uintptr_t)&dl >> 8);
    POKE(0xD705, (uint16_t)(uintptr_t)&dl & 0xFFu);  /* enhanced trigger:
                                                        runs to completion
                                                        (CPU is held) */
}

void plat_lcopy(uint32_t src, uint32_t dst, uint16_t n)
{
    dl.opt_src_mb_tag = 0x80; dl.src_mb = (uint8_t)(src >> 20);
    dl.opt_dst_mb_tag = 0x81; dl.dst_mb = (uint8_t)(dst >> 20);
    dl.opt_end = 0x00;
    dl.cmd = 0x00;                               /* copy */
    dl.count = n;
    dl.src = (uint16_t)src;  dl.src_bank = (uint8_t)(src >> 16) & 0x0Fu;
    dl.dst = (uint16_t)dst;  dl.dst_bank = (uint8_t)(dst >> 16) & 0x0Fu;
    dl.cmd_msb = 0x00;
    dl.modulo = 0x0000;
    dma_run();
}

static void lfill(uint32_t dst, uint8_t val, uint16_t n)
{
    dl.opt_src_mb_tag = 0x80; dl.src_mb = 0x00;
    dl.opt_dst_mb_tag = 0x81; dl.dst_mb = (uint8_t)(dst >> 20);
    dl.opt_end = 0x00;
    dl.cmd = 0x03;                               /* fill */
    dl.count = n;
    dl.src = val;            dl.src_bank = 0x00;
    dl.dst = (uint16_t)dst;  dl.dst_bank = (uint8_t)(dst >> 16) & 0x0Fu;
    dl.cmd_msb = 0x00;
    dl.modulo = 0x0000;
    dma_run();
}

/* ---- video ------------------------------------------------------------- */
void plat_clear(void)
{
    memset(fb, 0, sizeof fb);
}

/* EXACTLY the VECTOR.ASM algorithm - see core/draw_ref.c, the normative
 * reference.  24.8 fixed point, plot-candidate-then-step, endpoint never
 * plotted, dot-period fades, x-clip on the integer high byte.  Kept in
 * C: at 40 MHz the 6502 keeps up without an assembly rasterizer. */
static const uint8_t bit_mask[8] = {
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};

static int16_t incre(int16_t delta, uint16_t length)
{
    int32_t q = ((int32_t)(delta < 0 ? -delta : delta) << 8)
                / (int32_t)length;
    return (int16_t)(delta < 0 ? -q : q);
}

void plat_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                    uint8_t vctfad, uint8_t flags)
{
    uint8_t period = (uint8_t)(vctfad + 1u);
    uint8_t fadcnt;
    int16_t dx, dy;
    uint16_t adx, ady, length, i;
    int32_t xx, yy, xinc, yinc;

    if (period == 0) {            /* VCTFAD == 0xFF: invisible */
        return;
    }
    fadcnt = period;

    dx = (int16_t)(x1 - x0);
    dy = (int16_t)(y1 - y0);
    adx = (uint16_t)(dx < 0 ? -dx : dx);
    ady = (uint16_t)(dy < 0 ? -dy : dy);
    length = (ady < adx) ? adx : ady;
    if (length == 0) {
        return;
    }

    xinc = incre(dx, length);
    yinc = incre(dy, length);

    xx = ((int32_t)x0 << 8) | 0x80;   /* X0 + 0.5 */
    yy = ((int32_t)y0 << 8) | 0x80;

    for (i = length; i != 0; --i) {
        --fadcnt;
        if (fadcnt == 0) {
            int16_t xi = (int16_t)(xx >> 8);
            int16_t yi = (int16_t)(yy >> 8);
            fadcnt = period;
            if ((xi & (int16_t)0xFF00) == 0 &&
                yi >= 0 && yi < 192) {
                uint8_t *p = fb + rowbase[yi] + ((uint8_t)xi & 0xF8u);
                uint8_t m = bit_mask[(uint8_t)xi & 7u];
                if (flags & PLAT_LINE_INVERSE) {
                    *p &= (uint8_t)~m;
                } else {
                    *p |= m;
                }
            }
        }
        xx += xinc;
        yy += yinc;
    }
}

/* A character cell is 8 consecutive bytes in this layout; only the 7
 * glyph scanlines are written (COMTXT.ASM deposits 7 bytes).
 * No bounds checks here or in plat_invert_region (unlike desktop/):
 * callers are core/viewer_text.c grid arithmetic, whose col/row stay
 * inside the 32x24 cell grid by construction - fb ends flush against
 * the program image, so an out-of-range row would corrupt code. */
void plat_blit_glyph(uint8_t col, uint8_t row, const uint8_t rows[7])
{
    uint8_t *p = fb + ((uint16_t)row << 8) + ((uint16_t)col << 3);
    uint8_t k;
    for (k = 0; k < 7u; ++k) {
        p[k] = rows[k];
    }
}

void plat_invert_region(uint8_t col, uint8_t row, uint8_t ncols)
{
    uint8_t *p = fb + ((uint16_t)row << 8) + ((uint16_t)col << 3);
    uint8_t k, c;
    for (c = 0; c < ncols; ++c) {
        for (k = 0; k < 7u; ++k) {
            p[k] ^= 0xFFu;
        }
        p += 8;
    }
}

void plat_present(void)
{
    /* 24 row-block DMAs (256 bytes each into the 320-byte bitmap
     * rows); still well under a millisecond - no double buffer */
    uint8_t r;
    for (r = 0; r < 24u; ++r) {
        plat_lcopy((uint32_t)(uintptr_t)fb + ((uint16_t)r << 8),
                   BITMAP_ADDR + (uint16_t)r * 320u + 32u, 256u);
    }
}

/* ---- sound: implementation in sound_mega65.c --------------------------- */
extern void snd_load_blob(void);

/* ---- input -------------------------------------------------------------- */
/* $D610: oldest queued key as ASCII (0 = empty); any write pops it. */
int16_t plat_poll_key(void)
{
    uint8_t a = PEEK(0xD610);
    if (a == 0) {
        return -1;
    }
    POKE(0xD610, 0);
    if (a >= 'a' && a <= 'z') {
        a -= 32;                              /* unshifted -> upper */
    } else if (a == 0x14) {
        a = 0x08;                             /* INST/DEL -> backspace */
    }
    if ((a >= 'A' && a <= 'Z') || (a >= '0' && a <= '9') ||
        a == ' ' || a == '\r' || a == 0x08) {
        return (int16_t)a;
    }
    return -1;                                /* cursor keys etc: drop */
}

/* ---- timing -------------------------------------------------------------- */
/* Maintained by irq.s; 16-bit reads must be atomic against it. */
volatile jiffy_t isr_jiffies;
volatile uint8_t isr_phase;
volatile uint8_t isr_sixty;
extern void irq_handler(void);

jiffy_t plat_jiffies(void)
{
    jiffy_t j;
    asm volatile ("sei" ::: "memory");
    j = isr_jiffies;
    asm volatile ("cli" ::: "memory");
    return j;
}

void plat_yield(void)
{
    /* no HALT on the 6502; the wait loops spin on plat_jiffies() */
}

/* ---- persistence: one save slot, overwritten in place ------------------ */
/* Hyppo's writefile trap ($1C, hyppo_write.s) writes whole sectors of
 * the CURRENT open file and never grows it, so DAGGOR65.SAV must
 * already exist on the SD's FAT root with at least SAVE_BLOB_LEN
 * (~3K) bytes - the release ships a zeroed 4K one.  ZSAVE fails
 * gracefully (PLAT_ERR_IO -> the game's error path) if it's absent. */
#include <mega65/fileio.h>
extern uint8_t write512(const uint8_t *buf);   /* hyppo_write.s */

static const char SAVE_NAME[] = "DAGGOR65.SAV";

/* The shipped slot file is exactly this big (Makefile dd rule); hyppo
 * never grows a file, so a blob past this would silently truncate -
 * refuse instead.  Checked at runtime because SAVE_BLOB_LEN is private
 * to the frozen core (player_cmds.c) and only reaches us as `len`. */
#define SAVE_SLOT_BYTES 4096u

/* Failed-save signal: the frozen core discards plat_save_state()'s
 * status (ZLOAD reports via CMDERR; ZSAVE has no error path), so flash
 * the border red - otherwise a missing/failed DAGGOR65.SAV looks like
 * a good save. */
static void save_fail_flash(void)
{
    uint8_t old = VICIV.bordercol;
    uint8_t i;
    for (i = 0; i < 3u; ++i) {
        jiffy_t t0;
        VICIV.bordercol = 2;               /* red */
        t0 = plat_jiffies();
        while ((jiffy_t)(plat_jiffies() - t0) < 12u) { }
        VICIV.bordercol = old;
        t0 = plat_jiffies();
        while ((jiffy_t)(plat_jiffies() - t0) < 8u) { }
    }
}

uint8_t plat_save_state(const void *buf, uint16_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    uint8_t fd;
    uint8_t ok = 1;
    if (len > SAVE_SLOT_BYTES) {
        save_fail_flash();
        return PLAT_ERR_IO;
    }
    fd = open((char *)SAVE_NAME);
    if (fd == 0xFFu) {
        save_fail_flash();
        return PLAT_ERR_IO;
    }
    while (len > 0 && ok) {
        /* always a full sector from the caller's buffer; the tail
         * read-over on the last chunk is benign bank-0 RAM and load
         * never reads those bytes back */
        ok = write512(p);
        p += 512;
        len = (uint16_t)(len > 512u ? len - 512u : 0u);
    }
    close(fd);
    if (!ok) {
        save_fail_flash();
        return PLAT_ERR_IO;
    }
    return PLAT_OK;
}

extern uint8_t save_bounce[512];  /* absolute low RAM, layout.s */

uint8_t plat_load_state(void *buf, uint16_t len)
{
    uint8_t *sec = save_bounce;   /* bounce for the final partial sector */
    uint8_t *p = (uint8_t *)buf;
    uint8_t fd;
    uint8_t ok = 1;
    if (len > SAVE_SLOT_BYTES) {
        return PLAT_ERR_IO;
    }
    fd = open((char *)SAVE_NAME);
    if (fd == 0xFFu) {
        return PLAT_ERR_IO;
    }
    while (len >= 512u && ok) {
        ok = (read512(p) == 512u);
        p += 512;
        len = (uint16_t)(len - 512u);
    }
    if (len > 0 && ok) {          /* read512 always writes 512 bytes */
        ok = (read512(sec) >= len);
        memcpy(p, sec, len);
    }
    close(fd);
    return ok ? PLAT_OK : PLAT_ERR_IO;
}

/* ---- lifecycle ----------------------------------------------------------- */
void plat_init(void)
{
    uint16_t y;

    /* entered with unknown I/D flags when injected via the monitor */
    asm volatile ("sei\n\tcld");

    /* MEGA65 I/O personality (also enables $D610 and the DMAgic MB
     * options), then full speed */
    VICIV.key = 0x47;                        /* 'G' */
    VICIV.key = 0x53;                        /* 'S' */
    VICIV.ctrlc |= 0x40;                     /* VFAST: 40 MHz          */
    POKE(0x0000, 65);                        /* CPU port speed = full  */

    /* legacy mode bits first, while the VIC-IV hot registers still
     * recompute the derived state (linestep, chrcount, borders) for
     * the classic 40x25 geometry ... */
    VICIV.sdbdrwd_msb |= 0x80;               /* HOTREG on              */
    VICIV.ctrlb = 0x40;                      /* VIC-III: FAST only     */
    VICIV.ctrl1 = 0x3B;                      /* BMM | DEN | RSEL | y=3 */
    VICIV.ctrl2 = 0xC8;                      /* 40 cols, MCM off, x=0  */
    VICIV.addr  = 0x18;                      /* legacy D018 (replaced) */
    /* ... then freeze them and point the fetches at bank-1 chip RAM */
    VICIV.sdbdrwd_msb &= (uint8_t)~0x80;     /* HOTREG off             */
    VICIV.scrnptr = MATRIX_ADDR;
    VICIV.charptr_lsb = (uint8_t)(BITMAP_ADDR & 0xFFu);
    VICIV.charptr_msb = (uint8_t)((BITMAP_ADDR >> 8) & 0xFFu);
    VICIV.charptr_bnk = (uint8_t)(BITMAP_ADDR >> 16);
    VICIV.bordercol = 0x00;
    VICIV.screencol = 0x00;

    for (y = 0; y < 192u; ++y) {
        rowbase[y] = (uint16_t)(((y >> 3) << 8) + (y & 7u));
    }

    lfill(BITMAP_ADDR, 0x00, 8000);          /* blank the live bitmap  */
    lfill(MATRIX_ADDR, 0x10, 1000);          /* white ink, black paper */

    snd_load_blob();                         /* hyppo SD read; silent
                                                no-op if absent.  Uses
                                                fb as bounce buffer */
    plat_clear();

    /* jiffy clock: raster IRQ only, all CIA sources off */
    POKE(0xDC0D, 0x7F); (void)PEEK(0xDC0D);  /* CIA1 IRQs off + ack    */
    POKE(0xDD0D, 0x7F); (void)PEEK(0xDD0D);  /* CIA2 NMIs off + ack    */
    isr_jiffies = 0;
    isr_phase = 5;
    isr_sixty = (VICIV.rasline0 & 0x80u) ? 1u : 0u;   /* $D06F.7 NTSC  */
    POKE(0x0314, (uint16_t)(uintptr_t)irq_handler & 0xFFu);
    POKE(0x0315, (uint16_t)(uintptr_t)irq_handler >> 8);
    VICIV.rasterline = 0xFF;                 /* compare line 255       */
    VICIV.ctrl1 &= 0x7Fu;                    /* compare bit 8 = 0      */
    VICIV.imr = 0x01;                        /* raster IRQ enable      */
    VICIV.irr = 0xFF;                        /* ack anything pending   */
    asm volatile ("cli");
}

void plat_shutdown(void)
{
}
