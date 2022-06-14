SECTIONS
{
	.text : {
		*(.text);
        *(.text.*);
		*(.gnu.linkonce.t.*);
	}
	
	.rodata : {
		*(.rdata);
		*(.rodata);
		*(.rodata.*);
		*(.gnu.linkonce.r.*);
	}
	
	. = ALIGN(4);
	
	.ctors : {
		*(.ctors.*); /* Include priority ctors */
		*(.ctors);  /* Include ctors */
	}
	
	.dtors : {
		*(.dtors.*); /* Include priority dtors */
		*(.dtors); /* Include dtors */
	}
	
	.data : {
		*(.data);
		*(.data.*);
		*(.gnu.linkonce.d.*);
	}

	.bss (NOLOAD) : {
		*(COMMON);
		*(.scommon*);
		*(.bss);
		*(.bss.*);
		*(.gnu.linkonce.b.*);
	}
	
	/* Discard everything not specifically mentioned above. */
	/DISCARD/ :
	{
		*(*);
	}
}