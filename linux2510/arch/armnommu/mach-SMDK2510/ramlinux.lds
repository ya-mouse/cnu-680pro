/* ld script to make ARM Linux kernel
 * taken from the i386 version by Russell King
 * Written by Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 */
INCLUDE arch/armnommu/mach-SMDK2510/romfs.ld 
OUTPUT_ARCH(arm)
ENTRY(stext)
SECTIONS
{

	. = 0x20000;
	.init : {			/* Init code and data		*/
		_stext = .;
		__init_begin = .;
			*(.text.init)
		__proc_info_begin = .;
			*(.proc.info)
		__proc_info_end = .;
		__arch_info_begin = .;
			*(.arch.info)
		__arch_info_end = .;
			*(.data.init)
		. = ALIGN(16);
		__setup_start = .;
			*(.setup.init)
		__setup_end = .;
		__initcall_start = .;
			*(.initcall.init)
		__initcall_end = .;
		. = ALIGN(4096);
		__init_end = .;
	}

	/DISCARD/ : {			/* Exit code and data		*/
		*(.text.exit)
		*(.data.exit)
		*(.exitcall.exit)
	}

	.text : {			/* Real text segment		*/
		_text = .;		/* Text and read-only data	*/
			*(.text)
			*(.fixup)
			*(.gnu.warning)
			*(.text.lock)	/* out-of-line lock text */
			*(.rodata)
			*(.glue_7)
			*(.glue_7t)
			*(.kstrtab)
		. = ALIGN(16);
		__start___ex_table = .;	/* Exception table		*/
			*(__ex_table)
		__stop___ex_table = .;

		__start___ksymtab = .;	/* Kernel symbol table		*/
			*(__ksymtab)
		__stop___ksymtab = .;

		__start___kallsyms = .; /* All kernel symbols           */
			*(__kallsyms)
		__stop___kallsyms = .;


		*(.got)			/* Global offset table		*/

		_etext = .;		/* End of text section		*/
	}

	. = ALIGN(8192);

	__data_start = .;
	__data_rom_start = .;
	.data : {
		/*
		 * first, the init task union, aligned
		 * to an 8192 byte boundary.
		 */
		*(.init.task)

		/*
		 * then the cacheline aligned data
		 */
		. = ALIGN(32);
		*(.data.cacheline_aligned)

		/*
		 * and the usual data section
		 */
		*(.data)
		CONSTRUCTORS

		. = ALIGN(32);	/* To be sure to align .fs . OZH */
		_edata = .;
		
	}

	.fs :	{
		_romfs = ABSOLUTE (.);
		. = . + romfs_size;
		_end_romfs = ABSOLUTE (.);
		}

	.bss : {
		__bss_start = .;	/* BSS				*/
		*(.bss)
		*(COMMON)
		__bss_end = .;
		_end = . ;
	}

	. = ALIGN(8192);
	_end_kernel = . ;

					/* Stabs debugging sections.	*/
	.stab 0 : { *(.stab) }
	.stabstr 0 : { *(.stabstr) }
	.stab.excl 0 : { *(.stab.excl) }
	.stab.exclstr 0 : { *(.stab.exclstr) }
	.stab.index 0 : { *(.stab.index) }
	.stab.indexstr 0 : { *(.stab.indexstr) }
	.comment 0 : { *(.comment) }
}
