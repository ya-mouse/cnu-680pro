MEMORY 
	{
	romvec : ORIGIN = 0x04030000, LENGTH = 1k
	flash  : ORIGIN = 0x04030400, LENGTH = 2M - 197k
	eflash : ORIGIN = 0x04200000, LENGTH = 0
	ramvec : ORIGIN = 0x00000000, LENGTH = 1k
	bvec   : ORIGIN = 0x00010000, LENGTH = 1k
	ram    : ORIGIN = 0x00010400, LENGTH = 32M - 65k
	eram   : ORIGIN = 0x02000000, LENGTH = 0
	}

SECTIONS
{
	.fakevec :
	{
	} > romvec
	.rom :
	{
	__rom_start = . ;
	} > flash
	.eflash :
	{
	_flashend = . ;
	} > eflash
	.realvec :
	{
	__ramvec = . ;
	} > ramvec
	.romvec :
	{
	_romvec = . ;
	} > bvec
        .text :
	{
	__ram_start = . ;
	text_start = . ;
        	*(.text)
		*(.text.*)
		*(.rodata)
	. = ALIGN(0x4) ;
		*(.kstrtab)
	. = ALIGN(16);          /* Exception table              */
	__start___ex_table = .;
		*(__ex_table)
	__stop___ex_table = .;

        __start___ksymtab = .;  /* Kernel symbol table          */
		 *(__ksymtab)
	__stop___ksymtab = .;

	_etext = . ;
	__data_rom_start = ALIGN ( 4 ) ;
        } > ram
        .data :
	{
	_sdata = . ;
	__data_start = . ;

        . = ALIGN(0x2000) ;
        *(.data.init_task)
        . = ALIGN(0x2000) ;

        *(.data)
        *(.data.*)      
        *(.setup.init)          
        *(.exitcall.exit)               

        . = ALIGN(4096) ;
        __init_begin = .;
        *(.text.init)
        *(.data.init)
        . = ALIGN(16);
        __setup_start = .;
        *(.setup.init)
        __setup_end = .;
        __initcall_start = .;
        *(.initcall.init)
        . = ALIGN(4) ;
        __initcall_end = .;
        __init_end = .;

	_edata = . ;
	edata = ALIGN( 0x10 ) ;
        } > ram
        .bss :
        {
	_sbss = ALIGN( 0x10 ) ;
	__bss_start = ALIGN( 0x10 ) ;
	__data_end = ALIGN( 0x10 ) ;
	*(.bss)
	*(COMMON)
	_ebss = . ;
	__bss_end = . ;
	end = ALIGN( 0x10 ) ;
	_end = ALIGN( 0x10 ) ;
	} > ram
	.eram :
	{
	__ramend = . ;
	_ramend = . ;
	} > eram
}
