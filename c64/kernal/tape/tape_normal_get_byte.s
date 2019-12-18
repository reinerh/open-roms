
//
// Tape (turbo) helper routine - byte reading
//
// Returns byte in .A, Carry flag set = error
//


#if CONFIG_TAPE_NORMAL


tape_normal_get_byte:

	// Init byte checksum and canary bit
	lda #$01
	sta ROPRTY
	lda #%01111111                     // canary bit is 0
	pha

	// First we need a byte marker - (L,M)
!:
	jsr tape_normal_get_pulse
	cmp #($100 - $95 - $05)
	bcs !-                             // too short for a long pulse
	jsr tape_normal_get_pulse
	bcs !-                             // too short for a medium pulse

	// Now fetch individual bits

tape_normal_get_byte_loop:

	jsr tape_normal_get_bit
	bcs tape_normal_get_byte_error
	beq !+

	// Handle parity bit
	lda #$01
	eor ROPRTY
	sta ROPRTY

	sec
!:                                     // moved bit state from Zero to Carry flag

	pla
	ror                                // put the bit in
	pha

	bcs tape_normal_get_byte_loop      // loop if no canary bit reached

	// Byte retrieved (on stack), now we need to validate the checksum

	jsr tape_normal_get_bit
	bcs tape_normal_get_byte_error
	beq !+
	lda #$01
!:
	eor ROPRTY
	beq tape_normal_get_byte_error

	// Checksum validated succesfully
	
	ldx #$0B
	// Carry already clear

	// FALLTROUGH

tape_normal_get_byte_done:

	pla
	stx VIC_EXTCOL
	rts


tape_normal_get_byte_error:

	ldx #$09
	sec                                // make sure Carry is set to indicate error
	bcs tape_normal_get_byte_done


#endif
