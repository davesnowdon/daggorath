/* plat_next.c - ZX Spectrum Next backend.
 *
 * Video    ULA 256x192 bitmap; the game draws into bank 7's low page
 *          (MMU2 window at 0x4000) and plat_present() flips it to the
 *          visible bank 5.  Line rasterizer = draw_z80n.asm, verified
 *          pixel-identical to core/draw_ref.c by tests/z80draw.
 * Input    ISR matrix scan (kbd_isr_scan) with edge detection into a
 *          16-slot ring; typed-ahead input survives CPU-bound stalls.
 * Timing   IM2 frame ISR (isr.asm); a +1,+1,+1,+1,+2 accumulator
 *          yields exactly 60 jiffies per 50 frames, and 60 Hz display
 *          modes (REG_PERIPHERAL_1 rate bit, read at init) count 1:1.
 * Sound    sound_next.c: the packed 7350 Hz blob esxDOS-loaded into
 *          8K pages, played through zxnDMA -> Covox with a volume-LUT
 *          scaling pass (snd_scale.asm); verified by tests/next-sound.
 * Saves    plat_save/load_state via esxDOS (daggorath.sav); verified
 *          byte-exact by tests/next-save.
 */
#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <intrinsic.h>
#include <im2.h>
#include <string.h>
#include <stdint.h>

#include "platform.h"

#define ULA_BITMAP ((uint8_t *)0x4000)
#define ULA_ATTR   ((uint8_t *)0x5800)

/* ULA scanline base addresses (thirds/character interleave) */
static uint8_t *row_addr[192];

/* ---- video ------------------------------------------------------------ */
void plat_clear(void)
{
    memset(ULA_BITMAP, 0, 6144);
}

/* The shipping rasterizer is draw_z80n.asm - EXACTLY the VECTOR.ASM
 * algorithm (24.8 fixed point, plot-candidate-then-step, endpoint
 * never plotted, dot-period fades, x-clip on the integer high byte),
 * verified pixel-identical to the normative core/draw_ref.c by
 * tests/z80draw/run.sh under z88dk-ticks.  (An untested C copy of the
 * DDA lived here behind DRAW_C_FALLBACK until 2026-07-17; deleted as
 * unverified drift risk - core/draw_ref.c IS the reference.) */
extern void plat_draw_line_asm(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                               uint8_t vctfad, uint8_t flags);

void plat_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                    uint8_t vctfad, uint8_t flags)
{
    plat_draw_line_asm(x0, y0, x1, y1, vctfad, flags);
}

/* No bounds checks here or in plat_invert_region (unlike desktop/):
 * callers are core/viewer_text.c grid arithmetic, whose col/row stay
 * inside the 32x24 cell grid by construction. */
void plat_blit_glyph(uint8_t col, uint8_t row, const uint8_t rows[7])
{
    uint8_t y = (uint8_t)(row << 3);
    uint8_t k;
    for (k = 0; k < 7u; ++k) {
        *(row_addr[y + k] + col) = rows[k];
    }
}

void plat_invert_region(uint8_t col, uint8_t row, uint8_t ncols)
{
    uint8_t y = (uint8_t)(row << 3);
    uint8_t k, c;
    for (k = 0; k < 7u; ++k) {
        uint8_t *p = row_addr[y + k] + col;
        for (c = 0; c < ncols; ++c) {
            p[c] ^= 0xFFu;
        }
    }
}

void plat_present(void)
{
    /* The draw buffer is bank 7's low 8K page mapped at 0x4000; the
     * ULA displays bank 5.  Present = copy the bitmap back: map bank
     * 5's low page over the ROM at 0x0000 (our IM2 handler and vector
     * table live in top RAM, so interrupts stay safe), LDIR, restore.
     * 6144 bytes at 28 MHz is ~4-5 ms - slow redraws become one clean
     * flip instead of visible line-by-line painting. */
    ZXN_NEXTREG(0x50, 10);            /* MMU0 <- bank 5 low page */
    memcpy((void *)0x0000, ULA_BITMAP, 6144);
    ZXN_NEXTREG(0x50, 0xFF);          /* MMU0 <- ROM */
}

/* ---- sound: real implementation lives in sound_next.c ------------------ */
extern void snd_load_blob(void);

/* ---- input ------------------------------------------------------------ */
/* Direct 8x5 matrix scan of port 0xFE with per-key edge detection (a
 * newly-pressed key emits once).  Only the keys the game uses map to
 * ASCII; CAPS+0 (ZX DELETE) emits backspace. */
static uint8_t kbd_row(uint8_t half) __z88dk_fastcall __naked
{
    (void)half;
    __asm
    ld b, l
    ld c, 0xfe
    in a, (c)
    ld l, a
    ret
    __endasm;
}

/* half-row select bytes and their key->ASCII maps, bit 0..4 */
static const uint8_t row_sel[8] = {
    0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F
};
static const uint8_t row_ascii[8][5] = {
    { 0,   'Z', 'X', 'C', 'V' },   /* bit0 = CAPS SHIFT */
    { 'A', 'S', 'D', 'F', 'G' },
    { 'Q', 'W', 'E', 'R', 'T' },
    { 0,   0,   0,   0,   0   },   /* 1 2 3 4 5 unused */
    { 0,   0,   0,   0,   0   },   /* 0 9 8 7 6: CAPS+0 = delete */
    { 'P', 'O', 'I', 'U', 'Y' },
    { 0x0D,'L', 'K', 'J', 'H' },
    { ' ', 0,   'M', 'N', 'B' },   /* bit1 = SYMBOL SHIFT */
};

/* The scan runs from the frame ISR (isr.asm calls kbd_isr_scan), so a
 * key pressed while the game is CPU-bound - maze generation takes
 * whole seconds - still lands in the queue.  Single-producer (ISR) /
 * single-consumer (plat_poll_key) byte ring: race-free without DI. */
static volatile uint8_t kq[16];
static volatile uint8_t kq_head, kq_tail;

void kbd_isr_scan(void)
{
    static uint8_t prev[8] = {
        0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F
    };
    uint8_t r, b;

    for (r = 0; r < 8u; ++r) {
        uint8_t now = kbd_row(row_sel[r]) & 0x1Fu;
        uint8_t edges = (uint8_t)(prev[r] & (uint8_t)~now); /* 1->0 = press */
        prev[r] = now;
        if (edges == 0) {
            continue;
        }
        for (b = 0; b < 5u; ++b) {
            if (edges & (uint8_t)(1u << b)) {
                uint8_t a = row_ascii[r][b];
                if (r == 4u && b == 0u &&
                    (kbd_row(0xFEu) & 0x01u) == 0u) {
                    a = 0x08;                /* CAPS+0 = delete */
                }
                if (a != 0u) {
                    /* full-queue guard: drop the NEWEST key rather than
                     * advance tail onto head (which reads as "empty" and
                     * silently discards the whole typed-ahead backlog) */
                    uint8_t next = (uint8_t)((kq_tail + 1u) & 15u);
                    if (next != kq_head) {
                        kq[kq_tail] = a;
                        kq_tail = next;
                    }
                }
            }
        }
    }
}

int16_t plat_poll_key(void)
{
    uint8_t k;
    if (kq_head == kq_tail) {
        return -1;
    }
    k = kq[kq_head];
    kq_head = (uint8_t)((kq_head + 1u) & 15u);
    return (int16_t)k;
}

/* ---- timing ------------------------------------------------------------ */
/* The IM2 frame ISR (isr.asm) maintains the jiffy counter with the
 * 6/5 phase pattern; reads must be atomic against it. */
extern volatile jiffy_t isr_jiffies;
extern volatile uint8_t isr_sixty;    /* 1 = 60 Hz display: no 6/5 */

jiffy_t plat_jiffies(void)
{
    jiffy_t j;
    intrinsic_di();
    j = isr_jiffies;
    intrinsic_ei();
    return j;
}

void plat_yield(void)
{
    intrinsic_halt();             /* wait for the 50 Hz frame interrupt */
}

/* ---- persistence: one esxDOS save slot next to the .nex ---------------- */
static const char SAVE_NAME[] = "daggorath.sav";

/* Failed-save signal: the frozen core discards plat_save_state()'s
 * status (ZLOAD reports via CMDERR; ZSAVE has no error path), so flash
 * the border red - otherwise a full/absent SD looks like a good save. */
static void save_fail_flash(void)
{
    uint8_t i;
    for (i = 0; i < 3u; ++i) {
        jiffy_t t0;
        zx_border(INK_RED);
        t0 = plat_jiffies();
        while ((jiffy_t)(plat_jiffies() - t0) < 12u) { }
        zx_border(INK_BLACK);
        t0 = plat_jiffies();
        while ((jiffy_t)(plat_jiffies() - t0) < 8u) { }
    }
}

uint8_t plat_save_state(const void *buf, uint16_t len)
{
    uint8_t h = esx_f_open(SAVE_NAME,
                           ESX_MODE_W | ESX_MODE_OPEN_CREAT_TRUNC);
    uint16_t n;
    if (h == 0xFFu) {
        save_fail_flash();
        return PLAT_ERR_IO;
    }
    n = (uint16_t)esx_f_write(h, (void *)buf, len);
    esx_f_close(h);
    if (n != len) {
        save_fail_flash();
        return PLAT_ERR_IO;
    }
    return PLAT_OK;
}

uint8_t plat_load_state(void *buf, uint16_t len)
{
    uint8_t h = esx_f_open(SAVE_NAME, ESX_MODE_R | ESX_MODE_OPEN_EXIST);
    uint16_t n;
    if (h == 0xFFu) {
        return PLAT_ERR_IO;
    }
    n = (uint16_t)esx_f_read(h, buf, len);
    esx_f_close(h);
    return (n == len) ? PLAT_OK : PLAT_ERR_IO;
}

/* ---- lifecycle ---------------------------------------------------------- */
/* IM2 vector table: 257 bytes of 0xFD at 0xFE00, JP stub at 0xFDFD.
 * REGISTER_SP (zpragma.inc) sits just below the stub. */
#define IM2_TABLE ((uint8_t *)0xFE00)
#define IM2_STUB  ((uint8_t *)0xFDFD)

extern void frame_isr(void);

void plat_init(void)
{
    uint16_t y;

    ZXN_NEXTREG(REG_TURBO_MODE, 3);   /* 28 MHz (macro absent in this
                                         z88dk's headers) */

    for (y = 0; y < 192u; ++y) {
        row_addr[y] = ULA_BITMAP
                    + (((y & 0xC0u) << 5) | ((y & 0x07u) << 8)
                       | ((y & 0x38u) << 2));
    }

    zx_border(INK_BLACK);
    /* attributes go to the VISIBLE screen (bank 5) before the remap;
     * present() only ever copies the bitmap */
    memset(ULA_ATTR, PAPER_BLACK | INK_WHITE, 768);

    /* from here on, 0x4000 is the DRAW buffer: bank 7's low page */
    ZXN_NEXTREG(0x52, 14);            /* MMU2 <- bank 7 low page */
    plat_clear();
    plat_present();                   /* blank the visible screen too */

    /* the ROM IM1 handler needs IY = sysvars, which the sdcc_iy CLIB
     * owns - so interrupts run through our own IM2 handler instead */
    memset(IM2_TABLE, 0xFD, 257);
    IM2_STUB[0] = 0xC3;               /* JP frame_isr */
    IM2_STUB[1] = (uint8_t)((uint16_t)frame_isr & 0xFFu);
    IM2_STUB[2] = (uint8_t)((uint16_t)frame_isr >> 8);
    im2_init(IM2_TABLE);

    /* 60 Hz displays deliver one jiffy per frame; 50 Hz uses the 6/5
     * accumulator in the ISR */
    isr_sixty = (uint8_t)
        ((ZXN_READ_REG(REG_PERIPHERAL_1) & RP1_RATE_60) ? 1 : 0);

    snd_load_blob();

    intrinsic_ei();
}

void plat_shutdown(void)
{
}
