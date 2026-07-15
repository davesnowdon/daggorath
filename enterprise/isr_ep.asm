; isr_ep.asm - Enterprise IM1 frame interrupt: the jiffy clock + keyboard.
;
; The Nick video interrupt (LPT IRQ record, Dave port B4h "int_1") fires once
; per PAL frame (50 Hz).  We keep a 16-bit jiffy counter with a
; +1,+1,+1,+1,+2 phase pattern: exactly 60 jiffies per 50 frames, so the
; core's 60 Hz scheduler keeps CoCo-accurate wall-clock time.  Counting in
; the ISR (not around HALT) means long rasterizer bursts can never lose
; frames.
;
; Entry is via a JP at 0038h (IM 1), patched in by ep_isr_start (plat_ep.c).
; EXOS is already dead - the loader killed it and paged its system segment
; out - so there is NO RST 30h stub to preserve (unlike a co-resident port).
;
; Register policy (plan): on this platform the ISR owns the shadow set, so
; mainline code never holds live alternate-register values.  The ISR
; therefore saves only the main + index registers the sdcc-compiled C
; keyboard scan may touch - no exx / ex af,af'.

SECTION code_user

PUBLIC _frame_isr
PUBLIC _isr_jiffies
EXTERN _kbd_isr_scan

_frame_isr:
    push af
    ; ack the Nick video interrupt: reset the int_1 latch, keep it enabled.
    ; (dave.cpp reg 14h decodes value^0x55; 0x30 -> enable_int_1=1, active=0.)
    ; Do it first so a long keyboard scan can never miss the next frame edge.
    ld   a, 0x30
    out  (0xb4), a

    push hl
    ld   hl, (_isr_jiffies)
    inc  hl                     ; +1 every frame
    ld   a, (isr_phase)
    inc  a
    cp   5
    jr   nz, isr_no_double
    inc  hl                     ; 5th frame counts double: 50 -> 60
    xor  a
isr_no_double:
    ld   (isr_phase), a
    ld   (_isr_jiffies), hl

    ; scan the keyboard matrix every frame, even while the game is CPU-bound
    ; (maze generation takes whole seconds), so no press is dropped.
    ; kbd_isr_scan is sdcc C: save every main + index register it may clobber.
    push bc
    push de
    push ix
    push iy
    call _kbd_isr_scan
    pop  iy
    pop  ix
    pop  de
    pop  bc

    pop  hl
    pop  af
    ei
    reti

SECTION bss_user

_isr_jiffies:  defw 0
isr_phase:     defb 0
