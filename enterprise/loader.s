; loader.s - Dungeons of Daggorath, Enterprise 64/128 big-program loader.
;
; WHY: EXOS loads a type-5 .com into a single ~16 KB user segment (page 0,
; Z80 0x0100-0x3FFF).  Our game is 34.5 KB (Z80 0x0100-0x87E1, three
; segments) so a plain .com only ever gets its first 16 KB into RAM.  This
; small loader (which DOES fit page 0) reads the rest of the game image off
; disk into RAM and enters the game's crt0.
;
; SEGMENTS: on this boot (EP128 + IS-BASIC + EXDOS + FileIO) the non-video
; segments F8h-FBh are all taken, so EXOS fn 24 (allocate) returns 0xCF -
; there is nothing to give.  BUT the four Nick-visible segments FCh-FFh stay
; free "until last" and can be claimed directly with OUT (Bn),seg (vault
; 02/03/21) - exactly what the standalone video test did.  We claim them by
; fixed number: FC/FD/FE = the game's three code pages, FF = video (and,
; until our final DI, EXOS's own system segment - fine, EXOS is dead by then).
;
; PLAN:
;   * EXOS loads THIS at 0x0100 (P0 = a non-video loader segment), IRQs on,
;     EXOS alive with its system segment in P3 (0xC000-0xFFFF).
;   * Open GAME.BIN (raw z88dk image: byte 0 == game 0x0100) on channel 1.
;   * Read the three game pages into FC/FD/FE.  EXOS works through P3, so we
;     read through the *P1* window (0x4000-0x7FFF): page the target segment
;     into P1, EXOS fn 6 read-block into the window.  (P1 is untouched by
;     EXOS; if a real machine proves otherwise, fall back to chunk-via-P0.)
;   * Close the file, DI (EXOS now dead - we own the machine).
;   * Copy a tiny trampoline into FF (already in P3) high, clear of the
;     framebuffer/LPT, and run it from there.  Running from P3 lets it repage
;     P0/P1/P2 (FC/FD/FE) without pulling the rug from under itself, then
;     JP 0x0100 into the game crt0.  plat_init then reclaims FF for video.
;
; Build: sjasmplus loader.s --raw=loader.com   (emits header+body from 0x00F0)

GAME_ORG    EQU 0x0100
GAME_P0LEN  EQU 0x4000 - GAME_ORG      ; 0x3F00  (page-0 bytes: game 0x0100..0x3FFF)
GAME_P1LEN  EQU 0x4000                 ; page-1 bytes: game 0x4000..0x7FFF
    INCLUDE "sizes.inc"                ; defines GAME_P2LEN (page-2 remainder)

SEG_P0      EQU 0xFC                   ; game code page 0  -> Z80 P0 at run time
SEG_P1      EQU 0xFD                   ; game code page 1  -> Z80 P1 at run time
SEG_P2      EQU 0xFE                   ; game code page 2  -> Z80 P2 at run time
SEG_VID     EQU 0xFF                   ; video / EXOS system segment (P3)

WIN         EQU 0x4000                 ; P1 load window base (0x4000-0x7FFF)
PORT_P0     EQU 0xB0
PORT_P1     EQU 0xB1
PORT_P3     EQU 0xB3
TRAMP_DST   EQU 0xFF00                 ; where the trampoline runs (in FF/P3),
                                       ; above framebuffer (C000) and LPT (F800)

    MACRO EXOS n
    rst 0x30
    db n
    ENDM

    ORG 0x00F0
; ---- EXOS type-5 module header (16 bytes) ----
    db 0x00, 0x05
    dw codeEnd - main                  ; module body length
    dw 0,0,0,0,0,0

main:                                  ; 0x0100
    di
    ld sp, 0x0FE0                      ; loader stack, page 0, below the window
    ei                                 ; EXOS calls expect interrupts on (midiplay)

    ; ---- open GAME.BIN on channel 1 ----
    ld a, 1
    ld de, fname
    EXOS 1
    or a
    jr nz, fail

    ; ---- read page 0 (game 0x0100-0x3FFF) into FC @ WIN+0x100 ----
    ld a, SEG_P0
    out (PORT_P1), a
    ld a, 1
    ld de, WIN + 0x0100
    ld bc, GAME_P0LEN
    EXOS 6

    ; ---- read page 1 (game 0x4000-0x7FFF) into FD @ WIN ----
    ld a, SEG_P1
    out (PORT_P1), a
    ld a, 1
    ld de, WIN
    ld bc, GAME_P1LEN
    EXOS 6

    ; ---- read page 2 (game 0x8000..end) into FE @ WIN ----
    ld a, SEG_P2
    out (PORT_P1), a
    ld a, 1
    ld de, WIN
    ld bc, GAME_P2LEN
    EXOS 6

    ; ---- close the file ----
    ld a, 1
    EXOS 3

    ; ---- own the machine: EXOS is done, no more RST 30h ----
    di
    ld a, SEG_VID
    out (PORT_P3), a                   ; FF into P3 (it already is; make it sure)

    ; ---- copy the trampoline into FF@TRAMP_DST and run it there ----
    ld hl, tramp
    ld de, TRAMP_DST
    ld bc, trampEnd - tramp
    ldir
    jp TRAMP_DST                       ; enter trampoline (running from P3/FF)

fail:
    out (0x81), a                      ; border = the EXOS error status code
fail_loop:
    jr fail_loop

; ---- trampoline: assembled here, RUN from TRAMP_DST (P3/FF). No stack. ----
; Segment numbers are fixed constants, so no page-0 reads are needed before
; we page page 0 out.  Map the three game segments and enter the game.
tramp:
    ld a, SEG_P0
    out (PORT_P0), a                   ; FC -> P0 (loader is now gone)
    ld a, SEG_P1
    out (PORT_P0 + 1), a               ; FD -> P1
    ld a, SEG_P2
    out (PORT_P0 + 2), a               ; FE -> P2
    jp GAME_ORG                        ; enter the game crt0 at 0x0100
trampEnd:

fname:
    db 10, "A:GAME.BIN"               ; device prefix required (bare name hits
                                       ; the wrong default device and fails)

codeEnd:
