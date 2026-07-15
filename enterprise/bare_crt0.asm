;
;       Enterprise 64/128 BARE C stub for Dungeons of Daggorath.
;
;       Derived from z88dk's enterprise_crt0.asm (Stefano Bodrato, 2011) with
;       ALL EXOS startup calls and the `out (0b2h),0ffh` page-2 remap removed.
;
;       WHY: our big-program loader has already paged the three game code
;       segments into Z80 pages 0-2 and killed EXOS; the game then seizes the
;       machine bare-metal (own LPT, own IM1 in later phases).  The stock crt0
;       opens EXOS VIDEO/KEYBOARD channels and forces segment FFh into page 2,
;       both of which fight our manual paging.  This bare crt0 only establishes
;       the C runtime (SP, BSS, atexit) and calls main.
;
                MODULE  enterprise_crt0

                defc    crt0 = 1
                INCLUDE "zcc_opt.def"

        EXTERN    _main

        PUBLIC    __Exit
        PUBLIC    l_dcal

        PUBLIC    set_exos_multi_variables
        PUBLIC    _DEV_VIDEO
        PUBLIC    _DEV_KEYBOARD
        PUBLIC    _DEV_NET
        PUBLIC    _DEV_EDITOR
        PUBLIC    _DEV_SERIAL
        PUBLIC    _DEV_TAPE
        PUBLIC    _DEV_PRINTER
        PUBLIC    _DEV_SOUND

        PUBLIC    _esccmd
        PUBLIC    _esccmd_cmd
        PUBLIC    _esccmd_x
        PUBLIC    _esccmd_y
        PUBLIC    _esccmd_p1
        PUBLIC    _esccmd_p2
        PUBLIC    _esccmd_p3
        PUBLIC    _esccmd_p4
        PUBLIC    _esccmd_p5
        PUBLIC    _esccmd_p6
        PUBLIC    _esccmd_p7
        PUBLIC    _esccmd_p8
        PUBLIC    _esccmd_p9
        PUBLIC    _esccmd_env
        PUBLIC    _esccmd_p
        PUBLIC    _esccmd_vl
        PUBLIC    _esccmd_vr
        PUBLIC    _esccmd_sty
        PUBLIC    _esccmd_ch
        PUBLIC    _esccmd_d
        PUBLIC    _esccmd_f
        PUBLIC    _esccmd_en
        PUBLIC    _esccmd_ep
        PUBLIC    _esccmd_er
        PUBLIC    _esccmd_phase
        PUBLIC    _esccmd_cp
        PUBLIC    _esccmd_cl
        PUBLIC    _esccmd_cr
        PUBLIC    _esccmd_pd

        defc    TAR__clib_exit_stack_size = 32
        defc    TAR__register_sp = 0x7f00
        defc __CPU_CLOCK = 4000000
        INCLUDE "crt/classic/crt_rules.inc"

IF      !DEFINED_CRT_ORG_CODE
        defc    CRT_ORG_CODE  = 100h
ENDIF
        org     CRT_ORG_CODE

;----------------------
; Execution starts here
;----------------------
start:
        di                              ; bare metal: EXOS is dead, no IM1 yet
        ld      (__restore_sp_onexit+1),sp
        INCLUDE "crt/classic/crt_init_sp.inc"      ; SP = REGISTER_SP (0xBF00)

        call    crt0_init
        INCLUDE "crt/classic/crt_init_atexit.inc"
        INCLUDE "crt/classic/crt_init_heap.inc"
        INCLUDE "crt/classic/crt_init_eidi.inc"

        call    _main

__Exit:
        call    crt0_exit
        INCLUDE "crt/classic/crt_exit_eidi.inc"

IF (!DEFINED_startup | (startup=1))
warmreset:
    PUBLIC    warmreset
    ld      sp, 0100h
    ld      a, 0ffh
    out     (0b2h), a
    ld      c, 60h
    rst     30h
    defb    0
    ld      de, _basiccmd
    rst     30h
    defb    26
    ld      a, 01h
    out     (0b3h), a
    ld      a, 6
    jp      0c00dh

_basiccmd:
    defb    5
    defm    "BASIC"
ENDIF

__restore_sp_onexit:
    ld      sp,0
    ret

l_dcal:
    jp      (hl)

set_exos_multi_variables:
_l1:    ld    b, 1
        ld    c, (hl)
        inc   c
        dec   c
        ret   z
        inc   hl
        ld    d, (hl)
        inc   hl
        rst   30h
        defb  16
        jr    _l1
        ret

daveReset:
        push  bc
        xor   a
        ld    bc, 010afh
_l2:    out   (c), a
        dec   c
        djnz  _l2
        pop   bc
        ret


_DEV_VIDEO:
        defb  6
        defm  "VIDEO:"

_DEV_KEYBOARD:
        defb  9
        defm  "KEYBOARD:"

_DEV_EDITOR:
        defb  4
        defm  "EDITOR:"

_DEV_NET:
        defb  4
        defm  "NET:"

_DEV_SERIAL:
        defb  7
        defm  "SERIAL:"

_DEV_TAPE:
        defb  5
        defm  "TAPE:"

_DEV_PRINTER:
        defb  8
        defm  "PRINTER:"

_DEV_SOUND:
        defb  6
        defm  "SOUND:"

_esccmd:
        defb  27
_esccmd_cmd:
        defb  0
_esccmd_x:
_esccmd_p1:
_esccmd_env:
_esccmd_en:
        defb  0
_esccmd_p2:
_esccmd_p:
_esccmd_ep:
        defb  0
_esccmd_y:
_esccmd_p3:
_esccmd_er:
        defb  0
_esccmd_phase:
_esccmd_p4:
_esccmd_vl:
_esccmd_cp:
        defb  0
_esccmd_p5:
_esccmd_vr:
        defb  0
_esccmd_p6:
_esccmd_sty:
_esccmd_cl:
        defb  0
_esccmd_p7:
_esccmd_ch:
        defb  0
_esccmd_p8:
_esccmd_d:
_esccmd_cr:
        defb  0
_esccmd_p9:
_esccmd_pd:
        defb  0
_esccmd_f:
        defb  0


__VideoVariables:
        defb  22, 0
        defb  23, 0
        defb  24, 40
        defb  25, 25
        defb  0

end:     defb    0


        INCLUDE "crt/classic/crt_runtime_selection.inc"

        INCLUDE "crt/classic/crt_section.inc"
