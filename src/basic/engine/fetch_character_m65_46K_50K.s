// #LAYOUT# M65 BASIC_0 #TAKE-HIGH
// #LAYOUT# *   *       #IGNORE

//
// Fetches a single character (depending on version - also skips spaces) - optimized version
//


#if CONFIG_MEMORY_MODEL_46K || CONFIG_MEMORY_MODEL_50K


fetch_character:

	ldy #0

	// Unmap BASIC lower ROM

	lda #$26
	sta CPU_R6510

	// Retrieve value from under ROMs, advance text pointer

	lda (TXTPTR), y
	inw TXTPTR

	// FALLTROUGH

fetch_character_end:

	// Restore memory mapping

	pha
	lda #$27
	sta CPU_R6510
	pla

	rts

fetch_character_skip_spaces:

	ldy #0

	// Unmap BASIC lower ROM

	lda #$26
	sta CPU_R6510

	// Retrieve value from under ROMs, advance text pointer
!:
	lda (TXTPTR), y
	inw TXTPTR

	// Skip space characters

	cmp #$20
	beq !-

	// Restore memory mapping

	bra fetch_character_end


#endif
