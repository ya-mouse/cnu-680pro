/* $Id: vmlinux.lds,v 1.1.1.1 2003/11/17 02:33:22 jipark Exp $
 * ld script to make SuperH Linux kernel
 * Written by Niibe Yutaka
 */



/*
 * Automatically generated C config: don't edit
 */






/*
 * Code maturity level options
 */


/*
 * Loadable module support
 */




/*
 * Processor type and features
 */




































/*
 * General setup
 */

























/*
 * Parallel port support
 */


/*
 * Memory Technology Devices (MTD)
 */







/*
 * User Modules And Translation Layers
 */






/*
 * RAM/ROM/Flash chip drivers
 */














/*
 * Mapping drivers for chip access
 */


















/*
 * Self-contained MTD device drivers
 */





/*
 * Disk-On-Chip Device Drivers
 */






/*
 * NAND Flash Device Drivers
 */


/*
 * Block devices
 */

















/*
 * Multi-device support (RAID and LVM)
 */









/*
 * Networking options
 */























/*
 *   IP: Netfilter Configuration
 */






















































/*
 *  
 */



/*
 * Appletalk devices
 */












/*
 * QoS and/or fair queueing
 */

























/*
 * Network testing
 */


/*
 * ATA/IDE/MFM/RLL support
 */




/*
 * SCSI support
 */


/*
 * IEEE 1394 (FireWire) support (EXPERIMENTAL)
 */


/*
 * Network device support
 */


/*
 * ARCnet devices
 */







/*
 * Ethernet (10 or 100Mbit)
 */




















































/*
 * Ethernet (1000 Mbit)
 */

























/*
 * Wireless LAN (non-hamradio)
 */


/*
 * Token Ring devices
 */





/*
 * Wan interfaces
 */


/*
 * Old CD-ROM drivers (not SCSI, not IDE)
 */


/*
 * Input core support
 */






/*
 * Character devices
 */





/*
 * Unix 98 PTY support
 */


/*
 * Joysticks
 */


/*
 * Input core support is needed for gameports
 */

/*
 * Input core support is needed for joysticks
 */


/*
 * Watchdog Cards
 */




/*
 * File systems
 */




















































/*
 * Network File Systems
 */























/*
 * Partition Types
 */




/*
 * Multimedia devices
 */


/*
 * Sound
 */


/*
 * USB support
 */


/*
 * Kernel hacking
 */




/*
 * Library routines
 */





OUTPUT_FORMAT("elf32-sh-linux", "elf32-sh-linux", "elf32-sh-linux")

OUTPUT_ARCH(sh)
ENTRY(_start)
SECTIONS
{
  . = 0x80000000 + 0x08000000 + 0x1000;
  _text = .;			/* Text and read-only data */
  text = .;			/* Text and read-only data */
  .empty_zero_page : {
	*(.empty_zero_page)
	} = 0
  .text : {
	*(.text)
	*(.fixup)
	*(.gnu.warning)
	} = 0x0009
  .rodata : { *(.rodata) *(.rodata.*) }
  .kstrtab : { *(.kstrtab) }

  . = ALIGN(16);		/* Exception table */
  __start___ex_table = .;
  __ex_table : { *(__ex_table) }
  __stop___ex_table = .;

  __start___ksymtab = .;	/* Kernel symbol table */
  __ksymtab : { *(__ksymtab) }
  __stop___ksymtab = .;

  __start___kallsyms = .;	/* All kernel symbols */
  __kallsyms : { *(__kallsyms) }
  __stop___kallsyms = .;

  _etext = .;			/* End of text section */

  .data : {			/* Data */
	*(.data)
	CONSTRUCTORS
	}

  _edata = .;			/* End of data section */

  . = ALIGN(8192);		/* init_task */
  .data.init_task : { *(.data.init_task) }
  /* stack */
  .stack : { stack = .;  _stack = .; }

  . = ALIGN(4096);		/* Init code and data */
  __init_begin = .;
  .text.init : { *(.text.init) }
  .data.init : { *(.data.init) }
  . = ALIGN(16);
  __setup_start = .;
  .setup.init : { *(.setup.init) }
  __setup_end = .;
  __initcall_start = .;
  .initcall.init : { *(.initcall.init) }
  __initcall_end = .;
  __machvec_start = .;
  .machvec.init : { *(.machvec.init) }
  __machvec_end = .;
  . = ALIGN(4096);
  __init_end = .;

  . = ALIGN(4096);
  .data.page_aligned : { *(.data.idt) }


  . = ALIGN(32);

  .data.cacheline_aligned : { *(.data.cacheline_aligned) }

  . = ALIGN(4);
  __bss_start = .;		/* BSS */
  .bss : {
	*(.bss)
	}
  . = ALIGN(4096);
  _end = . ;

  /* When something in the kernel is NOT compiled as a module, the
   * module cleanup code and data are put into these segments.  Both
   * can then be thrown away, as cleanup code is never called unless
   * it's a module.
   */
  /DISCARD/ : {
	*(.text.exit)
	*(.data.exit)
	*(.exitcall.exit)
	}

  /* Stabs debugging sections.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment 0 : { *(.comment) }
  /* DWARF debug sections.
     Symbols in the DWARF debugging section are relative to the beginning
     of the section so we begin .debug at 0.  */
  /* DWARF 1 */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }
  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }
  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }
  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }
  /* These must appear regardless of  .  */
}
