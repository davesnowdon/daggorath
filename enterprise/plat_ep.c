/* plat_ep.c - Enterprise 64/128 backend (Phase 1: video first light, all C).
 *
 * Video    Nick 2-colour PIXEL mode, a LINEAR 256x192 1bpp framebuffer
 *          (32-byte stride, 8 px/byte, bit 7 = leftmost) in a video
 *          segment paged into Z80 page 3.  Single displayed buffer, so
 *          plat_present() is a no-op (visible line-by-line drawing is
 *          CoCo-authentic).  The C line rasterizer here must match
 *          core/draw_ref.c pixel for pixel; draw_ep.asm replaces it in
 *          Phase 3.
 * Input    port-B5h keyboard matrix, scanned from the frame ISR with edge
 *          detection into a lock-free ring (plat_poll_key drains it).
 * Timing   IM1 takeover.  The Nick video interrupt (50 Hz PAL) drives a
 *          +1,+1,+1,+1,+2 jiffy accumulator (isr_ep.asm) = true 60 Hz;
 *          plat_yield HALTs to the frame.
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

/* The buffer the game currently draws into (the hidden one).  NOT static:
 * draw_ep.asm reads it per call (and rebuilds its row-high lookup when the
 * high byte changes - both buffers are 256-aligned). */
uint8_t *draw_base = FB1_BASE;

/* row_addr[y] = draw_base + y*32 (linear; kept as a table so the hot plot
 * path is one index, matching the Next backend's shape). */
static uint8_t *row_addr[192];

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

/* The shipping rasterizer is draw_ep.asm (plain Z80, linear stride, solid
 * H/V fast paths), verified pixel-identical to core/draw_ref.c by
 * tests/z80draw-ep/run.sh under z88dk-ticks.  Build with -DDRAW_C_FALLBACK
 * to use the original C DDA below instead - kept compiled in as the
 * reference fallback. */
extern void plat_draw_line_asm(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                               uint8_t vctfad, uint8_t flags);

#ifndef DRAW_C_FALLBACK

void plat_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                    uint8_t vctfad, uint8_t flags)
{
    plat_draw_line_asm(x0, y0, x1, y1, vctfad, flags);
}

#else /* DRAW_C_FALLBACK */

/* EXACTLY the VECTOR.ASM / core/draw_ref.c algorithm: 24.8 fixed point,
 * plot-candidate-then-step, endpoint never plotted, dot-period fades,
 * x-clip on the integer high byte.  Only the plot primitive differs from
 * draw_ref.c: bit-packed linear bytes instead of a byte-per-pixel map. */
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

#endif /* DRAW_C_FALLBACK */

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

#ifdef DOD_HEART_STATUS_ONLY
/* Live blits target the VISIBLE buffer (the one that is not draw_base), so
 * the fast-heartbeat status refresh appears without a present/flip and never
 * disturbs the hidden buffer's in-progress or stale scene.  The main thread
 * runs sched_CLOCK (the only caller) and plat_present serially, so draw_base
 * is stable here - no ISR touches it. */
void plat_blit_glyph_live(uint8_t col, uint8_t row, const uint8_t rows[7])
{
    uint8_t *vbase = (draw_base == FB0_BASE) ? FB1_BASE : FB0_BASE;
    uint8_t y = (uint8_t)(row << 3);
    uint8_t k;
    for (k = 0; k < 7u; ++k) {
        *(vbase + (uint16_t)(y + k) * FB_STRIDE + col) = rows[k];
    }
}

void plat_invert_region_live(uint8_t col, uint8_t row, uint8_t ncols)
{
    uint8_t *vbase = (draw_base == FB0_BASE) ? FB1_BASE : FB0_BASE;
    uint8_t y = (uint8_t)(row << 3);
    uint8_t k, c;
    for (k = 0; k < 7u; ++k) {
        uint8_t *p = vbase + (uint16_t)(y + k) * FB_STRIDE + col;
        for (c = 0; c < ncols; ++c) {
            p[c] ^= 0xFFu;
        }
    }
}
#endif

/* Flip: publish the just-drawn hidden buffer, then hand the old front buffer
 * back as the new draw target.  Nick reloads LD1 from the LPT only at the top
 * of each frame, so writing the LPT bytes mid-frame can't tear the current
 * frame.  We then wait for the frame boundary so the flip is live before we
 * start clearing the old front buffer (which would otherwise still be on
 * screen).  Z80 base == Nick address in the FF segment, so draw_base IS LD1. */
extern volatile uint8_t frame_flag;    /* isr_ep.asm: set on each frame int */

void plat_present(void)
{
    uint16_t a = (uint16_t)draw_base;
    LPT_LD1[0] = (uint8_t)a;             /* LD1 lo */
    LPT_LD1[1] = (uint8_t)(a >> 8);      /* LD1 hi */

    /* Nick re-reads the visible record's LD1 at the top of every frame, so
     * writing the LPT bytes mid-frame can't tear the current frame.  Wait
     * for the next FRAME interrupt (fired near end-of-visible, before the
     * top border + reload) so the flip is live before we hand the old front
     * buffer back as the draw target and the core clears it.  A plain HALT
     * is not enough once sound plays: sample interrupts (up to 7353/s) wake
     * HALT early, so loop on the ISR's frame flag. */
    frame_flag = 0;
    do {
        __asm halt __endasm;
    } while (!frame_flag);

    draw_base = (draw_base == FB0_BASE) ? FB1_BASE : FB0_BASE;
    ep_rebuild_row_addr();
}

/* ---- sound: sound_ep.c (Dave DAC PCM; isr_ep.asm sample engine) -------- */
extern void ep_snd_init(void);            /* sound_ep.c */

/* ---- input: port-B5h keyboard matrix scan ----------------------------- */
/* EP keyboard: OUT (B5h),row selects one of 10 rows (Dave latches row =
 * value & 0x0F), then IN A,(B5h) reads that row's 8 key bits with a CLEARED
 * bit = key pressed (active low).  We map only the keys Daggorath uses
 * (A-Z, space, enter, delete) to raw ASCII; the core's ascii_filter drops
 * everything else.  Matrix verified against ep128emu's ep_keys.cfg
 * (vault note 01 - Hardware Overview). */
static uint8_t kbd_row(uint8_t row) __z88dk_fastcall __naked
{
    (void)row;                       /* arg arrives in L (fastcall) */
    __asm
        ld   a, l
        out  (0xb5), a              ; select keyboard row
        in   a, (0xb5)              ; read its 8 key bits (active low)
        ld   l, a                   ; return in L
        ret
    __endasm;
}

/* row_ascii[row][bit] - the ASCII a cleared bit emits (0 = key we ignore).
 * Columns are bit 0 (b0) .. bit 7 (b7); see the vault matrix table. */
static const uint8_t row_ascii[10][8] = {
    /* b0    b1    b2    b3    b4    b5    b6    b7  */
    { 'N',  0,   'B',  'C',  'V',  'X',  'Z',  0   }, /* 0: LSh Z X V C B \ N */
    { 'H',  0,   'G',  'D',  'F',  'S',  'A',  0   }, /* 1: Ctl A S F D G Lk H */
    { 'U', 'Q',  'Y',  'R',  'T',  'E',  'W',  0   }, /* 2: Tab W E T R Y Q U */
    { 0,    0,    0,    0,    0,    0,    0,    0   }, /* 3: Esc 2 3 5 4 6 1 7 */
    { 0,    0,    0,    0,    0,    0,    0,    0   }, /* 4: function keys     */
    { 0,    0,    0,    0,    0,    0,    0,    0   }, /* 5: Ers ^ 0 - 9 8     */
    { 'J',  0,   'K',   0,   'L',   0,    0,    0   }, /* 6: ] : L ; K J       */
    { 0,    0,    0,    0,    0,    0,   0x0D,  0   }, /* 7: Alt Ent ... Stop  */
    { 'M', 0x08,  0,    0,    0,    0,   0x20,  0   }, /* 8: Ins Sp RSh . / , Del M */
    { 'I',  0,   'O',   0,   'P',   0,    0,    0   }, /* 9: [ P @ O I         */
};

/* The scan runs from the frame ISR (isr_ep.asm calls kbd_isr_scan), so a
 * key pressed while the game is CPU-bound - maze generation takes whole
 * seconds - still lands in the queue.  Single-producer (ISR) / single-
 * consumer (plat_poll_key) byte ring: race-free without DI. */
static volatile uint8_t kq[16];
static volatile uint8_t kq_head, kq_tail;

void kbd_isr_scan(void)
{
    static uint8_t prev[10] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };
    uint8_t r, b;

    for (r = 0; r < 10u; ++r) {
        uint8_t now = kbd_row(r);
        uint8_t edges = (uint8_t)(prev[r] & (uint8_t)~now); /* 1->0 = press */
        prev[r] = now;
        if (edges == 0) {
            continue;
        }
        for (b = 0; b < 8u; ++b) {
            if (edges & (uint8_t)(1u << b)) {
                uint8_t a = row_ascii[r][b];
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

/* ---- timing: ISR-driven jiffies (IM1, Nick video interrupt) ----------- */
/* isr_ep.asm maintains isr_jiffies at 60 Hz (a +1,+1,+1,+1,+2 accumulator
 * over the 50 Hz PAL frame interrupt).  A 16-bit read is not atomic against
 * the ISR, so DI around it. */
extern volatile jiffy_t isr_jiffies;

jiffy_t plat_jiffies(void)
{
    jiffy_t j;
    __asm di __endasm;
    j = isr_jiffies;
    __asm ei __endasm;
    return j;
}

/* HALT parks the CPU until the next (only-enabled) interrupt = the Nick
 * video frame interrupt, so the game paces to the 50 Hz frame while the ISR
 * keeps wall-clock jiffies advancing at 60 Hz independently. */
void plat_yield(void)
{
    __asm halt __endasm;
}

/* ---- persistence: the EXOS dance (dance_ep.asm; Phase 5) --------------- */
/* Implemented in ep_dance() below plat_init (it rebuilds the video state
 * with the same helpers plat_init uses). */
static uint8_t ep_dance(uint8_t op, uint16_t buf, uint16_t len);
static void ep_saves_init(void);

uint8_t plat_save_state(const void *buf, uint16_t len)
{
    return ep_dance(0, (uint16_t)buf, len);
}

uint8_t plat_load_state(void *buf, uint16_t len)
{
    return ep_dance(1, (uint16_t)buf, len);
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

/* Install the IM1 frame ISR and arm the Nick video interrupt.  EXOS is dead
 * (the loader killed it and paged its system segment out), so 0038h is now
 * free RAM in our program segment (P0 = FCh, whose 00-FF the loader never
 * wrote - the game image loads at 0100h): patch a JP _frame_isr there, set
 * IM 1, enable + reset int_1 via Dave port B4h, then EI.  Called last in
 * plat_init, after the framebuffers and LPT are live. */
extern void frame_isr(void);           /* isr_ep.asm: _frame_isr */

static void ep_isr_start(void)
{
    uint16_t v = (uint16_t)&frame_isr;
    *((uint8_t *)0x0038) = 0xC3u;                  /* JP nn */
    *((uint8_t *)0x0039) = (uint8_t)v;             /* lo */
    *((uint8_t *)0x003A) = (uint8_t)(v >> 8);      /* hi */
    __asm
        im   1
        ld   a, #0x30
        out  (0xb4), a           ; enable int_1 (Nick video), reset its latch
        ei
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

    ep_isr_start();                     /* IM1: 60 Hz jiffies + keyboard; EI */

    ep_snd_init();                         /* Dave DAC PCM, if the loader left a
                                         * blob (handoff block at 0x00F0) */

    ep_saves_init();                    /* the EXOS dance, if the loader
                                         * stashed the EXOS state */
}

void plat_shutdown(void)
{
}

/* ---- the EXOS dance (Phase 5): in-game save/load ----------------------- */
/* Full story in dance_ep.asm.  The dance restores the EXOS-state stash
 * into the video segment, revives EXOS for the channel I/O (system
 * segment back at page 2, loader segment's EXOS stub at page 0, blob
 * bounced into the loader segment's dead low RAM), then re-stashes and
 * freezes it again.  Everything it destroys is video state this wrapper
 * rebuilds: the LPT, the rasterizer lookup tables, and FB1's content
 * (FB0 keeps the last frame, minus a 112-byte strip the parked mini-LPT
 * borrowed; the game's next text print / full redraw repaints it). */
#define EP_HANDOFF ((const volatile uint8_t *)0x00F0)

extern volatile uint8_t dance_op, dance_status, dance_loader_seg;
extern volatile uint16_t dance_buf, dance_len;
extern void dance_run(void);
extern void draw_tables_reset(void);   /* draw_ep.asm */
extern void ep_snd_reinit(void);       /* sound_ep.c */

static uint8_t ep_saves_ok;

static void ep_saves_init(void)
{
    dance_loader_seg = EP_HANDOFF[10];
    ep_saves_ok = EP_HANDOFF[11];
}

static uint8_t ep_dance(uint8_t op, uint16_t buf, uint16_t len)
{
    if (!ep_saves_ok) {
        return PLAT_ERR_UNSUPPORTED;
    }
    /* page 0 is remapped during the dance and page 3 belongs to EXOS: the
     * blob must sit in pages 1-2 (it does: SAVBUF is core BSS) */
    if (buf < 0x4000u || (uint16_t)(buf + len) > 0xC000u || buf + len < buf) {
        return PLAT_ERR_IO;
    }
    plat_sound_stop();
    dance_op = op;
    dance_buf = buf;
    dance_len = len;
    __asm di __endasm;
    dance_run();                       /* returns with DI held, P0 = game */

    /* rebuild everything the EXOS restore overwrote in the video segment */
    ep_build_lpt();
    ep_nick_lpt();
    draw_tables_reset();
    /* The rebuilt LPT shows FB0, so canonicalize the flip parity: FB1 (the
     * buffer the stash landed in, whatever was hidden at entry) becomes the
     * draw target and is cleared.  Without this, a dance entered with
     * draw_base == FB0 would leave the game drawing into the visible buffer
     * and publish EXOS garbage on the next present. */
    draw_base = FB1_BASE;
    ep_rebuild_row_addr();
    plat_clear();
    ep_snd_reinit();
    __asm ei __endasm;

    return (dance_status == 0) ? PLAT_OK : PLAT_ERR_IO;
}
