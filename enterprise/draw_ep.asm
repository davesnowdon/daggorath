; draw_ep.asm - Z80 line rasterizer for the Enterprise 64/128 backend.
;
; Derived from spectrum-next/draw_z80n.asm (the harness-proven Z80n
; rasterizer) with three deliberate differences:
;   1. LINEAR framebuffer addressing (32-byte stride) instead of the ULA
;      thirds interleave, with the base taken from the C backend's
;      _draw_base (the EP is double-buffered: the base flips between
;      0xC000 and 0xD800 on every plat_present).
;   2. NO Z80n opcodes: MUL D,E is replaced by a plain 8x8 shift-add
;      (pmul), so the routine runs on a stock Z80 and is verified under
;      z88dk-ticks -mz80 (tests/z80draw-ep).
;   3. Solid-line fast paths: period==1 skips the length/period division
;      and both step multiplies; axis-aligned solid lines (dy==0 /
;      dx==0) take dedicated horizontal/vertical fill paths; and solid
;      non-axis lines whose BOTH endpoints are on-screen take unclipped
;      octant loops (x-major / y-major / perfect diagonal, ~2.5-3x the
;      generic loop).  All are candidate-set IDENTITIES with the generic
;      DDA (see the proofs at hz_line / vt_line / sd_entry below),
;      proven by the tests/z80draw-ep corpus.  Why they matter at 4 MHz:
;      viewer_clear_screen on inverse levels paints 192 full-width solid
;      rows (3.4s generic -> ~38ms here), the 3D rooms are axis-heavy,
;      and creature shapes + receding wall edges are solid diagonals.
;
; Pixel-identical to core/draw_ref.c (the normative C port of the 6809
; VECTOR.ASM): 24.8 fixed-point DDA, plot-candidate-then-step, endpoint
; never plotted, dot-period fades via vctfad, inverse mode clears pixels.
; Verified bit-for-bit against draw_line_ref by tests/z80draw-ep/run.sh.
;
; C prototype (see enterprise/plat_ep.c):
;   void plat_draw_line_asm(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
;                           uint8_t vctfad, uint8_t flags);
;
; Calling convention: standard zsdcc/sdcc stack convention (sdcccall(0)),
; caller pops the arguments:
;   sp+0..1  return address
;   sp+2..3  x0   (int16, little endian)
;   sp+4..5  y0
;   sp+6..7  x1
;   sp+8..9  y1
;   sp+10    vctfad (uint8_t args occupy ONE stack byte)
;   sp+11    flags  (bit 0 = PLAT_LINE_INVERSE: clear pixels)
; Register usage: AF, BC, DE, HL only.  IX and IY are never touched.
; No shadow registers (they belong to the EP ISR by platform policy), so
; the routine is interrupt safe with no DI.  NOT re-entrant (static
; scratch and self-modifying operands) - main thread only; the ISR never
; draws.  The code must live in RAM (always true for the loader image).
;
; Addressing: pixel byte = draw_base + y*32 + (x>>3), mask = 0x80>>(x&7).
; Three 256-byte lookup pages at (ep_tbl_pg << 8), built lazily on the
; first call:
;     page+0  mask[256]   0x80 >> (x&7)
;     page+1  rowlo[192]  (y&7)<<5      (bits 5-7; ORs with x>>3, bits 0-4)
;     page+2  rowhi[192]  base_hi + (y>>3)
; _ep_tbl_pg defaults to 0xF0: on hardware the tables live at 0xF000-
; 0xF2FF in the video segment (free between FB1's end 0xEFFF and the LPT
; at 0xF800), always mapped whenever the rasterizer runs.  The identity-
; test wrapper repoints it below its org (0x58) BEFORE the first draw
; call; changing it after the first call is not supported (the page
; immediates are patched inside the one-time build).  rowhi bakes the
; framebuffer base high byte in (draw_base is 256-aligned, low byte 0)
; and is REBUILT whenever _draw_base's high byte differs from the cached
; v_basehi - i.e. once per buffer flip, ~192 writes, noise vs a frame's
; plot volume.

SECTION code_user

PUBLIC _plat_draw_line_asm
EXTERN _draw_base               ; plat_ep.c: uint8_t *draw_base (flips per present)

; ---------------------------------------------------------------------
; entry / setup
; ---------------------------------------------------------------------
_plat_draw_line_asm:
    ; lazy one-time table build (bss is zero on startup both under the
    ; EP crt and under ticks); also patches the table-page immediates
    ld   a, (v_ready)
    or   a
    call z, build_tables

    ; rebuild rowhi if the draw buffer flipped since the last call
    ld   a, (_draw_base+1)
    ld   hl, v_basehi
    cp   (hl)
    call nz, build_rowhi

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

    ; select the plot template (normal = OR set, inverse = AND ~mask),
    ; remember it for the fast paths, patch the generic plot site, and
    ; set the solid-fill byte the horizontal path uses for whole bytes
    ld   a, (v_flags)
    and  0x01                   ; PLAT_LINE_INVERSE
    ld   hl, plot_norm_ops
    ld   e, 0xFF
    jr   z, dl_pat
    ld   hl, plot_inv_ops
    ld   e, 0x00
dl_pat:
    ld   a, e
    ld   (v_fillb), a
    ld   (v_plt), hl
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

    ; ---- dispatch: solid lines get the fast setups/paths -------------
    ld   a, (v_period)
    dec  a
    jr   nz, dl_generic
    ld   hl, (v_ady)
    ld   a, h
    or   l
    jp   z, hz_line             ; solid horizontal
    ld   hl, (v_adx)
    ld   a, h
    or   l
    jp   z, vt_line             ; solid vertical

    ; solid non-axis: if BOTH endpoints are on-screen, every candidate
    ; provably is too (see the octant fast-path header below), so the
    ; generic loop's clip gates can never fire -> unclipped octant
    ; loops.  Any off-screen endpoint falls through to the generic DDA.
    ld   a, (v_x0+1)            ; x in [0,255] <=> int16 high byte 0
    or   a
    jr   nz, sd_generic
    ld   a, (v_x1+1)
    or   a
    jr   nz, sd_generic
    ld   a, (v_y0+1)            ; y in [0,191]: high 0 and low < 192
    or   a
    jr   nz, sd_generic
    ld   a, (v_y1+1)
    or   a
    jr   nz, sd_generic
    ld   a, (v_y0)
    cp   192
    jr   nc, sd_generic
    ld   a, (v_y1)
    cp   192
    jp   c, sd_entry

sd_generic:
    ; solid diagonal: nplots = length (no division), step = q (period*q
    ; needs no multiply), first-plot offset = step - q = 0
    ld   hl, (v_len)
    ld   (v_np), hl
    ld   hl, (v_adx)
    call qdiv
    ld   (v_pxu), hl
    ld   hl, (v_ady)
    call qdiv
    ld   (v_pyu), hl
    ld   hl, 0
    ld   (v_offx), hl
    ld   (v_offy), hl
    jr   dl_incs

dl_generic:
    ; nplots = length / period ; 0 -> fade gate never fires
    ld   a, (v_period)
    ld   c, a
    ld   hl, (v_len)
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

dl_incs:
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
pl_mskpg:
    ld   h, 0                   ; SMC: mask table page (build_tables)
    ld   l, e
    ld   a, (hl)
    ld   (msk_ld+1), a          ; SMC: stash mask
    ld   a, e
    rrca
    rrca
    rrca
    and  0x1F                   ; x>>3 (bits 0-4)
pl_rowpg:
    ld   h, 0                   ; SMC: rowlo page (build_tables)
    ld   l, d
    add  a, (hl)                ; + (y&7)<<5 (bits 5-7: disjoint, no carry)
    inc  h                      ; rowhi page (built adjacent)
    ld   h, (hl)                ; base_hi + (y>>3)
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
; horizontal solid line (period==1, dy==0, len=adx!=0)
;
; Candidate-set identity: len==adx -> qdiv(adx) returns exactly 0x100,
; so xx steps by a whole pixel with the fraction pinned at 0x80: the
; candidates are exactly the integers x0, x0+-1, ..., x0+-(len-1), i.e.
; the inclusive span [x0, x1-1] (sx=+) or [x1+1, x0] (sx=-).  qy =
; (0<<8)/len = 0, so yi == y0 for every candidate.  The generic per-
; candidate gates therefore reduce to: nothing unless 0 <= y0 < 192,
; and clip the span to x in [0, 255].  The fill below reproduces that
; exact pixel set with two masked RMWs and a solid middle run.
; ---------------------------------------------------------------------
hz_line:
    ; y gate (int16): high byte 0 and low < 192
    ld   a, (v_y0+1)
    or   a
    ret  nz
    ld   a, (v_y0)
    cp   192
    ret  nc

    ; inclusive span [lo, hi] (int16)
    ld   a, (v_sx)
    or   a
    jr   nz, hz_neg
    ld   hl, (v_x0)             ; lo = x0
    ld   de, (v_x1)
    dec  de                     ; hi = x1 - 1
    jr   hz_clip
hz_neg:
    ld   hl, (v_x1)
    inc  hl                     ; lo = x1 + 1
    ld   de, (v_x0)             ; hi = x0
hz_clip:
    ld   (v_lo16), hl
    ld   (v_hi16), de
    ; empty if hi < lo (signed; magnitudes are core-bounded, no overflow)
    ex   de, hl
    ld   de, (v_lo16)
    or   a
    sbc  hl, de
    ret  m
    ; clip lo to >= 0; lo >= 256 -> empty
    ld   hl, (v_lo16)
    bit  7, h
    jr   z, hz_lo_pos
    ld   hl, 0
hz_lo_pos:
    ld   a, h
    or   a
    ret  nz
    ld   a, l
    ld   (v_clo), a
    ; clip hi to <= 255; hi < 0 -> empty
    ld   hl, (v_hi16)
    bit  7, h
    ret  nz
    ld   a, h
    or   a
    jr   z, hz_hi_ok
    ld   l, 0xFF
hz_hi_ok:
    ld   a, (v_clo)
    ld   b, a
    ld   a, l
    ld   (v_chi), a
    cp   b
    ret  c                      ; clipped empty

    ; patch the fast-path plot site with this call's template
    ld   hl, (v_plt)
    ld   de, plot_op2
    ldi
    ldi
    ldi

    ; left mask -> E: 0xFF >> (lo&7)  (pixels lo&7 .. 7 of the byte)
    ld   a, (v_clo)
    and  7
    ld   e, 0xFF
    jr   z, hz_lm_done
hz_lm_sh:
    srl  e
    dec  a
    jr   nz, hz_lm_sh
hz_lm_done:
    ; right mask -> D: top (hi&7)+1 bits (pixels 0 .. hi&7)
    ld   a, (v_chi)
    and  7
    ld   d, 0x80
    jr   z, hz_rm_done
hz_rm_sh:
    sra  d                      ; sign-extends: 0x80 -> C0 -> E0 -> ...
    dec  a
    jr   nz, hz_rm_sh
hz_rm_done:
    ; byte indices: C = lb, B = hb - lb
    ld   a, (v_clo)
    rrca
    rrca
    rrca
    and  0x1F
    ld   c, a
    ld   a, (v_chi)
    rrca
    rrca
    rrca
    and  0x1F
    sub  c
    ld   b, a
    ; HL = byte address of (lb, y0): h = rowhi[y], l = rowlo[y] | lb
    ld   a, (v_y0)
    push de
    ld   e, a
hz_rowpg:
    ld   h, 0                   ; SMC: rowlo page (build_tables)
    ld   l, e
    ld   a, (hl)
    or   c                      ; | lb (disjoint bits)
    inc  h
    ld   h, (hl)                ; rowhi[y] (base baked in)
    ld   l, a
    pop  de

    ld   a, b
    or   a
    jr   nz, hz_multi
    ; single byte: mask = lmask & rmask
    ld   a, e
    and  d
    jp   apply_mask
hz_multi:
    ld   a, e
    call apply_mask             ; left partial (preserves HL, DE, B)
    inc  l                      ; rows are 32-byte page-local: no carry
    dec  b                      ; middle bytes = hb - lb - 1
    jr   z, hz_right
    ld   a, (v_fillb)
hz_mid:
    ld   (hl), a                ; solid middle: plain store, no RMW
    inc  l
    djnz hz_mid
hz_right:
    ld   a, d
    jp   apply_mask             ; right partial (tail call)

; ---------------------------------------------------------------------
; vertical solid line (period==1, dx==0, len=ady!=0)
;
; Candidate-set identity, mirroring hz_line: qdiv(ady) == 0x100 exactly
; so yi walks the integers [y0, y1-1] (sy=+) or [y1+1, y0] (sy=-), and
; qx == 0 pins xi == x0.  Gates reduce to: nothing unless 0 <= x0 <= 255,
; clip the row span to [0, 191].  Same mask ORed/cleared down the column,
; address += 32 per row.
; ---------------------------------------------------------------------
vt_line:
    ; x gate (int16): high byte must be 0
    ld   a, (v_x0+1)
    or   a
    ret  nz

    ; inclusive row span [lo, hi] (int16)
    ld   a, (v_sy)
    or   a
    jr   nz, vt_neg
    ld   hl, (v_y0)             ; lo = y0
    ld   de, (v_y1)
    dec  de                     ; hi = y1 - 1
    jr   vt_clip
vt_neg:
    ld   hl, (v_y1)
    inc  hl                     ; lo = y1 + 1
    ld   de, (v_y0)             ; hi = y0
vt_clip:
    ld   (v_lo16), hl
    ld   (v_hi16), de
    ; empty if hi < lo (signed)
    ex   de, hl
    ld   de, (v_lo16)
    or   a
    sbc  hl, de
    ret  m
    ; clip lo to >= 0; lo >= 192 -> empty
    ld   hl, (v_lo16)
    bit  7, h
    jr   z, vt_lo_pos
    ld   hl, 0
vt_lo_pos:
    ld   a, h
    or   a
    ret  nz
    ld   a, l
    cp   192
    ret  nc
    ld   (v_clo), a
    ; clip hi to <= 191; hi < 0 -> empty
    ld   hl, (v_hi16)
    bit  7, h
    ret  nz
    ld   a, h
    or   a
    jr   nz, vt_hi_clamp
    ld   a, l
    cp   192
    jr   c, vt_hi_ok
vt_hi_clamp:
    ld   l, 191
vt_hi_ok:
    ld   a, (v_clo)
    ld   b, a
    ld   a, l
    ld   (v_chi), a
    cp   b
    ret  c                      ; clipped empty

    ; patch the fast-path plot site with this call's template
    ld   hl, (v_plt)
    ld   de, plot_op3
    ldi
    ldi
    ldi

    ; B = row count = hi - lo + 1 (1..192)
    ld   a, (v_clo)
    ld   c, a
    ld   a, (v_chi)
    sub  c
    inc  a
    ld   b, a
    ; C = pixel mask
vt_mskpg:
    ld   h, 0                   ; SMC: mask page (build_tables)
    ld   a, (v_x0)
    ld   l, a
    ld   c, (hl)
    ; HL = byte address of (x0, lo)
    ld   a, (v_clo)
    ld   e, a
vt_rowpg:
    ld   h, 0                   ; SMC: rowlo page (build_tables)
    ld   l, e
    ld   a, (v_x0)
    rrca
    rrca
    rrca
    and  0x1F
    add  a, (hl)                ; x>>3 + rowlo (disjoint bits)
    inc  h
    ld   h, (hl)                ; rowhi[lo]
    ld   l, a
vt_loop:
    ld   a, c
plot_op3:
    or   (hl)                   ; SMC 3 bytes (same templates as plot_op)
    ld   (hl), a
    nop
    ld   a, l
    add  a, 32                  ; next row (linear stride)
    ld   l, a
    jr   nc, vt_nc
    inc  h
vt_nc:
    djnz vt_loop
    ret

; masked RMW at HL with this call's plot semantics (A = mask).
; Preserves HL, DE, BC.
apply_mask:
plot_op2:
    or   (hl)                   ; SMC 3 bytes (same templates as plot_op)
    ld   (hl), a
    nop
    ret

; ---------------------------------------------------------------------
; solid non-axis octant fast paths (period==1, adx!=0, ady!=0, and all
; of x0,x1 in [0,255], y0,y1 in [0,191] - checked at dispatch)
;
; Candidate-set identity: period==1 plots EVERY candidate k = 0..len-1
; at xx_k = xx0 + k*xinc (plot before step, endpoint never reached).
; |xinc| = floor(adx*256/len) so |k*xinc| <= (len-1)*adx*256/len
; < adx*256: every candidate's integer part lies inside the endpoint
; bounding box [min(x0,x1),max(x0,x1)] x [min(y0,y1),max(y0,y1)].  With
; both endpoints on-screen the generic gates can never fire, so these
; unclipped loops plot the identical pixel set:
;   x-major (adx > ady): len = adx -> xinc = +-0x100 EXACTLY (qdiv of
;     |delta|==len), so xi walks x0, x0+-1, ... with its fraction pinned
;     at 0x80 - modelled by the mask rotate + byte step.  yi advances by
;     qy = floor(ady*256/adx) in [1,254]: an 8-bit fraction accumulator
;     seeded 0x80 whose carry (add) / borrow (sub) is exactly the
;     reference's 24.8 integer-byte increment; the linear framebuffer
;     makes that integer step "address +- 32" (16-bit, carry into H).
;     The high bytes of the reference's 24-bit xx/yy can never leave 0
;     inside the bounding box, so dropping them loses nothing.
;   y-major (ady > adx): the mirror image - row step every iteration,
;     mask rotate on the x fraction's carry/borrow.
;   diagonal (adx == ady): xinc AND yinc are +-0x100 exactly; both
;     fractions stay pinned - a pure mask-rotate + row-step walk.
; The post-final loop iteration steps registers once past the last
; candidate (mask/HL may go stale) but never dereferences them.
; Faded lines (period > 1) and any-endpoint-off-screen lines keep the
; generic DDA.  Proven pixel-identical by the tests/z80draw-ep corpus.
;
; Direction handling is single-byte SMC on each loop's ops:
;   x step:  rrca(0F)+inc hl(23) for +x   / rlca(07)+dec hl(2B) for -x
;   y frac:  add a,d(82) for +y           / sub d(92) for -y
;   y row:   add a,n(C6)+inc h(24) for +y / sub n(D6)+dec h(25) for -y
; (add sets carry on overflow -> inc; sub sets carry on borrow -> dec,
; so one "jr nc, skip / patched-op" skeleton serves both directions.)
; ---------------------------------------------------------------------
sd_entry:
    ; octant dispatch (spans fit in one byte here: adx<=255, ady<=191)
    ld   a, (v_ady)
    ld   b, a
    ld   a, (v_adx)
    cp   b
    jp   z, sd_diag
    jp   c, sd_ymajor

; ----- x-major: adx > ady >= 1 ---------------------------------------
sd_xmajor:
    ld   hl, (v_ady)            ; qy = (ady<<8)/adx in [1,254]
    call qdiv                   ; (v_len == adx; destroys A,B,C,DE)
    push hl                     ; save qy (L)
    ld   hl, (v_plt)            ; this call's plot template
    ld   de, plot_opx
    ldi
    ldi
    ldi
    ld   a, (v_sx)
    or   a
    jr   nz, sdx_xneg
    ld   a, 0x0F                ; rrca
    ld   (sdx_mrot), a
    ld   a, 0x23                ; inc hl
    ld   (sdx_bstep), a
    jr   sdx_ypat
sdx_xneg:
    ld   a, 0x07                ; rlca
    ld   (sdx_mrot), a
    ld   a, 0x2B                ; dec hl
    ld   (sdx_bstep), a
sdx_ypat:
    ld   a, (v_sy)
    or   a
    jr   nz, sdx_yneg
    ld   a, 0x82                ; add a,d
    ld   (sdx_fop), a
    ld   a, 0xC6                ; add a,n
    ld   (sdx_rop1), a
    ld   a, 0x24                ; inc h
    ld   (sdx_rop2), a
    jr   sdx_go
sdx_yneg:
    ld   a, 0x92                ; sub d
    ld   (sdx_fop), a
    ld   a, 0xD6                ; sub n
    ld   (sdx_rop1), a
    ld   a, 0x25                ; dec h
    ld   (sdx_rop2), a
sdx_go:
    call sd_addr                ; C = mask[x0], HL = addr(x0,y0)
    pop  de
    ld   d, e                   ; D = qy
    ld   e, 0x80                ; E = y fraction (reference seed)
    ld   a, (v_adx)
    ld   b, a                   ; count = len = adx (2..255)
sdx_loop:
    ld   a, c
plot_opx:
    or   (hl)                   ; SMC 3 bytes (same templates as plot_op)
    ld   (hl), a
    nop
    ; y: fraction +- qy; on carry/borrow step one row
    ld   a, e
sdx_fop:
    add  a, d                   ; SMC 1 byte: 82 add a,d / 92 sub d
    ld   e, a
    jr   nc, sdx_nrow
    ld   a, l
sdx_rop1:
    add  a, 32                  ; SMC 1 byte: C6 add a,n / D6 sub n
    ld   l, a
    jr   nc, sdx_nrow
sdx_rop2:
    inc  h                      ; SMC 1 byte: 24 inc h / 25 dec h
sdx_nrow:
    ; x: rotate mask; on wrap step one byte
    ld   a, c
sdx_mrot:
    rrca                        ; SMC 1 byte: 0F rrca / 07 rlca
    ld   c, a
    jr   nc, sdx_nbyte
sdx_bstep:
    inc  hl                     ; SMC 1 byte: 23 inc hl / 2B dec hl
sdx_nbyte:
    djnz sdx_loop
    ret

; ----- y-major: ady > adx >= 1 ---------------------------------------
sd_ymajor:
    ld   hl, (v_adx)            ; qx = (adx<<8)/ady in [1,254]
    call qdiv                   ; (v_len == ady; destroys A,B,C,DE)
    push hl                     ; save qx (L)
    ld   hl, (v_plt)
    ld   de, plot_opy
    ldi
    ldi
    ldi
    ld   a, (v_sx)
    or   a
    jr   nz, sdy_xneg
    ld   a, 0x82                ; add a,d
    ld   (sdy_fop), a
    ld   a, 0x0F                ; rrca
    ld   (sdy_mrot), a
    ld   a, 0x23                ; inc hl
    ld   (sdy_bstep), a
    jr   sdy_ypat
sdy_xneg:
    ld   a, 0x92                ; sub d
    ld   (sdy_fop), a
    ld   a, 0x07                ; rlca
    ld   (sdy_mrot), a
    ld   a, 0x2B                ; dec hl
    ld   (sdy_bstep), a
sdy_ypat:
    ld   a, (v_sy)
    or   a
    jr   nz, sdy_yneg
    ld   a, 0xC6                ; add a,n
    ld   (sdy_rop1), a
    ld   a, 0x24                ; inc h
    ld   (sdy_rop2), a
    jr   sdy_go
sdy_yneg:
    ld   a, 0xD6                ; sub n
    ld   (sdy_rop1), a
    ld   a, 0x25                ; dec h
    ld   (sdy_rop2), a
sdy_go:
    call sd_addr                ; C = mask[x0], HL = addr(x0,y0)
    pop  de
    ld   d, e                   ; D = qx
    ld   e, 0x80                ; E = x fraction (reference seed)
    ld   a, (v_ady)
    ld   b, a                   ; count = len = ady (2..191)
sdy_loop:
    ld   a, c
plot_opy:
    or   (hl)                   ; SMC 3 bytes (same templates as plot_op)
    ld   (hl), a
    nop
    ; x: fraction +- qx; on carry/borrow rotate mask (maybe step byte)
    ld   a, e
sdy_fop:
    add  a, d                   ; SMC 1 byte: 82 add a,d / 92 sub d
    ld   e, a
    jr   nc, sdy_nx
    ld   a, c
sdy_mrot:
    rrca                        ; SMC 1 byte: 0F rrca / 07 rlca
    ld   c, a
    jr   nc, sdy_nx
sdy_bstep:
    inc  hl                     ; SMC 1 byte: 23 inc hl / 2B dec hl
sdy_nx:
    ; y: one row every iteration
    ld   a, l
sdy_rop1:
    add  a, 32                  ; SMC 1 byte: C6 add a,n / D6 sub n
    ld   l, a
    jr   nc, sdy_ny
sdy_rop2:
    inc  h                      ; SMC 1 byte: 24 inc h / 25 dec h
sdy_ny:
    djnz sdy_loop
    ret

; ----- perfect diagonal: adx == ady ----------------------------------
sd_diag:
    ld   hl, (v_plt)
    ld   de, plot_opd
    ldi
    ldi
    ldi
    ld   a, (v_sx)
    or   a
    jr   nz, sdd_xneg
    ld   a, 0x0F                ; rrca
    ld   (sdd_mrot), a
    ld   a, 0x23                ; inc hl
    ld   (sdd_bstep), a
    jr   sdd_ypat
sdd_xneg:
    ld   a, 0x07                ; rlca
    ld   (sdd_mrot), a
    ld   a, 0x2B                ; dec hl
    ld   (sdd_bstep), a
sdd_ypat:
    call sd_addr                ; C = mask[x0], HL = addr(x0,y0)
    ld   de, 32                 ; +y row stride
    ld   a, (v_sy)
    or   a
    jr   z, sdd_go
    ld   de, 0xFFE0             ; -y row stride (-32)
sdd_go:
    ld   a, (v_adx)
    ld   b, a                   ; count = len (1..191)
sdd_loop:
    ld   a, c
plot_opd:
    or   (hl)                   ; SMC 3 bytes (same templates as plot_op)
    ld   (hl), a
    nop
    add  hl, de                 ; y: one row every iteration
    ld   a, c
sdd_mrot:
    rrca                        ; SMC 1 byte: 0F rrca / 07 rlca
    ld   c, a
    jr   nc, sdd_nb
sdd_bstep:
    inc  hl                     ; SMC 1 byte: 23 inc hl / 2B dec hl
sdd_nb:
    djnz sdd_loop
    ret

; C = mask[x0], HL = framebuffer byte address of (x0, y0).
; In-bounds x0/y0 only (fast-path dispatch guarantees).  Uses A, E.
sd_addr:
sd_mskpg:
    ld   h, 0                   ; SMC: mask page (build_tables)
    ld   a, (v_x0)
    ld   l, a
    ld   c, (hl)                ; C = 0x80 >> (x0&7)
    rrca
    rrca
    rrca
    and  0x1F                   ; x0>>3
    ld   e, a
sd_rowpg:
    ld   h, 0                   ; SMC: rowlo page (build_tables)
    ld   a, (v_y0)
    ld   l, a
    ld   a, (hl)                ; (y0&7)<<5
    or   e                      ; | x0>>3 (disjoint bits)
    inc  h
    ld   h, (hl)                ; rowhi[y0] (base baked in)
    ld   l, a
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

; DE = period * HL, HL in 0..0x100 ; destroys A, B, HL.
; Plain-Z80 8x8 shift-add (the Next used Z80n MUL D,E; the EP has none).
; Only runs for faded lines (period > 1), twice per call - the extra
; ~200 T-states are noise there.
pmul:
    ld   a, (v_period)
    ld   d, a
    ld   a, h
    or   a
    jr   z, pm_small
    ld   e, 0                   ; q == 0x100 -> period << 8
    ret
pm_small:
    ld   a, d                   ; A = multiplier (period)
    ld   e, l                   ; DE = multiplicand (q low)
    ld   d, 0
    ld   hl, 0
    ld   b, 8
pm_lp:
    add  hl, hl
    add  a, a
    jr   nc, pm_no
    add  hl, de
pm_no:
    djnz pm_lp
    ex   de, hl                 ; result -> DE
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
; one-time table build at (ep_tbl_pg << 8) (see header):
;   +0  mask[x]  = 0x80 >> (x&7)                (256 bytes)
;   +1  rowlo[y] = (y&7)<<5                     (192 used)
;   +2  rowhi[y] = draw_base_hi + (y>>3)        (built by build_rowhi)
; Also patches every table-page immediate in the code (the page is fixed
; from the first call on).  Runs on the first draw call, i.e. after
; plat_init has mapped the video segment (under ticks it is plain RAM).
; ---------------------------------------------------------------------
build_tables:
    ld   a, (_ep_tbl_pg)
    ld   (pl_mskpg+1), a
    ld   (vt_mskpg+1), a
    ld   (sd_mskpg+1), a
    inc  a
    ld   (pl_rowpg+1), a
    ld   (hz_rowpg+1), a
    ld   (vt_rowpg+1), a
    ld   (sd_rowpg+1), a
    dec  a
    ld   h, a                   ; mask page
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
    and  7
    rrca                        ; (y&7)<<5 == three ROR of a 3-bit value
    rrca
    rrca
    ld   l, d
    ld   (hl), a                ; rowlo[y]
    inc  d
    ld   a, d
    cp   192
    jr   nz, bt_rows
    ld   a, 1
    ld   (v_ready), a
    ret

; Invalidate the lookup tables (Phase 5): the save dance's EXOS-state
; restore overwrites the video segment above 0xD800, which includes the
; table pages at 0xF000-0xF2FF.  Zeroing both flags makes the next draw
; call rebuild everything (v_basehi 0 never matches a real 0xC0/0xD8
; base, so build_rowhi re-runs too).
PUBLIC _draw_tables_reset
_draw_tables_reset:
    xor  a
    ld   (v_ready), a
    ld   (v_basehi), a
    ret

; rebuild rowhi[y] = draw_base_hi + (y>>3) and cache the base high byte.
; Called whenever _draw_base's high byte != v_basehi (once per buffer
; flip; both EP framebuffers are 256-aligned so the low byte is 0).
build_rowhi:
    ld   a, (_ep_tbl_pg)
    add  a, 2
    ld   h, a                   ; rowhi page
    ld   a, (_draw_base+1)
    ld   (v_basehi), a
    ld   c, a                   ; C = base high byte
    ld   d, 0                   ; y
brh_lp:
    ld   a, d
    rrca
    rrca
    rrca
    and  0x1F                   ; y>>3 (y < 192 -> 0..23)
    add  a, c
    ld   l, d
    ld   (hl), a
    inc  d
    ld   a, d
    cp   192
    jr   nz, brh_lp
    ret

; plot operation templates (copied over the plot sites per call)
plot_norm_ops:
    defb 0xB6, 0x77, 0x00       ; or (hl) / ld (hl),a / nop
plot_inv_ops:
    defb 0x2F, 0xA6, 0x77       ; cpl / and (hl) / ld (hl),a

; ---------------------------------------------------------------------
SECTION data_user

PUBLIC _ep_tbl_pg

; high byte of the 3-page lookup block.  0xF0 = hardware home (video
; segment, between FB1 and the LPT).  The identity-test wrapper sets it
; below its org BEFORE the first draw call.
_ep_tbl_pg: defb 0xF0

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
v_basehi:  defb 0
v_plt:     defw 0               ; this call's plot template address
v_fillb:   defb 0               ; solid-fill byte (0xFF set / 0x00 inverse)
v_lo16:    defw 0               ; fast-path clip scratch
v_hi16:    defw 0
v_clo:     defb 0               ; clipped span, inclusive (8-bit)
v_chi:     defb 0
