; snd_scale.asm - Covox volume scaling: copy a sample chunk through a
; 256-byte volume lookup table.
;
; The Covox DAC has no level control, so creature-distance volume and
; the fade buzz ramp are done by scaling the PCM bytes on the CPU:
; sound_next.c builds the table for a volume tier and streams the
; sample through this routine into the bounce pages (see the "Covox
; volume tiers" block there).  Verified byte-identical to the C
; reference by tests/z80scale/run.sh (z88dk-ticks emulation).
;
; C prototype (see spectrum-next/sound_next.c):
;   void snd_scale_chunk(const uint8_t *src, uint8_t *dst, uint16_t len);
;
; The table lives at the fixed page SND_LUT_PG in the free tail of the
; bank-7 draw page - the same always-mapped scratch region that holds
; the rasterizer's tables at 0x5800-0x5AFF (see draw_z80n.asm; 0x5C00+
; stays free).
;
; Calling convention: standard zsdcc/sdcc stack convention (sdcccall(0)),
; caller pops the arguments:
;   sp+0..1  return address
;   sp+2..3  src   (points into the MMU1 sample window)
;   sp+4..5  dst   (points into the MMU0 bounce window)
;   sp+6..7  len   (bytes; 0 = no-op)
; Register usage: AF, BC, DE, HL and the SHADOW DE (via exx).  The
; shadow set is safe here: sdcc's Z80 codegen never uses the alternate
; registers and isr.asm does not exx, so an IM2 interrupt may fire at
; any point.  IX and IY are never touched.  Not re-entrant (main
; thread only, like the rasterizer).
;
; Inner loop is 69 T-states/byte; the worst case (the 22050-byte buzz,
; re-scaled only when the fade ramp crosses a volume tier, i.e. every
; other 300 ms buzz step) is ~54 ms at 28 MHz.

SECTION code_user

PUBLIC _snd_scale_chunk

defc SND_LUT_PG = 0x5B              ; keep in sync with SND_LUT (sound_next.c)

_snd_scale_chunk:
    ld   hl, 2
    add  hl, sp
    ld   e, (hl)
    inc  hl
    ld   d, (hl)                    ; DE = src
    inc  hl
    ld   c, (hl)
    inc  hl
    ld   b, (hl)                    ; BC = dst (parked in DE' below)
    inc  hl
    ld   a, (hl)
    inc  hl
    ld   h, (hl)
    ld   l, a                       ; HL = len
    ld   a, h
    or   l
    ret  z
    push bc
    exx
    pop  de                         ; DE' = dst
    exx
    ld   b, h
    ld   c, l                       ; BC = len
    ld   h, SND_LUT_PG              ; HL = table entry (H fixed, L = sample)
scl_loop:
    ld   a, (de)                    ; 7   sample byte
    ld   l, a                       ; 4
    ld   a, (hl)                    ; 7   scaled through the table
    inc  de                         ; 6
    exx                             ; 4
    ld   (de), a                    ; 7   store to the bounce window
    inc  de                         ; 6
    exx                             ; 4
    dec  bc                         ; 6
    ld   a, b                       ; 4
    or   c                          ; 4
    jp   nz, scl_loop               ; 10
    ret
