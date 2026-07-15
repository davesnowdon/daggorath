; layout.s - absolute placement of the big buffers in low RAM, outside
; the llvm-mos link region ($2001-$CFFF), which the core's code + data
; + save buffer fill almost completely.
;
;   $0200-$03FF  kernal vectors ($0314!), monitor bootstrap stubs
;   $0400-$05FF  save_bounce: plat_load_state's partial-sector bounce
;   $0800-$1FFF  fb: the 6144-byte shadow framebuffer (ends AT $2000,
;                flush against the PRG's load address)
;
; Neither is zeroed by crt0 (they are not in .bss): plat_init clears
; fb explicitly and save_bounce needs no init.

.global fb
fb = 0x0800

.global save_bounce
save_bounce = 0x0400
