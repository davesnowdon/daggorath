; draw_z80n.asm - Z80n line rasterizer for the Spectrum Next backend.
;
; Pixel-identical to core/draw_ref.c (the normative C port of the 6809
; VECTOR.ASM): 24.8 fixed-point DDA, plot-candidate-then-step, endpoint
; never plotted, dot-period fades via vctfad, inverse mode clears pixels.
; Verified bit-for-bit against draw_line_ref by tests/z80draw/run.sh
; (z88dk-ticks emulation vs the gcc-compiled reference).
;
; C prototype (see spectrum-next/plat_next.c):
;   void plat_draw_line_asm(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
;                           uint8_t vctfad, uint8_t flags);
;
; Calling convention: standard zsdcc/sdcc stack convention (sdcccall(0)),
; caller pops the arguments.  Verified against zsdcc 4.5.0 codegen for
; both "+zxn -clib=sdcc_iy" and "+test -clib=z80n":
;   sp+0..1  return address
;   sp+2..3  x0   (int16, little endian)
;   sp+4..5  y0
;   sp+6..7  x1
;   sp+8..9  y1
;   sp+10    vctfad (uint8_t args occupy ONE stack byte)
;   sp+11    flags  (bit 0 = PLAT_LINE_INVERSE: clear pixels)
; Register usage: AF, BC, DE, HL only.  IX and IY are never touched (the
; sdcc_iy CLIB owns IY; IX may be the caller's frame pointer).  Shadow
; registers are never used, so the routine is IM2-interrupt safe with no
; DI: the ISR may fire at any point.  NOT re-entrant (static scratch and
; self-modifying operands) - it is only ever called from the main thread.
;
; Exactness notes, mirroring draw_ref.c line for line:
;   period = vctfad+1; period==0 (vctfad 0xFF) -> draw nothing.
;   dx,dy signed 16-bit; length = max(|dx|,|dy|); length==0 -> nothing.
;   xinc = sign*(|dx|<<8)/length truncated toward zero (|q| <= 0x100).
;   xx = (x0<<8)|0x80 carried in 24-bit two's complement - the reference
;   uses int32 but its value provably stays within 24-bit signed range
;   for all int16 inputs, so 24-bit arithmetic is bit-identical.
;   The reference plots (candidate) at loop iterations i = j*period,
;   j = 1..floor(length/period), using xx = xx0 + (i-1)*xinc, and steps
;   xx += xinc every iteration.  Because two's-complement addition is
;   associative, this equals: start at xx0 + (period-1)*xinc and advance
;   by period*xinc per plot, floor(length/period) times.  This routine
;   uses that exact transformation: identical plotted positions, but the
;   inner loop runs once per CANDIDATE instead of once per step, so
;   faded lines (period > 1) cost period-times fewer iterations.
;   Plot gate: xi=(xx>>8), yi=(yy>>8) as int16; plot only when
;   (xi & 0xFF00)==0 and 0 <= yi < 192 (ymin/ymax_excl hard-coded 0/192:
;   the Next core always draws the full screen; draw_ref parameterises).
;   Inverse (flags bit0): pixel byte ANDed with ~mask instead of ORed.
;
; ULA addressing: identical to plat_next.c's row_addr[]/bit_mask[]:
;   addr = 0x4000 | ((y&0xC0)<<5) | ((y&7)<<8) | ((y&0x38)<<2) | (x>>3)
;   mask = 0x80 >> (x&7)
; DEVIATION FROM THE OBVIOUS Z80N CODING: PIXELAD/SETAE would compute
; exactly this, but z88dk-ticks v0.14c does not emulate them (verified:
; they execute as NOPs), which would make emulation-based verification
; impossible.  The routine instead uses three page-aligned lookup tables
; built lazily on first call at FIXED addresses in the free tail of the
; draw-buffer 8K page (the ULA bitmap ends at 0x57FF; the visible
; attributes live in bank 5, written before plat_init's MMU2 remap, so
; 0x5800-0x5FFF of the bank-7 draw page is unused and always mapped
; whenever the rasterizer runs):
;     0x5800  mask[256]   0x80 >> (x&7)
;     0x5900  rowlo[192]  (y&0x38)<<2
;     0x5A00  rowhi[192]  0x40 | ((y&0xC0)>>3) | (y&7)
; (0x5B00+ stays free.)  Fixed pages avoid any linker ALIGN support and
; cost no main-RAM BSS.  Z80n opcodes used: MUL D,E (which ticks does
; emulate; verified).
;
; Self-modifying code: the six increment bytes, the y fraction, the two
; table page numbers and the 3-byte plot operation are patched per call.
; The code must therefore live in RAM (always true for .nex loads).

SECTION code_user

PUBLIC _plat_draw_line_asm

; lookup-table pages in the draw-buffer 8K page (see header)
defc TBL_MASK_PG  = 0x58
defc TBL_ROWLO_PG = 0x59            ; rowhi is the following page

; ---------------------------------------------------------------------
; entry / setup
; ---------------------------------------------------------------------
_plat_draw_line_asm:
    ; lazy one-time table build (bss is zero on startup in both the
    ; newlib .nex crt and under ticks)
    ld   a, (v_ready)
    or   a
    call z, build_tables

    ; copy the 10 argument bytes to static scratch
    ld   hl, 2
    add  hl, sp
    ld   de, v_x0
    ld   bc, 10
    ldir

    ; period = vctfad + 1 ; 0xFF -> invisible
    ld   a, (v_vctfad)
    inc  a
    ret  z
    ld   (v_period), a

    ; patch the plot operation: normal = OR (set), inverse = AND ~mask
    ld   hl, plot_norm_ops
    ld   a, (v_flags)
    and  0x01                   ; PLAT_LINE_INVERSE
    jr   z, dl_patch
    ld   hl, plot_inv_ops
dl_patch:
    ld   de, plot_op
    ldi
    ldi
    ldi

    ; dx = x1 - x0 -> sign v_sx, magnitude v_adx
    ld   hl, (v_x1)
    ld   de, (v_x0)
    or   a
    sbc  hl, de
    call abs16
    ld   (v_sx), a
    ld   (v_adx), hl

    ; dy = y1 - y0 -> sign v_sy, magnitude v_ady
    ld   hl, (v_y1)
    ld   de, (v_y0)
    or   a
    sbc  hl, de
    call abs16
    ld   (v_sy), a
    ld   (v_ady), hl

    ; length = max(adx, ady) ; 0 -> nothing to draw
    ld   hl, (v_ady)
    ld   de, (v_adx)
    or   a
    sbc  hl, de
    jr   nc, dl_len_ady
    ld   hl, (v_adx)
    jr   dl_len_set
dl_len_ady:
    ld   hl, (v_ady)
dl_len_set:
    ld   (v_len), hl
    ld   a, h
    or   l
    ret  z

    ; nplots = length / period ; 0 -> fade gate never fires
    ld   a, (v_period)
    ld   c, a
    call udiv16_8
    ld   (v_np), hl
    ld   a, h
    or   l
    ret  z

    ; qx = (adx<<8)/length, qy = (ady<<8)/length  (0..0x100, truncated)
    ld   hl, (v_adx)
    call qdiv
    ld   (v_qx), hl
    ld   hl, (v_ady)
    call qdiv
    ld   (v_qy), hl

    ; per-plot step = period*q, first-plot offset = step - q
    ld   hl, (v_qx)
    call pmul
    ld   (v_pxu), de
    ex   de, hl
    ld   de, (v_qx)
    or   a
    sbc  hl, de
    ld   (v_offx), hl

    ld   hl, (v_qy)
    call pmul
    ld   (v_pyu), de
    ex   de, hl
    ld   de, (v_qy)
    or   a
    sbc  hl, de
    ld   (v_offy), hl

    ; patch the six 24-bit increment bytes (sign restored)
    ld   a, (v_sx)
    ld   hl, (v_pxu)
    call signed24
    ld   a, e
    ld   (px0+1), a
    ld   a, d
    ld   (px1+1), a
    ld   a, c
    ld   (px2+1), a

    ld   a, (v_sy)
    ld   hl, (v_pyu)
    call signed24
    ld   a, e
    ld   (py0+1), a
    ld   a, d
    ld   (py1+1), a
    ld   a, c
    ld   (py2+1), a

    ; xx_start = ((x0<<8)|0x80) +/- offx   (24-bit)
    ld   a, (v_x0)
    ld   h, a                   ; low 16 bits of xx0 = (x0lo<<8)|0x80
    ld   l, 0x80
    ld   de, (v_offx)
    ld   a, (v_sx)
    or   a
    jr   nz, dl_xneg
    add  hl, de
    ld   a, (v_x0+1)
    adc  a, 0
    jr   dl_xstore
dl_xneg:
    or   a
    sbc  hl, de
    ld   a, (v_x0+1)
    sbc  a, 0
dl_xstore:
    ld   (v_xb2), a
    ld   a, l
    ld   (v_xb0), a
    ld   a, h
    ld   (v_xb1), a

    ; yy_start = ((y0<<8)|0x80) +/- offy   (24-bit)
    ld   a, (v_y0)
    ld   h, a
    ld   l, 0x80
    ld   de, (v_offy)
    ld   a, (v_sy)
    or   a
    jr   nz, dl_yneg
    add  hl, de
    ld   a, (v_y0+1)
    adc  a, 0
    jr   dl_ystore
dl_yneg:
    or   a
    sbc  hl, de
    ld   a, (v_y0+1)
    sbc  a, 0
dl_ystore:
    ld   (v_yb2), a
    ld   a, l
    ld   (yf_ld+1), a           ; y fraction lives as an SMC immediate
    ld   a, h
    ld   (v_yb1), a

    ; loop registers:
    ;   C = x fraction        E = x integer low     L = x integer high
    ;   (SMC) y fraction      D = y integer low     H = y integer high
    ;   B = inner counter, (v_pages) = outer page counter
    ld   a, (v_xb0)
    ld   c, a
    ld   a, (v_xb1)
    ld   e, a
    ld   a, (v_yb1)
    ld   d, a
    ; pages = np_hi + (np_lo != 0), B = np_lo (0 means 256 to djnz)
    ld   a, (v_np)
    ld   b, a
    or   a
    ld   a, (v_np+1)
    jr   z, dl_nolow
    inc  a
dl_nolow:
    ld   (v_pages), a
    ld   a, (v_xb2)
    ld   l, a
    ld   a, (v_yb2)
    ld   h, a

; ---------------------------------------------------------------------
; inner loop: one iteration per plot CANDIDATE
; ---------------------------------------------------------------------
dl_loop:
    ; gate: x high byte == 0 (0 <= xi <= 255) and y high == 0, yi < 192
    ld   a, l
    or   h
    jr   nz, dl_noplot
    ld   a, d
    cp   192
    jr   nc, dl_noplot
    ; plot pixel (E, D); H and L are both known to be 0 here
    ld   h, TBL_MASK_PG
    ld   l, e
    ld   a, (hl)
    ld   (msk_ld+1), a          ; SMC: stash mask
    ld   a, e
    rrca
    rrca
    rrca
    and  0x1F                   ; x>>3
    ld   h, TBL_ROWLO_PG
    ld   l, d
    add  a, (hl)                ; + row low byte (32-aligned: no carry)
    inc  h                      ; rowhi page (built adjacent)
    ld   h, (hl)
    ld   l, a                   ; HL = pixel byte address
msk_ld:
    ld   a, 0                   ; SMC: mask
plot_op:
    or   (hl)                   ; SMC 3 bytes: B6 77 00 (set) or
    ld   (hl), a                ;              2F A6 77 (clear)
    nop
    ld   hl, 0                  ; restore x/y integer-high (were 0)
dl_noplot:
    ; xx += pxinc (24-bit)
    ld   a, c
px0:
    add  a, 0                   ; SMC
    ld   c, a
    ld   a, e
px1:
    adc  a, 0                   ; SMC
    ld   e, a
    ld   a, l
px2:
    adc  a, 0                   ; SMC
    ld   l, a
    ; yy += pyinc (24-bit)
yf_ld:
    ld   a, 0                   ; SMC: y fraction
py0:
    add  a, 0                   ; SMC
    ld   (yf_ld+1), a
    ld   a, d
py1:
    adc  a, 0                   ; SMC
    ld   d, a
    ld   a, h
py2:
    adc  a, 0                   ; SMC
    ld   h, a
    djnz dl_loop
    ld   a, (v_pages)
    dec  a
    ld   (v_pages), a
    jp   nz, dl_loop
    ret

; ---------------------------------------------------------------------
; helpers
; ---------------------------------------------------------------------

; HL = |HL| ; A = 1 if HL was negative else 0
abs16:
    bit  7, h
    jr   z, abs16_pos
    xor  a
    sub  l
    ld   l, a
    sbc  a, a
    sub  h
    ld   h, a
    ld   a, 1
    ret
abs16_pos:
    xor  a
    ret

; HL = HL / C (C >= 1), remainder in A ; destroys B
udiv16_8:
    xor  a
    ld   b, 16
ud_lp:
    add  hl, hl
    rla
    jr   c, ud_sub
    cp   c
    jr   c, ud_next
ud_sub:
    sub  c
    inc  l
ud_next:
    djnz ud_lp
    ret

; HL = (HL << 8) / (v_len), with HL <= v_len ; result in 0..0x100.
; Truncated (floor) division - matches draw_ref's toward-zero trunc on
; the non-negative magnitude.  Destroys A, B, C, DE.
qdiv:
    ld   de, (v_len)
    or   a
    sbc  hl, de
    ld   a, h
    or   l
    jr   nz, qd_go
    ld   hl, 0x0100             ; |delta| == length -> q = 1.0
    ret
qd_go:
    add  hl, de                 ; restore magnitude (remainder seed)
    ld   b, 8
    ld   c, 0
qd_lp:
    add  hl, hl
    jr   c, qd_one_ovf          ; bit16 set: R >= 65536 > length
    or   a
    sbc  hl, de
    jr   nc, qd_one
    add  hl, de                 ; R < length: restore, emit 0
    or   a
    rl   c
    djnz qd_lp
    jr   qd_done
qd_one_ovf:
    or   a
    sbc  hl, de                 ; mod-2^16 subtract is exact here
qd_one:
    scf
    rl   c
    djnz qd_lp
qd_done:
    ld   l, c
    ld   h, 0
    ret

; DE = period * HL, HL in 0..0x100 (uses Z80n MUL D,E) ; destroys A
pmul:
    ld   a, (v_period)
    ld   d, a
    ld   a, h
    or   a
    jr   z, pm_small
    ld   e, 0                   ; q == 0x100 -> period << 8
    ret
pm_small:
    ld   e, l
    mul  d, e
    ret

; E:D:C = 24-bit two's complement of (A ? -HL : +HL)   (b0,b1,b2)
signed24:
    or   a
    jr   nz, s24_neg
    ld   e, l
    ld   d, h
    ld   c, 0
    ret
s24_neg:
    xor  a
    sub  l
    ld   e, a
    ld   a, 0
    sbc  a, h
    ld   d, a
    ld   a, 0
    sbc  a, 0
    ld   c, a
    ret

; ---------------------------------------------------------------------
; one-time table build at the fixed pages (see header):
;   TBL_MASK_PG :  mask[x]  = 0x80 >> (x&7)               (256 bytes)
;   TBL_ROWLO_PG:  rowlo[y] = (y&0x38)<<2                 (192 used)
;   +1          :  rowhi[y] = 0x40 | ((y&0xC0)>>3) | (y&7)(192 used)
; Runs on the first draw call, i.e. after plat_init has mapped the
; draw-buffer page at 0x4000 (under ticks it is ordinary RAM).
; ---------------------------------------------------------------------
build_tables:
    ld   h, TBL_MASK_PG
    ld   l, 0
    ld   b, 0                   ; 256 iterations
    ld   a, 0x80
bt_mask:
    ld   (hl), a
    inc  l
    rrca
    djnz bt_mask
    inc  h                      ; rowlo page
    ld   d, 0                   ; y
bt_rows:
    ld   a, d
    and  0x38
    add  a, a
    add  a, a
    ld   l, d
    ld   (hl), a                ; rowlo[y]
    inc  h
    ld   a, d
    rrca
    rrca
    rrca
    and  0x18
    or   0x40
    ld   e, a
    ld   a, d
    and  7
    or   e
    ld   (hl), a                ; rowhi[y]
    dec  h
    inc  d
    ld   a, d
    cp   192
    jr   nz, bt_rows
    ld   a, 1
    ld   (v_ready), a
    ret

; plot operation templates (copied over plot_op per call)
plot_norm_ops:
    defb 0xB6, 0x77, 0x00       ; or (hl) / ld (hl),a / nop
plot_inv_ops:
    defb 0x2F, 0xA6, 0x77       ; cpl / and (hl) / ld (hl),a

; ---------------------------------------------------------------------
SECTION bss_user

; argument scratch - order and packing must match the LDIR above
v_x0:      defw 0
v_y0:      defw 0
v_x1:      defw 0
v_y1:      defw 0
v_vctfad:  defb 0
v_flags:   defb 0

v_period:  defb 0
v_sx:      defb 0
v_sy:      defb 0
v_adx:     defw 0
v_ady:     defw 0
v_len:     defw 0
v_np:      defw 0
v_qx:      defw 0
v_qy:      defw 0
v_pxu:     defw 0
v_pyu:     defw 0
v_offx:    defw 0
v_offy:    defw 0
v_xb0:     defb 0
v_xb1:     defb 0
v_xb2:     defb 0
v_yb1:     defb 0
v_yb2:     defb 0
v_pages:   defb 0
v_ready:   defb 0
