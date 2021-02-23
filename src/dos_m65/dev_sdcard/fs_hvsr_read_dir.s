
;
; Hypervisor virtual filesystem - reading directory
;


; XXX optimize the flow; we do not need 256-byte SD_DIRENT buffer, directory entries can be constructed directly


fs_hvsr_read_dir_open:

	; Open the directory

	lda #$12                          ; dos_opendir
	sta HTRAP00
	+nop

	; XXX handle read errors

	sta SD_DESC                       ; store directory descriptor  XXX invent better name

	; Reset status to OK

	lda #$00
	sta PAR_TRACK
	sta PAR_SECTOR
	jsr util_status_SD

	; Provide pointer to the header

	lda #$20
	sta SD_ACPTR_LEN+0
	lda #$00
	sta SD_ACPTR_LEN+1

	lda #<dir_hdr_sd
	sta SD_ACPTR_PTR+0
	lda #>dir_hdr_sd
	sta SD_ACPTR_PTR+1

	; Set directory phase to 'file name'

	lda #$01
	sta SD_DIR_PHASE

	; End

	jmp dos_EXIT

fs_hvsr_read_dir:

	; Read dirent structure into $1000, process it, restore the memory content.
	; Starting at $1000 VIC sees chargen, so this should be a safe place

	jsr fs_hvsr_direntmem_prepare
	jsr fs_hvsr_util_nextdirentry
	jsr fs_hvsr_direntmem_restore      ; processor status is preserved

	; If nothing to read, output 'blocks free'

	+bcs fs_hvsr_read_dir_blocksfree

	; Check if file name matches the filter

	; XXX debug this
	; jsr util_dir_filter
	; bne fs_hvsr_read_dir               ; if does not match, try the next entry

	; Otherwise, convert the file length from bytes to blocks to display

	jsr util_dir_filesize_bytes

	; Prepare output entry, starting from XX_DIR_ENTRY + initial offset

	ldx #$00                           ; offset for SD card unit

	lda #$01                           ; link to the next line - dummy, will be regenerated by BASIC
	sta XX_DIR_ENTRY, x
	inx
	sta XX_DIR_ENTRY, x
	inx

	lda PAR_FSIZE_BLOCKS+0             ; store number of blocks
	sta XX_DIR_ENTRY, x
	inx
	lda PAR_FSIZE_BLOCKS+1
	sta XX_DIR_ENTRY, x
	inx

	lda #' '                           ; indent file name

@lp1:
	dey
	bmi @lp1_done
	sta XX_DIR_ENTRY, x
	inx
	bra @lp1

@lp1_done:

	lda #$22                           ; opening quote
	sta XX_DIR_ENTRY, x
	inx

	ldy #$FF                           ; put file name

@lp2:

	iny
	lda PAR_FNAME, y
	cmp #$A0
	beq @lp2_done
	cpy #$10
	beq @lp2_done
	sta XX_DIR_ENTRY, x
	inx
	bra @lp2

@lp2_done:

	lda #$22                           ; closing quote
	sta XX_DIR_ENTRY, x
	inx

	ldy #$10                           ; put spaces for indentation

@lp3:

	dey
	lda PAR_FNAME, y
	cmp #$A0
	bne @lp3_done

	lda #' '
	sta XX_DIR_ENTRY, x
	inx
	bra @lp3

@lp3_done:

	lda PAR_FTYPE                      ; put 'damaged' mark if needed
	bmi @closed
	lda #'*'
	+skip_2_bytes_trash_nvz

@closed:

	lda #' '
	sta XX_DIR_ENTRY, x
	inx

	lda PAR_FTYPE                      ; put file type
	and #$3F
	dec
	asl
	asl
	tay

@lp4:
	
	lda dir_types, y
	beq @lp4_done
	sta XX_DIR_ENTRY, x
	inx
	iny
	bra @lp4
	
@lp4_done:

	lda PAR_FTYPE                      ; read-only mark (if needed)
	and #$40
	beq @skipro
	lda #'<'
	sta XX_DIR_ENTRY, x
	inx

@skipro:

	lda #$00                           ; put end of the line
	sta XX_DIR_ENTRY, x
	inx

	stx SD_ACPTR_LEN+0
	sta SD_ACPTR_LEN+1

	lda #<(XX_DIR_ENTRY)
	sta SD_ACPTR_PTR+0
	lda #>(XX_DIR_ENTRY)
	sta SD_ACPTR_PTR+1

	clc
	rts

fs_hvsr_read_dir_blocksfree:

	; Set pointer to 'BLOCKS FREE.' line

	; XXX determine free space

	lda #$13
	sta SD_ACPTR_LEN+0
	lda #$00
	sta SD_ACPTR_LEN+1

	lda #<dir_end
	sta SD_ACPTR_PTR+0
	lda #>dir_end
	sta SD_ACPTR_PTR+1

	; Mark end of directory

	lda #$00
	sta SD_DIR_PHASE

	; Close the directory within the hypervisor  XXX maybe move it to close routine

    ldx SD_DESC
	lda #$16                          ; dos_closedir
	sta HTRAP00
	+nop

	clc
	rts

;
; Helper routines
;

fs_hvsr_direntmem_prepare:

	ldx #$00
@1:
	ldy $1000, x
	sty SD_MEMSHADOW, x
	inx
	bne @1

	rts

fs_hvsr_direntmem_restore:

	php

	ldx #$00
@1:
	ldy SD_MEMSHADOW, x
	sty $1000, x
	inx
	bne @1

	bne @1

	plp
	rts
