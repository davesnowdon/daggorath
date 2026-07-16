; probe_asm.asm - Dave-chip probe: measurement loops + EXOS call helpers.
;
; All measurements are LATCH-POLLED with the CPU DI'd: Dave's interrupt
; latches (port B4h) set regardless of the Z80's interrupt state as long as
; the source's enable bit is on, so the probe counts latch sets against the
; 1 Hz latch as a timebase without ever taking an interrupt or disturbing
; EXOS's IM1 handler.  B4h write semantics (Dave doc + ep128emu dave.cpp):
;   b0=1 enable snd(1kHz/50Hz/TG) int   b1=1 reset its latch
;   b2=1 enable 1Hz int                 b3=1 reset its latch
;   b4/b5 INT1 (video), b6/b7 INT2 - and writing an enable bit as 0 ALSO
;   clears that source's latch (dave.cpp: tmp=value^0x55 decode).
; B4h read: b0 snd flip-flop state, b1 snd latch, b2 1Hz state, b3 1Hz
; latch, b4/b5 INT1 state/latch, b6/b7 INT2.
;
; Cross-boundary data goes through globals (no reliance on the sdcc arg
; ABI); 8-bit returns go in L (zsdcc convention).

SECTION code_user

PUBLIC _meas_rate
PUBLIC _stretch_run
PUBLIC _snap_wait
PUBLIC _stack_to_p0
PUBLIC _rd_b0, _rd_b1, _rd_b2, _rd_b3, _rd_b4
PUBLIC _wr_b1
PUBLIC _fast_mode
PUBLIC _exos_open_dat, _exos_close, _exos_read, _exos_alloc, _exos_free

PUBLIC _meas_a7, _meas_frq, _meas_count
PUBLIC _stretch_base, _stretch_count
PUBLIC _pg_val
PUBLIC _ex_chan, _ex_status, _ex_seg, _ex_bound, _ex_buf, _ex_len

; --- move the C stack into page 0 (0x3F00) so P1/P2 remaps can never pull
; the rug out.  Called first thing from main(), which has no live frame.
_stack_to_p0:
    pop  hl
    ld   sp, 0x3f00
    jp   (hl)

; --- set fast mode (no memory wait states), matching the game's plat_init
_fast_mode:
    ld   a, 0x0c
    out  (0xbf), a
    ret

; --- paging register reads (values return in L) and P1 write
_rd_b0:
    in   a, (0xb0)
    ld   l, a
    ret
_rd_b1:
    in   a, (0xb1)
    ld   l, a
    ret
_rd_b2:
    in   a, (0xb2)
    ld   l, a
    ret
_rd_b3:
    in   a, (0xb3)
    ld   l, a
    ret
_rd_b4:
    in   a, (0xb4)
    ld   l, a
    ret
_wr_b1:
    ld   a, (_pg_val)
    out  (0xb1), a
    ret

; --- passive pre-measurement delay: wait for 10 toggles of the 1 Hz
; divider FLIP-FLOP (B4h read bit2 - toggles once per second regardless of
; any enable bit), i.e. ~10 s, without writing a single Dave register.
; This is the host-side snapshot window for capturing EXOS's virgin state.
_snap_wait:
    ld   b, 10
    in   a, (0xb4)
    and  0x04
    ld   c, a
snapw_loop:
    in   a, (0xb4)
    and  0x04
    cp   c
    jr   z, snapw_loop
    ld   c, a
    djnz snapw_loop
    ret

; --- measure the sound-interrupt rate for one configuration.
; In:  _meas_a7  = A7h value (int-source select b5/b6 + DAC bits b3/b4)
;      _meas_frq = tone-0 period (12 bit)
; Out: _meas_count = latch sets in exactly one 1Hz-gated second
_meas_rate:
    di
    ; tone 0 period (low byte, then high nybble + square-wave mode)
    ld   a, (_meas_frq)
    out  (0xa0), a
    ld   a, (_meas_frq+1)
    out  (0xa1), a
    ; force-reload the tone-0 counter: pulse sync (A7h b0 "hold at preset"),
    ; then release with the target configuration
    ld   a, (_meas_a7)
    or   0x01
    out  (0xa7), a
    ld   a, (_meas_a7)
    out  (0xa7), a
    ; enable snd + 1Hz interrupts, reset both latches
    ld   a, 0x0f
    out  (0xb4), a
    ; wait for the NEXT 1Hz latch (aligns the window start)
mr_wait1hz:
    in   a, (0xb4)
    and  0x08
    jr   z, mr_wait1hz
    ; restart both latches; count snd latches until the next 1Hz latch
    ld   a, 0x0f
    out  (0xb4), a
    ld   hl, 0
mr_poll:
    in   a, (0xb4)
    bit  3, a
    jr   nz, mr_done            ; 1Hz latch: the second is up
    bit  1, a
    jr   z, mr_poll
    inc  hl
    ld   a, 0x07                ; ack snd latch, keep enables, keep 1Hz latch
    out  (0xb4), a
    jr   mr_poll
mr_done:
    ld   (_meas_count), hl
    ; restore a sane EXOS-ish state: A7h = 1kHz int select, all run, no DAC
    xor  a
    out  (0xa7), a
    ld   a, 0x0f
    out  (0xb4), a
    ei
    ret

; --- memory-read throughput: count 16-read blocks at _stretch_base for one
; 1Hz-gated second.  The block loop only reads (HL) with INC L (wraps inside
; a 256-byte page - we measure bus stretch, there is no cache), so the count
; ratio between a non-video and a video segment is the VRAM stretch factor.
_stretch_run:
    di
    ld   a, 0x0f
    out  (0xb4), a
sr_wait1hz:
    in   a, (0xb4)
    and  0x08
    jr   z, sr_wait1hz
    ld   a, 0x0f
    out  (0xb4), a
    ld   hl, (_stretch_base)
    ld   de, 0
sr_loop:
    ld   a, (hl)
    inc  l
    ld   a, (hl)
    inc  l
    ld   a, (hl)
    inc  l
    ld   a, (hl)
    inc  l
    ld   a, (hl)
    inc  l
    ld   a, (hl)
    inc  l
    ld   a, (hl)
    inc  l
    ld   a, (hl)
    inc  l
    ld   a, (hl)
    inc  l
    ld   a, (hl)
    inc  l
    ld   a, (hl)
    inc  l
    ld   a, (hl)
    inc  l
    ld   a, (hl)
    inc  l
    ld   a, (hl)
    inc  l
    ld   a, (hl)
    inc  l
    ld   a, (hl)
    inc  l
    in   a, (0xb4)
    bit  3, a
    jr   nz, sr_done
    inc  de
    jr   sr_loop
sr_done:
    ex   de, hl
    ld   (_stretch_count), hl
    ld   a, 0x0f
    out  (0xb4), a
    ei
    ret

; --- EXOS call helpers (fn byte must be inline after RST 30h, so one
; helper per function; IX/IY saved defensively around every call).
_exos_open_dat:                 ; fn 1: open channel _ex_chan on PROBEDAT.BIN
    push ix
    push iy
    ld   a, (_ex_chan)
    ld   de, dat_name
    rst  0x30
    defb 1
    ld   (_ex_status), a
    pop  iy
    pop  ix
    ret

_exos_close:                    ; fn 3: close channel _ex_chan
    push ix
    push iy
    ld   a, (_ex_chan)
    rst  0x30
    defb 3
    ld   (_ex_status), a
    pop  iy
    pop  ix
    ret

_exos_read:                     ; fn 6: read _ex_len bytes to _ex_buf
    push ix
    push iy
    ld   a, (_ex_chan)
    ld   de, (_ex_buf)
    ld   bc, (_ex_len)
    rst  0x30
    defb 6
    ld   (_ex_status), a
    pop  iy
    pop  ix
    ret

_exos_alloc:                    ; fn 24: allocate segment
    push ix
    push iy
    rst  0x30
    defb 24
    ld   (_ex_status), a
    ld   a, c
    ld   (_ex_seg), a
    ld   (_ex_bound), de
    pop  iy
    pop  ix
    ret

_exos_free:                     ; fn 25: free segment _ex_seg
    push ix
    push iy
    ld   a, (_ex_seg)
    ld   c, a
    rst  0x30
    defb 25
    ld   (_ex_status), a
    pop  iy
    pop  ix
    ret

dat_name:
    defb 14
    defm "A:PROBEDAT.BIN"

SECTION bss_user

_meas_a7:       defb 0
_meas_frq:      defw 0
_meas_count:    defw 0
_stretch_base:  defw 0
_stretch_count: defw 0
_pg_val:        defb 0
_ex_chan:       defb 0
_ex_status:     defb 0
_ex_seg:        defb 0
_ex_bound:      defw 0
_ex_buf:        defw 0
_ex_len:        defw 0
