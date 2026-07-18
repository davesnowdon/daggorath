; dance_ep.asm - the EXOS dance: in-game save/load file I/O on a machine
; where EXOS has been dead since boot.
;
; THE PROBLEM: EXOS's system segment is FFh, which the game repurposed as
; video RAM.  The EXOS-owned region (segment offsets 0x1806-0x3FFF per the
; fn-24 .SHARE boundary: channel RAM, system variables, system stack) was
; overwritten by FB1 + the draw tables + the LPT - all REBUILDABLE video
; state.  FB0 (offsets 0x0000-0x17FF) sits below the boundary and is never
; touched by EXOS.  At boot the loader stashed that region into the same
; offsets of its own segment (plus the EXOS page-zero stub that segment
; already carries); this dance restores it around each file operation.
;
; HARD-WON PAGING RULES (each cost a debugging session):
;   * The system segment content is read/written wherever FFh is MAPPED -
;     but when EXOS RUNS (calls or its ISR), it requires FFh AT PAGE 2:
;     "page 2 will always be the system segment containing the stack"
;     (kernel Ch7) is an invariant the kernel relies on for its variables
;     (0xBFxx) and stack, not a convention.  Every prior EXOS interaction
;     in this port (probe, loader) accidentally satisfied it; running the
;     window with the game's FEh at page 2 dies on the FIRST interrupt.
;   * Page 0 must hold a segment with a live EXOS page-zero stub (RST 30h
;     dispatcher, IM1 entry, kernel scratch at 0x59/0x5A).  The game's FCh
;     page zero holds OUR bytes; the loader's segment still has the stub
;     EXOS built when the loader started.
;   * Consequently the user buffer/filename EXOS sees must live in pages
;     0, 1 or 3 by USER addressing at call time.  The save blob (core BSS,
;     a page-2 address) is bounced into the loader segment's dead low RAM
;     (the loader image is long dead) and passed as a PAGE-0 address.
;   * Our stack (0xBB00-0xBF00) is a page-2 address too: the window runs
;     on a small dedicated stack inside this file's BSS (page 1).
;
; The dance runs at its LINK address (code_user, page 1 = the game's FDh,
; which is never remapped while it executes).  Page 3 keeps FFh throughout
; (the game's video mapping; the stub saves/restores whatever is there).
; Nick is parked on a copy of the LPT inside FB0 so its sync + IRQ records
; keep ticking for EXOS while the real LPT area holds EXOS state again.
;
; The C wrapper (plat_ep.c) rebuilds everything the restore destroys: the
; LPT, the rasterizer tables, FB1's content, and re-arms Dave.  Jiffies
; pause during the window (benign for a manual save).

SECTION code_user

PUBLIC _dance_run
PUBLIC _dance_op
PUBLIC _dance_status
PUBLIC _dance_loader_seg
PUBLIC _dance_buf
PUBLIC _dance_len
PUBLIC _dance_hdr

GAME_P0SEG EQU 0xFC
GAME_P2SEG EQU 0xFE
SYS_SEG    EQU 0xFF
LDR_FNAME  EQU 0x02F0          ; filename copy in the loader segment (P0)
LDR_BUF    EQU 0x0300          ; blob bounce in the loader segment (P0)

; C entry: staging globals set, interrupts DI'd, sound stopped.
; EXOS may clobber anything: save the sdcc frame registers.
_dance_run:
    push ix
    push iy

    ; A. park Nick on a copy of the LPT at Nick 0xC000 (inside FB0; the
    ;    display shows stale FB0 with a small glitch strip until the game
    ;    repaints).  The running LPT's final record has the reload bit, so
    ;    Nick picks up the new base at the next frame top.
    ld   hl, 0xF800
    ld   de, 0xC000
    ld   bc, 112
    ldir
    ; The live LPT's LD1 tracks the double-buffer flip, so about half the
    ; time it points at FB1 (0xD800) - exactly what step B floods with the
    ; EXOS stash.  Force the parked copy's LD1 (visible record = record 0,
    ; bytes +4/+5) to FB0 so the display is stale-but-stable either way.
    ld   hl, 0xC000
    ld   (0xC004), hl
    ld   a, 0x00                ; LPT base = Nick 0xC000
    out  (0x82), a
    ld   a, 0xCC
    out  (0x83), a

    ; B. restore the EXOS-state stash into FFh: loader segment at P2
    ;    (source window 0x9800+), FFh at P3 (dest 0xD800+).  Trashes
    ;    FB1/tables/LPT - all rebuilt by the C wrapper.
    ld   a, (_dance_loader_seg)
    out  (0xb2), a
    ld   hl, 0x9800
    ld   de, 0xD800
    ld   bc, 0x2800
    ldir

    ; C. loader segment into P0: the EXOS stub for the window, and the
    ;    P0 window for the filename + blob bounce.
    ld   a, (_dance_loader_seg)
    out  (0xb0), a
    ld   hl, dance_fname        ; (this code's data, page 1)
    ld   de, LDR_FNAME
    ld   bc, 13
    ldir
    ld   a, GAME_P2SEG          ; game data back at P2 for the blob copy
    out  (0xb2), a
    ld   a, (_dance_op)
    or   a
    jr   nz, dance_no_out
    ld   hl, _dance_hdr         ; SAVE: 8-byte envelope header first
    ld   de, LDR_BUF            ; (magic/version/len/checksum, built by
    ld   bc, 8                  ; plat_save_state)
    ldir
    ld   hl, (_dance_buf)       ; then blob -> loader-segment bounce
    ld   bc, (_dance_len)       ; (DE already = LDR_BUF+8)
    ldir
dance_no_out:

    ; D. window stack (page 1) + system segment at P2 + EXOS hw state
    ld   (dance_sp_save), sp
    ld   sp, dance_stack_top
    ld   a, SYS_SEG
    out  (0xb2), a
    xor  a
    out  (0xa7), a
    ld   a, 0x3c                ; enable+reset 1Hz + video ints (probe f)
    out  (0xb4), a
    ei

    ; E. the file I/O (channel 9; buffer + name are PAGE-0 addresses in
    ;    the loader segment, which EXOS reads via the user P0 it saved)
    ld   a, (_dance_op)
    or   a
    jr   nz, dance_load

    ; SAVE: create; if that fails (file exists on this EXDOS), open the
    ; existing file and overwrite in place (the blob length is constant)
    ld   a, 9
    ld   de, LDR_FNAME
    rst  0x30
    defb 2                      ; create channel
    or   a
    jr   z, dance_wr
    ld   a, 9
    ld   de, LDR_FNAME
    rst  0x30
    defb 1                      ; open channel
    or   a
    jr   nz, dance_fail
dance_wr:
    ld   a, 9
    ld   de, LDR_BUF
    ld   hl, (_dance_len)       ; header + payload
    ld   bc, 8
    add  hl, bc
    ld   b, h
    ld   c, l
    rst  0x30
    defb 8                      ; write block
    ld   (_dance_status), a
    jr   dance_close

dance_load:
    ld   a, 9
    ld   de, LDR_FNAME
    rst  0x30
    defb 1                      ; open channel
    or   a
    jr   nz, dance_fail
    ld   a, 9
    ld   de, LDR_BUF
    ld   hl, (_dance_len)       ; header + payload
    ld   bc, 8
    add  hl, bc
    ld   b, h
    ld   c, l
    rst  0x30
    defb 6                      ; read block
    ld   (_dance_status), a

dance_close:
    ld   a, 9
    rst  0x30
    defb 3                      ; close channel
    jr   dance_after
dance_fail:
    ld   (_dance_status), a

dance_after:
    ; F. freeze EXOS again, collect results, put the machine back
    di
    ld   sp, (dance_sp_save)
    ld   a, GAME_P2SEG
    out  (0xb2), a
    ld   a, (_dance_op)
    or   a
    jr   z, dance_no_in
    ld   hl, LDR_BUF            ; LOAD: envelope header out for the C
    ld   de, _dance_hdr         ; wrapper to validate...
    ld   bc, 8
    ldir
    ld   de, (_dance_buf)       ; ...then bounce -> blob (P0 -> P2)
    ld   bc, (_dance_len)       ; (HL already = LDR_BUF+8)
    ldir
dance_no_in:

    ; re-stash: EXOS state evolved during the window
    ld   a, (_dance_loader_seg)
    out  (0xb2), a
    ld   hl, 0xD800
    ld   de, 0x9800
    ld   bc, 0x2800
    ldir
    ld   a, GAME_P2SEG
    out  (0xb2), a
    ld   a, GAME_P0SEG
    out  (0xb0), a              ; page 0 = game code (our 0038h JP again)
    ld   a, 0x0c
    out  (0xbf), a              ; fast mode (EXOS resets the wait policy)
    ld   a, 0x30
    out  (0xb4), a              ; our convention: video int enabled + acked
    pop  iy
    pop  ix
    ret                         ; DI held; the C wrapper rebuilds the video

dance_fname:
    defb 12
    defm "A:DAGGOR.SAV"

SECTION bss_compiler

_dance_op:         defb 0      ; 0 = save, 1 = load
_dance_status:     defb 0      ; EXOS status of the block transfer
_dance_loader_seg: defb 0      ; from the loader handoff (byte 10)
_dance_buf:        defw 0
_dance_len:        defw 0
_dance_hdr:        defs 8      ; save-envelope header (magic 'DS', ver,
                               ; flags, len16, fletcher16 - plat_ep.c)
dance_sp_save:     defw 0
dance_stack:       defs 160    ; window stack (page 1): IM1 pushes + the
dance_stack_top:               ; kernel's entry saves before it switches
