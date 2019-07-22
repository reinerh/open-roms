
;; Implemented based on https://www.pagetable.com/?p=1135, https://github.com/mist64/cbmbus_doc

;; Expects command to send in BSOUR; Carry flag set = signal EOI
;; Preserves .X and .Y registers


iec_tx_command:

	;; Store .X and .Y on the stack - preserve them
	txa
	pha
	tya
	pha

	;; Notify all devices that we are going to send a byte
	;; and it is going to be a command (pulled ATN)
	jsr iec_pull_atn_clk_release_data

	;; Give devices time to respond (response is mandatory!)
	jsr iec_wait1ms

	;; Did at least one device respond by pulling DATA?
	lda CIA2_PRA
	and #BIT_CIA2_PRA_DAT_IN ; XXX try to optimize this, move to separate routine
	beq +

	;; No devices present on the bus, so we can immediately return with device not found	
	jmp iec_return_DEVICE_NOT_FOUND
*
	;; At least one device responded, but they are still allowed to stall
	;; (can be busy processing something), we have to wait till they are all
	;; ready (or bored with DOS attack...)
	;; Release back CLK, keep DATA released - to ask devices for status.

	jsr iec_release_clk_data

	;; Common part of iec_txbyte and iec_tx_common - waits for devices
	;; and transmits a byte

	clc ; Carry flag set is used for EOI mark
	jsr iec_tx_common

	;; According to https://www.pagetable.com/?p=1135 there is some complicated and dangerous
	;; flow here, but http://www.zimmers.net/anonftp/pub/cbm/programming/serial-bus.pdf (page 6)
	;; advices to just wait 1ms and check the DATA status
	jsr iec_wait1ms
	lda CIA2_PRA
	and #BIT_CIA2_PRA_DAT_IN ; XXX try to optimize this, move to separate routine
	beq +

	;; XXX possible optimization of the flow above: for many commands it is enough
	;; to wait for the DATA being pulled, as confirmation is expected from
	;; a single device only - but not sure if it's worth the trouble

	jmp iec_return_DEVICE_NOT_FOUND
*
	;; All done
	
	;; XXX All the documentation says we should release ATN at this point, but when I do so
	;; the turnaround mechanism does not work. Why???
	;; jsr iec_wait20us
	;; jsr iec_set_idle

	jmp iec_return_success

