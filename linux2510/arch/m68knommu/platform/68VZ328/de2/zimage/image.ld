STARTUP(crt0.o)
OUTPUT_ARCH(m68k)
SEARCH_DIR(.)
GROUP(-lc -lgcc)
__DYNAMIC  =  0;

/*

 To test the decompression use:

   rom (rwx) : ORIGIN = 0x20000, LENGTH = 2M

 download and run the compressed zImage.bin at 0x20000.
 To generate a flashable compressed image, use:

   rom (rwx) : ORIGIN = <addr_in_flash>, LENGTH = <available space>

 store and run the compressed image at <addr_in_flash>

*/

MEMORY
{
  ram (rwx) : ORIGIN =  0x2000, LENGTH = 128k-8k
  rom (rwx) : ORIGIN = 0x20000, LENGTH = 2M
}

PROVIDE (__stack = 8k - 16);

SECTIONS
{
  .text :
  {
    __text_start = .;
    *(.text)

    . = ALIGN(0x4);
    *(.rodata)

    . = ALIGN(0x4);
    *(.gcc_except_table) 

    . = ALIGN(0x4);
    *(.eh_frame)

    . = ALIGN(0x4);
    *(.lit)

    . = ALIGN(0x4);
    input_data = .;
    image.o
    input_data_end = .;

    . = ALIGN(0x4);
    __text_end = .;
  } > rom

  .data : AT(__text_end)
   {
    . = ALIGN (0x4);
    __data_start = .;

    *(.shdata)
    *(.data)
    *(.gcc_exc)

    . = ALIGN (0x4);
    __data_end = .;
   } > ram

  .bss :
  {
    . = ALIGN(0x4);
    __bss_start = . ;

    *(.shbss)
    *(.bss)
    *(COMMON)

    . = ALIGN(0x4);
    __bss_end = .;
    _end = .;
  } > ram

  .stab 0 (NOLOAD) :
  {
    *(.stab)
  }

  .stabstr 0 (NOLOAD) :
  {
    *(.stabstr)
  }
}
