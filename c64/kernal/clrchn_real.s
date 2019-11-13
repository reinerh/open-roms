
//
// Official Kernal routine, described in:
//
// - [RG64] C64 Programmer's Reference Guide   - page 282
// - [CM64] Compute's Mapping the Commodore 64 - page 230
//
// CPU registers that has to be preserved (see [RG64]): .Y
//


clrchn_real:

	// Handle IEC input device
	lda DFLTN
	jsr iec_check_devnum
	bcs !+
	// Previous device was IEC one - send UNTALK first
	jsr UNTLK
!:
	// Restore output device to default
	lda DFLTO
	jsr iec_check_devnum
	bcs !+
	// Handle IEC output device
	jsr UNLSN
!:
clrchn_reset: // entry needed by setup_vicii
	// Set input device number to keyboard
	lda #$00
	sta DFLTN
	// Set output device number to screen
	lda #$03
	sta DFLTO

	rts