; Function defined on pp272-273 of C64 Programmers Reference Guide

untlk:

	;; This trivial routine is documented in 'Compute's Mapping the Commodore 64', page 224
	lda #$5F
	jmp iec_tx_byte ;; XXX add special routine for commands
