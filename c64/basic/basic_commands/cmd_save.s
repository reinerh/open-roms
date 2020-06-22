// #LAYOUT# STD *       #TAKE
// #LAYOUT# *   BASIC_0 #TAKE
// #LAYOUT# *   *       #IGNORE


cmd_save:

	// Set default device and secondary address; channel is not important now

	jsr select_device
	ldy #$00
	jsr JSETFLS

	// Fetch the file name

	jsr helper_load_fetch_filename
	bcs_16 do_MISSING_FILENAME_error

	// Try to fetch device

	jsr fetch_device_secondary

	// Setup the start and end addresses

	lda #TXTTAB

	// Setup the end address

	ldx VARTAB+0
	ldy VARTAB+1

	// Perform saving

	jsr JSAVE
	bcs_16 do_kernal_error

	// The end

	jmp execute_statements
