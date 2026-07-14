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

_frame_isr:
    push af
    push hl
    ld   hl, (_isr_jiffies)
    inc  hl
    ld   a, (isr_phase)
    inc  a
    cp   5
    jr   nz, no_double
    inc  hl                     ; 5th frame counts double: 50 -> 60
    xor  a
no_double:
    ld   (isr_phase), a
    ld   (_isr_jiffies), hl
    pop  hl
    pop  af
    ei
    reti

SECTION bss_user

_isr_jiffies:  defw 0
isr_phase:     defb 0
