; hyppo_write.s - write512: the mirror of mega65-libc's read512.
;
; DMA-copies 512 bytes from the caller's buffer into the SD sector
; buffer at $FFD6E00, then asks hyppo (trap $1C, writefile) to write
; the CURRENT sector of the open file and advance.  Hyppo bounds the
; write by the file's existing size - the file is overwritten in
; place and never grows, so the save slot must be pre-sized (the
; release ships a zeroed DAGGOR65.SAV).
;
; uint8_t write512(const uint8_t *buf);   /* 1 = ok, 0 = failed */
; Buffer pointer arrives in __rc2/__rc3 (same ABI as read512).

HYPPO_WRITEFILE = $1C
HTRAP00         = $D640

.global write512
.section .text.hyppo_write512,"ax",@progbits
write512:
	; ensure the SD buffer (not the FDC buffer) is mapped
	lda #$80
	tsb $d689

	; patch the source address into the DMA list, then copy the
	; caller's 512 bytes up to the sector buffer at $FFD6E00
	lda __rc2
	sta dmalist_towrite_srcaddr+0
	lda __rc3
	sta dmalist_towrite_srcaddr+1
	lda #$00
	sta $d702
	sta $d704
	lda #>dmalist_copytosectorbuffer
	sta $d701
	lda #<dmalist_copytosectorbuffer
	sta $d705

	; hyppo: write the current sector of the current file
	lda #HYPPO_WRITEFILE
	sta HTRAP00
	clv
	bcs write_ok
	lda #$00
	rts
write_ok:
	lda #$01
	rts

.section .data.hyppo_write_dmalist
dmalist_copytosectorbuffer:
	; MEGA65 enhanced DMA options
	.byte $0A		; request format is F018A
	.byte $80,$00		; source is $00xxxxx (bank 0 RAM)
	.byte $81,$FF		; destination is $FFxxxxx
	.byte $00		; no more options
	; F018A DMA list
	.byte $00		; copy + last request in chain
	.short $0200		; count: 512 bytes
dmalist_towrite_srcaddr:
	.short $0000		; source address (patched)
	.byte $00		; of bank $0
	.short $6E00		; destination $6E00
	.byte $0D		; of bank $D  (= $FFD6E00 with the MB option)
	.short $0000		; modulo (unused)
