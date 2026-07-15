; isr.asm - IM2 frame interrupt: the jiffy clock.
;
; The 50 Hz frame interrupt increments a 16-bit jiffy counter with a
; +1,+1,+1,+1,+2 phase pattern: exactly 60 jiffies per 50 frames, so
; the core's 60 Hz scheduler keeps CoCo-accurate wall-clock time.
; Counting in the ISR (not around HALT) means long rasterizer bursts
; can never lose frames.
;
; The ROM IM1 handler is unusable: it assumes IY points at the sysvars
; and the sdcc_iy CLIB owns IY, so every ROM ISR entry corrupted the
; top of RAM.  (Found the hard way - see docs/PLATFORM-NOTES-next.md.)

SECTION code_user

PUBLIC _frame_isr
PUBLIC _isr_jiffies
EXTERN _kbd_isr_scan
EXTERN _snd_isr_tick

_frame_isr:
    push af
    push hl
    ld   hl, (_isr_jiffies)
    inc  hl
    ld   a, (_isr_sixty)
    or   a
    jr   nz, store              ; 60 Hz display: 1 frame = 1 jiffy
    ld   a, (isr_phase)
    inc  a
    cp   5
    jr   nz, no_double
    inc  hl                     ; 5th frame counts double: 50 -> 60
    xor  a
no_double:
    ld   (isr_phase), a
store:
    ld   (_isr_jiffies), hl

    ; scan the keyboard matrix every frame, even while the game is
    ; CPU-bound (kbd_isr_scan is sdcc-compiled C: save its registers;
    ; IY belongs to the CLIB and is not touched by sdcc code), then
    ; feed the zxnDMA sample stream its next chunk if one is due.
    ;
    ; The SHADOW set is saved too: mainline asm may hold live values in
    ; the alternate registers (snd_scale.asm's Covox scaling loop), and
    ; although zsdcc 4.5.0 never emits exx/ex af,af' (verified against
    ; the generated code for both C ISR callees), that is a compiler-
    ; version invariant nobody enforces.  ~130 T-states/frame buys an
    ; ISR that cannot corrupt anyone's alternate registers, ever.
    push bc
    push de
    push ix
    exx
    ex   af, af'
    push af
    push bc
    push de
    push hl
    exx
    ex   af, af'
    call _kbd_isr_scan
    call _snd_isr_tick
    exx
    ex   af, af'
    pop  hl
    pop  de
    pop  bc
    pop  af
    exx
    ex   af, af'
    pop  ix
    pop  de
    pop  bc

    pop  hl
    pop  af
    ei
    reti

SECTION bss_user

PUBLIC _isr_sixty

_isr_jiffies:  defw 0
isr_phase:     defb 0
_isr_sixty:    defb 0
