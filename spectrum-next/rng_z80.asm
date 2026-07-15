; rng_z80.asm - the RANDOM.ASM 24-bit shift-register RNG in Z80.
;
; Byte-exact replacement for core/rng.c's rng_RANDOM (the C version is
; compiled out with -DDOD_RNG_ASM and remains the normative reference).
; Verified byte-identical - outputs, SEED evolution AND the final
; rng.carry state byte - by tests/z80rng/run.sh (z88dk-ticks vs gcc).
;
; Why: the C rng_RANDOM shifts bit-by-bit through lsl()/rol() helper
; CALLS (~90 calls per random byte under zsdcc), and dungeon generation
; spins it hard - the "PREPARE!" pause measured ~30 wall seconds at
; 28 MHz.  This routine is ~650 T-states per call (~23 us).
;
; Exactness notes, mirroring rng.c line for line:
;   Per outer step (8 per call):
;     b = popcount(SEED[2] & 0xE1); b = lsr(b)
;       -> rng.carry = b & 1 = PARITY of the four tap bits.
;          The Z80 P/V flag after AND computes exactly this (P/V set =
;          even parity; the masked byte has only the four tap bits).
;     rol SEED[0], rol SEED[1], rol SEED[2]
;       -> a 24-bit rotate left through carry = three chained RL ops.
;   After the last step rng.carry = old SEED[2] bit 7 (0/1) - stored,
;   because the carry byte is part of the dumped/saved game state.
;   The inner lsl() loop's intermediate rng.carry values never survive
;   an outer step, so they need no equivalent here.
;   Returns SEED[0] (8-bit return = L in the zsdcc sdcccall(0) ABI,
;   verified against zsdcc 4.5.0 codegen).
;
; C prototype:  dodBYTE rng_RANDOM(void);
; Register usage: AF, BC, DE, HL only - no IX/IY (CLIB owns IY), no
; shadow set, no self-modification; IM2-interrupt safe and re-entrant
; in the ways that matter (though the game only calls it from the main
; thread).

SECTION code_user

PUBLIC _rng_RANDOM
EXTERN _rng                     ; rng_state { SEED[3]; carry; }

_rng_RANDOM:
    ld   hl, _rng
    ld   c, (hl)                ; C = SEED[0]
    inc  hl
    ld   d, (hl)                ; D = SEED[1]
    inc  hl
    ld   e, (hl)                ; E = SEED[2]   (HL parked at _rng+2)
    ld   b, 8
rng_step:
    ld   a, e
    and  0xE1                   ; taps; P/V = parity, carry cleared
    jp   pe, rng_even           ; even parity -> carry-in stays 0
    scf                         ; odd parity  -> carry-in 1
rng_even:
    rl   c                      ; 24-bit rotate left through carry:
    rl   d                      ;   SEED[0] <- SEED[1] <- SEED[2] <- cy
    rl   e                      ; carry <- old SEED[2] bit 7
    djnz rng_step               ; djnz preserves carry

    ld   (hl), e                ; store SEED back (HL = _rng+2)
    dec  hl
    ld   (hl), d
    dec  hl
    ld   (hl), c
    ld   a, 0                   ; keep the carry flag (xor a would not)
    adc  a, a                   ; A = final carry as 0/1
    inc  hl
    inc  hl
    inc  hl                     ; HL = _rng+3 = rng.carry
    ld   (hl), a
    ld   l, c                   ; return SEED[0]
    ret
