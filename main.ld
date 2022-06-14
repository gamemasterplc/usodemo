
ENTRY(__start)

MEMORY
{
    RAM (RWX) : ORIGIN = 0x80000400, LENGTH = 4M - 0x400
}

SECTIONS
{
	.text 0x80000400 : {
		*(.start); /* Include startup code */
		/* Include text sections */
		*(.text);
        *(.text.*);
		*(.gnu.linkonce.t.*);
	} > RAM
	
	.rodata : {
		/* Include rodata sections */
		*(.rdata);
		*(.rodata);
		*(.rodata.*);
		*(.gnu.linkonce.r.*);
	} > RAM
	
	. = ALIGN(4);
	
	.ctors : {
		HIDDEN(__ctor_start = .); /* Constructor start marker */
		*(.ctors.*); /* Include priority ctors */
		*(.ctors); /* Include ctors */
		HIDDEN(__ctor_end = .); /* Constructor end marker */
	} > RAM
	
	.data : {
		/* Include data sections */
		*(.data);
		*(.data.*);
		*(.gnu.linkonce.d.*);
		/* Align BSS section start to 8 bytes */
		. = ALIGN(8);
	} > RAM
	
	/* Place filesystem after data section */
	HIDDEN(__fs_rom_start = .);
	/* Calculate ROM address of filesystem */
	__fs_rom_start -= 0x80000400-0x1000;

	.bss (NOLOAD) : {
		/* Declare BSS start symbol */
		HIDDEN(__bss_start = .);
		/* Include common sections as part of BSS */
		*(COMMON);
		*(.scommon*);
		/* Include BSS sections */
		*(.bss);
		*(.bss.*);
		*(.gnu.linkonce.b.*);
		/* Align BSS section end to 8 bytes */
		. = ALIGN(8);
		/* Declare BSS end symbol */
		HIDDEN(__bss_end = .);
	} > RAM
	
	/* Add heap start marker */
	HIDDEN(__heap_start = .);
	
	/* Discard everything not specifically mentioned above. */
	/DISCARD/ :
	{
		*(*);
	}
}