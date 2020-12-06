;; #LAYOUT# STD *        #TAKE
;; #LAYOUT# *   KERNAL_0 #TAKE
;; #LAYOUT# *   *        #IGNORE


default_brk_handler:

	; Implemented according to Computes Mapping the Commodore 64, pages 73-74

	sei ; disable IRQs, to be sure they wont interfere

!ifdef CONFIG_PLATFORM_COMMODORE_64 {

	ldx #$00
	sta VIC_SCROLX           ; turn the display off - we want as little screen artifacts as possible
}

	cld                      ; make sure this dangerous flag is disabled

!ifdef HAS_OPCODES_65CE02 {

	see                      ; disable extended stack
}

!ifdef CONFIG_MB_M65 {

	; Make sure we have normal memory mapping

	jsr map_NORMAL

	lda VIC_CTRLA
	and #%01000111
	sta VIC_CTRLA
}

	jsr JRESTOR
	jsr JIOINIT

	; Original routine calls just a part of CINT - but I am not sure which one. Most likely it
	; skips the PAL/NTSC check by calling $E518. I suspect bug in the original ROM, lets
	; call the whole CINT, just to be sure everything is initialized.

	jsr CINT

	cli

	jmp (IBASIC_WARM_START)
