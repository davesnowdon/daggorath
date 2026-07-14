; irq.s - MEGA65 raster IRQ handler: the jiffy clock.
;
; Hooked through the kernal's $0314 RAM vector.  Both the closed ROM
; and open-roms push A, X, Y before JMP ($0314), so this handler ends
; by unwinding those pushes and RTI-ing itself - the ROM's own IRQ
; work (cursor blink, jiffy at $A0, keyboard scan) never runs.
;
; PAL 50 Hz frames carry 60 Hz jiffies with the same 6/5 accumulator
; as the Spectrum Next ISR: +1,+1,+1,+1,+2 = 60 jiffies / 50 frames.
; NTSC (isr_sixty != 0) counts 1:1.
;
; isr_jiffies/isr_phase/isr_sixty are defined in plat_mega65.c.

.text
.global irq_handler

irq_handler:
	lda #$ff
	sta $d019		; ack all VIC interrupt sources
	lda isr_sixty
	bne plus1
	dec isr_phase
	bne plus1
	lda #5			; every 5th 50 Hz frame counts double
	sta isr_phase
	lda isr_jiffies
	clc
	adc #2
	sta isr_jiffies
	bcc irq_exit
	inc isr_jiffies+1
	jmp irq_exit
plus1:
	inc isr_jiffies
	bne irq_exit
	inc isr_jiffies+1
irq_exit:
	pla			; ROM stub pushed A, X, Y (in that order)
	tay
	pla
	tax
	pla
	rti
