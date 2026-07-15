/* plat_ep.c - Enterprise 64/128 backend (Phase 1: video first light, all C).
 *
 * Video    Nick 2-colour PIXEL mode, a LINEAR 256x192 1bpp framebuffer
 *          (32-byte stride, 8 px/byte, bit 7 = leftmost) in a video
 *          segment paged into Z80 page 3.  Single displayed buffer, so
 *          plat_present() is a no-op (visible line-by-line drawing is
 *          CoCo-authentic).  The C line rasterizer here must match
 *          core/draw_ref.c pixel for pixel; draw_ep.asm replaces it in
 *          Phase 3.
 * Input    stubbed for first light (Phase 2 adds the port-B5h matrix scan).
 * Timing   stubbed jiffy counter ticking on plat_yield (Phase 2 takes over
 *          IM1 with the tone0 interrupt).
 * Sound    stubbed (Phase 4 = Dave DAC PCM).
 *
 * Segment numbers are DISCOVERED at init (Nick sees only segments
 * FCh-FFh); nothing is hard-coded.  See docs / vault note 02 + 06.
 */
#include <stdint.h>
#include <string.h>

#include "platform.h"

/* ---- video memory ------------------------------------------------------ */
/* Page 3 (C000-FFFF) = video segment FF (Nick 0xC000-0xFFFF).  DOUBLE
 * BUFFERED: two 6144-byte bitmaps, FB0 at Z80 0xC000 (Nick 0xC000) and FB1 at
 * Z80 0xD800 (Nick 0xD800); the Line Parameter Table is parked at 0xF800,
 * clear of both.  The core always recomposes a whole frame (clear + full
 * redraw) before plat_present, so we draw into the hidden buffer and flip the
 * LPT's line pointer on present -> no visible clear-to-black flicker.  For the
 * FF segment the Z80 address equals the Nick address, so a buffer's Z80 base
 * IS its LD1 value. */
#define FB0_BASE  ((uint8_t *)0xC000)
#define FB1_BASE  ((uint8_t *)0xD800)
#define LPT_BASE  ((uint8_t *)0xF800)
#define LPT_LD1   ((uint8_t *)0xF804)   /* visible record LD1 lo/hi (bytes 4-5) */
#define FB_STRIDE 32u
#define FB_BYTES  6144u

/* The buffer the game currently draws into (the hidden one). */
static uint8_t *draw_base = FB1_BASE;

/* row_addr[y] = draw_base + y*32 (linear; kept as a table so the hot plot
 * path is one index, matching the Next backend's shape). */
static uint8_t *row_addr[192];

static const uint8_t bit_mask[8] = {
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};

static void ep_rebuild_row_addr(void)
{
    uint16_t y;
    for (y = 0; y < 192u; ++y) {
        row_addr[y] = draw_base + (y * FB_STRIDE);
    }
}

/* ---- video ------------------------------------------------------------- */
/* Clear the HIDDEN bitmap (draw_base) with a discrete-write loop.  We
 * deliberately do NOT use the library memset here: its fill uses an LDIR whose
 * source and destination are BOTH in Nick video RAM, and that read-video/
 * write-video block move derails on this platform (a plain discrete-store loop
 * is reliable). */
void plat_clear(void)
{
    __asm
        ld   hl, (_draw_base)       ; hidden buffer base
        ld   de, #0x1800            ; FB_BYTES
    pcclr$:
        ld   (hl), #0x00
        inc  hl
        dec  de
        ld   a, d
        or   e
        jr   nz, pcclr$
    __endasm;
}

/* EXACTLY the VECTOR.ASM / core/draw_ref.c algorithm: 24.8 fixed point,
 * plot-candidate-then-step, endpoint never plotted, dot-period fades,
 * x-clip on the integer high byte.  Only the plot primitive differs from
 * draw_ref.c: bit-packed linear bytes instead of a byte-per-pixel map.
 * draw_ep.asm supersedes this in Phase 3 (proven identical under ticks). */
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
                uint8_t *p = row_addr[yi] + ((uint8_t)xi >> 3);
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

/* Flip: publish the just-drawn hidden buffer, then hand the old front buffer
 * back as the new draw target.  Nick reloads LD1 from the LPT only at the top
 * of each frame, so writing the LPT bytes mid-frame can't tear the current
 * frame.  We then wait for the frame boundary so the flip is live before we
 * start clearing the old front buffer (which would otherwise still be on
 * screen).  Z80 base == Nick address in the FF segment, so draw_base IS LD1. */
void plat_present(void)
{
    uint16_t a = (uint16_t)draw_base;
    LPT_LD1[0] = (uint8_t)a;             /* LD1 lo */
    LPT_LD1[1] = (uint8_t)(a >> 8);      /* LD1 hi */

    __asm
        ppw0$:                           ; wait for frame-sync flag to clear
            in   a, (0xb4)
            and  #0x10
            jr   nz, ppw0$
        ppw1$:                           ; then the rising edge = new frame live
            in   a, (0xb4)
            and  #0x10
            jr   z, ppw1$
    __endasm;

    draw_base = (draw_base == FB0_BASE) ? FB1_BASE : FB0_BASE;
    ep_rebuild_row_addr();
}

/* ---- sound: stubbed for Phase 1 --------------------------------------- */
void plat_sound_play(uint8_t sound_id, uint8_t volume)
{
    (void)sound_id;
    (void)volume;
}

void plat_sound_stop(void) { }

uint8_t plat_sound_playing(void) { return 0; }

/* ---- input: stubbed for Phase 1 (Phase 2 = port-B5h matrix scan) ------ */
int16_t plat_poll_key(void)
{
    return -1;
}

/* ---- timing: VSYNC-polled jiffies for first light --------------------- */
/* The core paces itself on jiffies.  Interrupts are OFF (we own the machine
 * bare-metal), so instead of an ISR we poll the Nick frame-sync flag
 * (port B4h bit 4) and advance one jiffy per frame -> ~50 Hz PAL pacing.
 * That is a hair slower than the core's 60 Hz model but STABLE, so the
 * attract/title animation renders at a sane rate instead of racing.
 * Phase 2 replaces this with the tone0 IM1 jiffy accumulator (true 60 Hz). */
static jiffy_t stub_jiffies;

jiffy_t plat_jiffies(void)
{
    return stub_jiffies;
}

void plat_yield(void)
{
    __asm
    ywl0$:                          ; wait for frame-sync flag to be low
        in   a, (0xb4)
        and  #0x10
        jr   nz, ywl0$
    ywl1$:                          ; then wait for the rising edge
        in   a, (0xb4)
        and  #0x10
        jr   z, ywl1$
    __endasm;
    ++stub_jiffies;
}

/* ---- persistence: stubbed for Phase 1 (Phase 5 = EXOS channel I/O) ----- */
uint8_t plat_save_state(const void *buf, uint16_t len)
{
    (void)buf;
    (void)len;
    return PLAT_ERR_UNSUPPORTED;
}

uint8_t plat_load_state(void *buf, uint16_t len)
{
    (void)buf;
    (void)len;
    return PLAT_ERR_UNSUPPORTED;
}

/* ---- lifecycle -------------------------------------------------------- */
/* We take the machine over bare-metal (the plan's model): DI so the EXOS
 * 50 Hz IM1 handler stops touching the system segment we page out; switch
 * the Z80 to no-wait 4 MHz; page a Nick-visible video segment into Z80
 * page 3 so the framebuffers/LPT (C000/D800/F800) are writable.
 *
 * The loader hands us the machine with segments FCh/FDh/FEh in Z80 pages
 * 0-2 (our code) and EXOS's system segment FFh in page 3.  FFh is EXOS's
 * (its LPT/vars live there) up to this point; after our DI here EXOS is
 * dead, so we repurpose FFh as our VIDEO segment.  Nick addressing
 * (vault 06):  Nick addr = (segment - 0xFC) * 0x4000 + offset.
 * segment FFh in Z80 page 3 => FB0(0xC000)/FB1(0xD800) = Nick 0xC000/0xD800
 * bitmaps, LPT_BASE(0xF800) = Nick 0xF800.
 */
#define VID_SEG   0xFFu           /* Nick-visible segment we repurpose     */
#define LPT_NICK  0xF800u         /* Nick address of LPT                    */

/* Immediate-port OUT (all Nick/Dave ports are compile-time constants). */
static void ep_hw_takeover(void) __naked
{
    __asm
        di
        ld   a, #0x0c
        out  (0xbf), a           ; system config: b3,b2=11 -> no wait states
        ld   a, #0xff            ; VID_SEG (was the EXOS system seg; EXOS now dead)
        out  (0xb3), a           ; page it into Z80 page 3 (C000-FFFF)
        xor  a
        out  (0x81), a           ; BORDER = black (matches COL0; the framebuffer
                                 ; sits on a clean full-screen black field)
        out  (0x80), a           ; FIXBIAS = 0 (palette 8-15, unused here)
        ret
    __endasm;
}

/* Point Nick at the LPT.  LPT at Nick 0xF800: port 82h = (a>>4)&0xFF =
 * 0x80; port 83h = ((a>>12)&0x0F) | 0xC0 = 0xCF (LP=1,LC=1 = run). */
static void ep_nick_lpt(void) __naked
{
    __asm
        ld   a, #0x80
        out  (0x82), a           ; LPT base A4-A11
        ld   a, #0xcf
        out  (0x83), a           ; LPT base A12-A15 + LP/LC run
        ret
    __endasm;
}

/* Full-frame Nick LPT: mode byte = I CC R MMM L (nick.cpp: colorMode
 * (b6:5), VRES (b4), videoMode (b3:1), reload (b0)); a 2-colour PIXEL
 * a 2-colour LPIXEL line is 0x1E (CC=00, VRES=1, MMM=111).  We use LPIXEL
 * (videoMode 7), NOT PIXEL: PIXEL fetches TWO bytes/slot (16 narrow px) so a
 * 32-byte line is only 16 slots wide -> a tall, narrow portrait image.  LPIXEL
 * fetches ONE byte/slot (8 double-width px, nick.cpp renderByte2ColorsL), so
 * the 32-byte line spans 32 slots = the full active width at the CoCo's 4:3
 * proportions.  Margins left 15 / right 47 (RM-LM = 32 slots).  With VRES on,
 * LD1 walks the bitmap continuously so the visible record's LD1 = Nick 0xC000
 * draws a linear 32-byte-stride framebuffer over all 192 lines.  Each display
 * line consumes exactly 32 bytes, so 192 lines span 6144 bytes (0xC000-0xD7FF)
 * and stop cleanly below the LPT.  The VSYNC /
 * border record chain (line counts, IRQ-at-half-line marker) is the
 * proven ChibiAkumas EP layout, rebalanced to 192 visible: 54+54 border +
 * 12 sync = 312 total PAL lines.  Record 5 (0x90) carries the future
 * video-IRQ (Phase 2). */
static const uint8_t lpt_data[] = {
    /* SC        MB    LM   RM   LD1lo LD1hi LD2  ..  COL0  COL1  COL2..7 */
    0x40, 0x1E,  15,  47, 0x00, 0xC0, 0,0, 0x00, 0xFF, 0,0,0,0,0,0, /* 192 vis LPIXEL, 32 slots */
    0xCA, 0x12,  63,   0, 0,0,0,0,      0,0,0,0,0,0,0,0,             /* 54 bottom border */
    0xFD, 0x10,  63,   0, 0,0,0,0,      0,0,0,0,0,0,0,0,             /* 3 sync off */
    0xFC, 0x10,   6,  63, 0,0,0,0,      0,0,0,0,0,0,0,0,             /* 4 sync on */
    0xFF, 0x90,  63,  32, 0,0,0,0,      0,0,0,0,0,0,0,0,             /* 1 IRQ line */
    0xFC, 0x12,   6,  63, 0,0,0,0,      0,0,0,0,0,0,0,0,             /* 4 black */
    0xCA, 0x13,  63,   0, 0,0,0,0,      0,0,0,0,0,0,0,0              /* 54 top, reload */
};

/* Copy the LPT into video RAM.  LDIR here reads from normal RAM (the rodata
 * lpt_data) and writes video RAM, which is fine (proven by the standalone
 * vtest); only the video->video memset fill was problematic. */
static void ep_build_lpt(void)
{
    __asm
        ld   hl, #_lpt_data
        ld   de, #0xf800            ; LPT_BASE (Nick 0xF800)
        ld   bc, #112              ; sizeof lpt_data
        ldir
    __endasm;
}

void plat_init(void)
{
    ep_hw_takeover();                  /* DI, fast mode, page VID_SEG @ P3 */

    /* Clear both buffers so neither shows EXOS/loader leftovers.  The LPT's
     * LD1 starts on FB0 (front); the game draws into FB1 (draw_base) first. */
    draw_base = FB0_BASE;
    ep_rebuild_row_addr();
    plat_clear();
    draw_base = FB1_BASE;
    ep_rebuild_row_addr();
    plat_clear();

    ep_build_lpt();
    ep_nick_lpt();                      /* Nick now scans our LPT (showing FB0) */
}

void plat_shutdown(void)
{
}
