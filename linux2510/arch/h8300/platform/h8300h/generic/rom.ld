OUTPUT_ARCH(h8300h)
ENTRY("__start")

INPUT(rootimage.o)

MEMORY 
	{
	vector : ORIGIN = 0x000000, LENGTH = 0x000100
	rom    : ORIGIN = 0x000100, LENGTH = 0x200000-0x000100
	erom   : ORIGIN = 0x200000, LENGTH = 1
	ram    : ORIGIN = 0x200000, LENGTH = 0x100000
	eram   : ORIGIN = 0x2fa000, LENGTH = 1
	}

SECTIONS
{
	.vectors :
	{
	__vector = . ;
		*(.vectors*)
	} > vector
        .text :
	{
		*(.int_redirect)
	__stext = . ;
        	*(.text)
	. = ALIGN(0x4) ;
		*(.text.*)
	. = ALIGN(0x4) ;
		*(.exitcall.exit)
	. = ALIGN(0x4) ;
		*(.kstrtab)
	. = ALIGN(0x4) ;
		*(.rodata*)
	. = ALIGN(16);          /* Exception table              */
	___start___ex_table = .;
		*(__ex_table)
	___stop___ex_table = .;

        ___start___ksymtab = .;  /* Kernel symbol table          */
		 *(__ksymtab)
	___stop___ksymtab = .;

	. = ALIGN(0x4) ;
	__etext = . ;
	} > rom
	.data : AT( ADDR(.text)+SIZEOF(.text))
	{
	__sdata = . ;
	___data_start = . ;

	. = ALIGN(0x4) ;
		*(.data)
	. = ALIGN(0x4) ;
		*(.data.*)	

	. = ALIGN(0x4) ;
	___setup_start = .;
		*(.setup.init)
	. = ALIGN(0x4) ;
	___setup_end = .;

	. = ALIGN(0x4) ;
	___initcall_start = .;
		*(.initcall.init)
	. = ALIGN(0x4) ;
	___initcall_end = .;

	. = ALIGN(0x2000) ;
		*(.data.init_task)
	. = ALIGN(0x2000) ;
	__edata = . ;
	} > ram
	__begin_data = LOADADDR(.data) ;
	.blkimg : AT( LOADADDR(.data) + SIZEOF(.data))
	{
	__blkimg = . ;
		*(.rootimg*)
	} > rom
	.erom :
	{
	__erom = . ;
	} > erom
        .bss :
        {
	. = ALIGN(0x4) ;
	__sbss = . ;
		*(.bss)
	. = ALIGN(0x4) ;
		*(COMMON)
	. = ALIGN(0x4) ;
	__ebss = . ;
	__end = . ;
	__ramstart = .;
	} > ram
        .dummy :
        {
        COMMAND_START = . - 0x200 ;
	__ramend = . ;
	} > eram
}
