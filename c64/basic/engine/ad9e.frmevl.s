// #LAYOUT# STD *       #TAKE
// #LAYOUT# X16 BASIC_0 #TAKE-OFFSET 2000
// #LAYOUT# *   BASIC_0 #TAKE
// #LAYOUT# *   *       #IGNORE


// Well-known BASIC routine, described in:
//
// - [CM64] Computes Mapping the Commodore 64 - page 102
// - https://sta.c64.org/cbm64basconv.html
//
// Output:
// - VALTYP - $00 = floating number, $FF = string
// - INTFLG (XXX float/integer, probably)
// - for float: FAC1
// - for string: __FAC1 + 0 = length, __FAC1 + 1 = pointer to string
//
// Idea:
// - use a stack to reorder operations, so that they can be executed
//   in a correct order
//

// XXX finish the implementation


FRMEVL:

	// Put the sentinel to the stack

	lda #$00
	pha

	// FALLTROUGH

FRMEVL_loop:

	// Check if end of statement

	jsr end_of_statement_check
	bcs_16 do_SYNTAX_error

	// There are 4 basic possibilities: a string, a float, an opening bracket, or a token

	jsr fetch_character
	cmp #$22                           // check for opening quote
	beq FRMEVL_fetch_string
	cmp #$28                           // check for opening bracket
	beq_16 do_NOT_IMPLEMENTED_error    // XXX
	cmp #$7F                           // check for a token
	bcc FRMEVL_fetch_float

	// Search for a value or unary operator, starting from the most probable
	// XXX check for BASIC functions here, too

	cmp #$AB                           // check for unary minus
	beq_16 do_NOT_IMPLEMENTED_error    // XXX
	cmp #$A8                           // check for NOT
	beq_16 do_NOT_IMPLEMENTED_error    // XXX
	cmp #$FF                           // check for PI
	beq_16 do_NOT_IMPLEMENTED_error    // XXX
	cmp #$AA                           // check for unary plus (ignore this one)    
	beq FRMEVL_loop

	// Nothing recognized

	jmp do_SYNTAX_error

FRMEVL_fetch_float:

	// XXX implement this

	jmp do_SYNTAX_error

FRMEVL_fetch_string:

	// Mark return value as a string, set initial length as 0

	ldx #$FF
	stx VALTYP

	inx
	stx __FAC1 + 0

	// Set pointer to start of the string

	ldx TXTPTR + 0
	stx __FAC1 + 1
	ldx TXTPTR + 1
	stx __FAC1 + 2
!:
	// Search for closing quote

	jsr fetch_character
	cmp #$22
	beq FRMEVL_got_value

	inc __FAC1 + 0
	bne !-

	jmp do_STRING_TOO_LONG_error

FRMEVL_got_value:

	jsr end_of_statement_check
	bcs FRMEVL_calculate

	// FALLTROUGH

FRMEVL_fetch_operator:

	// We have a value, but this is not the end of expression. We expect
	// a two argument operator here. First make sure we have enough space
	// on a stack - we will surely need it

	tsx
	cpx #$40                           // XXX is this a safe threshold?
	bcc_16 do_FORMULA_TOO_COMPLEX_error

	// Now look for an operator
	// XXX for now only plus operator is implemented

	jsr fetch_character
	cmp #$AA                           // check for a plus operator 
	beq_16 FRMEVL_push_value_operator

	// Something unknown - consider this the end of expression

	// FALTROUGH

FRMEVL_calculate:                      // this is the exit point for the operators

	// Now, we have to calculate all the operations; at this point we
	// should have a value in FAC1 and (possibly) operator on the stack

	pla
	beq FRMEVL_done                    // branch if sentinel

	// Priority can be ignored now - just jump to the operator

	// FALLTROUGH

FRMEVL_done:

	rts

FRMEVL_push_value_operator:

	// First push the value in FAC1/ARG1

	ldx VALTYP
	bmi FRMEVL_push_string

	// Push float

	// XXX implement this

FRMEVL_push_string:

	// Push string - address, length, type

	lda __FAC1+1
	pha
	lda __FAC1+2
	pha

	lda __FAC1+0
	pha

	txa
	pha

FRMEVL_push_operator_address:

	// Push the operator address to stack, in a form
	// suitable for RTS

	// XXX add support for other operators

	lda #>(oper_plus - 1)
	pha
	lda #<(oper_plus - 1)
	pha

	// Push operator priority

	lda #$01                           // XXX add support for other operators
	pha

	// Continue

	jmp FRMEVL_loop
