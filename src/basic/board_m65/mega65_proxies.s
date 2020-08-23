// #LAYOUT# M65 BASIC_0 #TAKE
// #LAYOUT# *   *       #IGNORE

//
// Proxies for calling Mega65 segment BASIC_0 and KERNAL_0 routines from BASIC_1
//


proxy_B1_peek_via_OLDTXT:

	jsr map_NORMAL
#if CONFIG_MEMORY_MODEL_60K
	.error "not implemented"
#elif CONFIG_MEMORY_MODEL_46K || CONFIG_MEMORY_MODEL_50K
	jsr peek_under_roms_via_OLDTXT
	cmp #$00
#else // CONFIG_MEMORY_MODEL_38K
	lda (OLDTXT),y
#endif
	jmp map_BASIC_1

proxy_B1_poke_via_OLDTXT:

	jsr map_NORMAL
#if CONFIG_MEMORY_MODEL_60K
	.error "not implemented"
#else // CONFIG_MEMORY_MODEL_38K || CONFIG_MEMORY_MODEL_46K || CONFIG_MEMORY_MODEL_50K
	sta (OLDTXT),y
#endif
	jmp map_BASIC_1

proxy_B1_JCLALL:

	jsr map_NORMAL
	jsr JCLALL
	jmp map_BASIC_1

#if CONFIG_DOS_WEDGE

proxy_B1_JCLOSE:

	jsr map_NORMAL
	jsr JCLOSE
	jmp map_BASIC_1


proxy_B1_JOPEN:

	jsr map_NORMAL
	jsr JOPEN
	jmp map_BASIC_1

#endif

proxy_B1_JCHROUT:

	jsr map_NORMAL
	jsr JCHROUT
	jmp map_BASIC_1

proxy_B1_print_hex_byte:

	jsr map_NORMAL
	jsr print_hex_byte
	jmp map_BASIC_1

proxy_B1_print_integer:

	jsr map_NORMAL
	jsr print_integer
	jmp map_BASIC_1


proxy_B1_print_return:

	jsr map_NORMAL
	jsr print_return
	jmp map_BASIC_1


proxy_B1_print_space:

	jsr map_NORMAL
	jsr print_space
	jmp map_BASIC_1

proxy_B1_plot_set:

	jsr map_NORMAL
	jsr plot_set
	jmp map_BASIC_1

proxy_B1_M65_SETWIN_Y:

	jsr map_NORMAL
	jsr M65_SETWIN_Y
	jmp map_BASIC_1

#if CONFIG_DOS_WEDGE

proxy_B1_fetch_character:

	jsr map_NORMAL
	jsr fetch_character
	jmp map_BASIC_1

proxy_B1_fetch_character_skip_spaces:

	jsr map_NORMAL
	jsr fetch_character_skip_spaces
	jmp map_BASIC_1

proxy_B1_fetch_uint8:

	jsr map_NORMAL
	jsr fetch_uint8
	jmp map_BASIC_1

proxy_B1_shell_main_loop:

	jsr map_NORMAL
	jmp shell_main_loop

#endif
