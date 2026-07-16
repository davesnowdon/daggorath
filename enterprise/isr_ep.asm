; isr_ep.asm - Enterprise IM1 interrupt: frame clock + keyboard + PCM sample
; engine.
;
; TWO Dave interrupt sources share the handler (dispatch on a B4h read):
;
;   int_1 (video, latch = read bit5): once per PAL frame (50 Hz) from the
;     LPT IRQ record.  Drives the 16-bit jiffy counter (+1,+1,+1,+1,+2
;     phase pattern = exactly 60 jiffies per 50 frames), the keyboard
;     matrix scan, and the present-pacing frame flag.
;
;   snd (tone 0, latch = read bit1): enabled ONLY while a sample plays.
;     Probe-verified (PHASE4-DAVE-PROBE.md): rate = 250000/(n+1) with the
;     period n retuned PER SAMPLE by plat_sound_play (n=33 -> 7352.94 Hz
;     full rate, n=67 -> 3676.47 Hz half rate), and tone-0 keeps
;     interrupting with both DAC bits set (A7h=0x58).  Each tick fetches
;     one 8-bit sample byte through the Z80 page-1 window, converts it
;     through the current 256-byte volume LUT, and writes the Dave L+R
;     DACs (A8h/ACh).
;
; SAMPLE STATE lives in the SHADOW register set, which the ISR owns on this
; platform (established Phase-2 policy - mainline code never holds live
; alternate registers; plat_sound_play loads them under DI):
;     HL' = window pointer (0x4000-0x7FFF)   D' = sample's segment
;     C'  = 128-byte blocks remaining        B', E' = free
; Samples are 128-aligned and whole-block padded (gen_sfxep.py), so the end
; check is "L wrapped to a 128 boundary -> dec C".
;
; B4h ack subtlety (dave.cpp value^0x55 decode): writing an ENABLE bit as 0
; also CLEARS that source's latch, so every write here must keep the other
; source's enable high and its reset bit low:
;     0x13 = ack snd    (snd on+reset,  video on, video latch untouched)
;     0x31 = ack video  (video on+reset, snd on,  snd latch untouched)
;     0x30 = ack video, snd disabled (idle)
;     0x10 = snd off+cleared, video on, video latch untouched (sample end)
; _b4_ack_vid holds 0x31/0x30 depending on whether a sample is active.
;
; PLACEMENT: the whole handler must live OUTSIDE Z80 page 1 (0x4000-0x7FFF)
; because the sample path remaps page 1 to the sample's segment while
; executing - so this file assembles into code_compiler, which starts at
; 0x0100 (the Makefile asserts _frame_isr < 0x4000).  P1 is restored to the
; game's code segment (FDh) before the fetch window ends.
;
; This ISR remains the ONLY user of 0038h; entry is a JP patched in by
; ep_isr_start (plat_ep.c).  EXOS is dead.

SECTION code_compiler

PUBLIC _frame_isr
PUBLIC _isr_jiffies
PUBLIC _frame_flag
PUBLIC _snd_active
PUBLIC _b4_ack_vid
PUBLIC _snd_lut
PUBLIC _ep_snd_start
PUBLIC _ep_snd_stop
PUBLIC _snd_seg, _snd_waddr, _snd_blocks, _snd_period
EXTERN _kbd_isr_scan

_frame_isr:
    push af
    in   a, (0xb4)
    bit  1, a                   ; tone-0 (sample) latch?
    jr   z, isr_no_snd

    ; ---- sample tick -------------------------------------------------
    ld   a, 0x13                ; ack snd, keep both enables, video latch kept
    out  (0xb4), a
    exx
    ld   a, d
    out  (0xb1), a              ; P1 <- sample segment
    ld   a, (hl)                ; fetch the 8-bit sample byte
    inc  hl
    ld   (snd_lutld+1), a       ; LUT index via SMC (no spare register pair)
snd_lutld:
_snd_lut:
    ld   a, (0x9700)            ; +2 byte = LUT page, set once by ep_snd_init
                                ; (the LUT is rebuilt in place on tier change)
    out  (0xa8), a              ; Dave left DAC
    out  (0xac), a              ; Dave right DAC
    ld   a, 0xfd
    out  (0xb1), a              ; P1 <- game code segment again
    ld   a, l
    and  0x7f                   ; 128-byte block boundary?
    jr   nz, snd_tick_done
    dec  c
    jr   nz, snd_tick_done
    ; sample finished: sound interrupt off (+latch cleared), engine idle
    ld   a, 0x10
    out  (0xb4), a
    xor  a
    ld   (_snd_active), a
    ld   a, 0x30
    ld   (_b4_ack_vid), a
snd_tick_done:
    exx
    in   a, (0xb4)              ; video may have latched during the tick

isr_no_snd:
    bit  5, a                   ; video (int_1) latch?
    jr   z, isr_exit

    ; ---- frame tick ---------------------------------------------------
    ld   a, (_b4_ack_vid)       ; ack video, preserving the snd enable state
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
    ld   a, 1
    ld   (_frame_flag), a       ; plat_present pacing

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

isr_exit:
    pop  af
    ei
    reti

; ---- main-thread sound control (called from sound_ep.c) ----------------
; plat_sound_play stages _snd_seg/_snd_waddr/_snd_blocks/_snd_period and
; patches the LUT page byte, then calls _ep_snd_start: retune tone 0 (the A7h
; b0 sync pulse forces the counter to reload the new period - probe (b)),
; load the shadow-register sample state, arm the sound interrupt.
; Preemption = just calling it again: everything is rewritten under DI.
_ep_snd_start:
    di
    ld   a, (_snd_period)
    out  (0xa0), a              ; tone-0 period low (33 or 67; high nybble 0)
    xor  a
    out  (0xa1), a
    ld   a, 0x59
    out  (0xa7), a              ; hold tone 0 at preset...
    ld   a, 0x58
    out  (0xa7), a              ; ...release: counter restarts on new period
    exx
    ld   hl, (_snd_waddr)
    ld   a, (_snd_seg)
    ld   d, a
    ld   a, (_snd_blocks)
    ld   c, a
    exx
    ld   a, 1
    ld   (_snd_active), a
    ld   a, 0x31
    ld   (_b4_ack_vid), a
    ld   a, 0x13                ; enable+reset snd latch, video untouched
    out  (0xb4), a
    ei
    ret

_ep_snd_stop:
    di
    ld   a, 0x10                ; snd off + latch cleared, video untouched
    out  (0xb4), a
    xor  a
    ld   (_snd_active), a
    ld   a, 0x30
    ld   (_b4_ack_vid), a
    ld   a, 0x20
    out  (0xa8), a              ; both DACs back to midline (silence)
    out  (0xac), a
    ei
    ret

SECTION bss_compiler

_isr_jiffies:  defw 0
isr_phase:     defb 0
_frame_flag:   defb 0
_snd_active:   defb 0
_snd_seg:      defb 0
_snd_waddr:    defw 0
_snd_blocks:   defb 0
_snd_period:   defb 0

SECTION data_compiler

_b4_ack_vid:   defb 0x30        ; video-ack value: 0x31 while a sample plays
