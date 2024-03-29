/*
 *  linux/arch/h8300h/kernel/setup.c
 *
 *  Copyleft  ()) 2000       James D. Schettine {james@telos-systems.com}
 *  Copyright (C) 1999,2000  Greg Ungerer (gerg@snapgear.com)
 *  Copyright (C) 1998,1999  D. Jeff Dionne <jeff@lineo.ca>
 *  Copyright (C) 1998       Kenneth Albanowski <kjahds@kjahds.com>
 *  Copyright (C) 1995       Hamish Macdonald
 *  Copyright (C) 2000       Lineo Inc. (www.lineo.com) 
 *  Copyright (C) 2001 	     Lineo, Inc. <www.lineo.com>
 *
 *  H8/300H porting Yoshinori Sato <ysato@users.sourceforge.jp>
 */

/*
 * This file handles the architecture-dependent parts of system setup
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/fb.h>
#include <linux/console.h>
#include <linux/genhd.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/major.h>
#include <linux/bootmem.h>
#include <linux/seq_file.h>

#include <asm/setup.h>
#include <asm/irq.h>

#ifdef CONFIG_BLK_DEV_INITRD
#include <linux/blk.h>
#include <asm/pgtable.h>
#endif

#if defined(CONFIG_CPU_H8300H)
#define CPU "H8/300H"
#endif

unsigned long rom_length;
unsigned long memory_start;
unsigned long memory_end;

struct task_struct *_current_task;

char command_line[512];
char saved_command_line[512];

extern int _stext, _etext, _sdata, _edata, _sbss, _ebss, _end;
extern int _ramstart, _ramend;
extern char _target_name[];

#if defined(CONFIG_GDB_EXEC) && defined(CONFIG_GDB_MAGICPRINT)
/* printk with gdb service */
static void gdb_console_output(struct console *c, char *msg, unsigned len)
{
	for (; len > 0; len--) {
		asm("mov.w %0,r2\n\t"
                    "jsr @0xc4"::"r"(*msg++):"er2");
	}
}

static kdev_t gdb_console_device(struct console *c)
{
    	/* TODO: this is totally bogus */
	/* return MKDEV(SCI_MAJOR, SCI_MINOR_START + c->index); */
	return 0;
}

/*
 *	Setup initial baud/bits/parity. We do two things here:
 *	- construct a cflag setting for the first rs_open()
 *	- initialize the serial port
 *	Return non-zero if we didn't find a serial port.
 */
static int __init gdb_console_setup(struct console *co, char *options)
{
	return 0;
}

static const struct console gdb_console = {
	name:		"gdb",
	write:		gdb_console_output,
	device:		gdb_console_device,
	setup:		gdb_console_setup,
	flags:		CON_PRINTBUFFER,
	index:		-1,
};
#endif

void setup_arch(char **cmdline_p)
{
	int bootmap_size;

	memory_start = PAGE_ALIGN((unsigned long)(&_ramstart));
	memory_end = &_ramend; /* by now the stack is part of the init task */

	init_mm.start_code = (unsigned long) &_stext;
	init_mm.end_code = (unsigned long) &_etext;
	init_mm.end_data = (unsigned long) &_edata;
	init_mm.brk = (unsigned long) 0; 

#if defined(CONFIG_GDB_EXEC) && defined(CONFIG_GDB_MAGICPRINT)
	register_console(&gdb_console);
#endif

	printk("\x0F\r\n\nuClinux " CPU "\n");
	printk("Target Hardware: %s\n",_target_name);
	printk("H8/300H support by Yoshinori Sato <ysato@users.sourceforge.jp>\n");

	printk("Flat model support (C) 1998,1999 Kenneth Albanowski, D. Jeff Dionne\n");


#ifdef DEBUG
	printk("KERNEL -> TEXT=0x%06x-0x%06x DATA=0x%06x-0x%06x "
		"BSS=0x%06x-0x%06x\n", (int) &_stext, (int) &_etext,
		(int) &_sdata, (int) &_edata,
		(int) &_sbss, (int) &_ebss);
	printk("KERNEL -> ROMFS=0x%06x-0x%06x MEM=0x%06x-0x%06x "
		"STACK=0x%06x-0x%06x\n",
	       (int) &_ebss, (int) memory_start,
		(int) memory_start, (int) memory_end,
		(int) memory_end, (int) &_ramend);
#endif

#ifdef CONFIG_BLK_DEV_BLKMEM
	ROOT_DEV = MKDEV(BLKMEM_MAJOR,0);
#endif

#ifdef CONFIG_DEFAULT_CMDLINE
	/* set from default command line */
	if (*command_line == '\0')
		strcpy(command_line,CONFIG_KERNEL_COMMAND);
#endif
	/* Keep a copy of command line */
	*cmdline_p = &command_line[0];
	memcpy(saved_command_line, command_line, sizeof(saved_command_line));
	saved_command_line[sizeof(saved_command_line)-1] = 0;

#ifdef DEBUG
	if (strlen(*cmdline_p)) 
		printk("Command line: '%s'\n", *cmdline_p);
#endif

	/*
	 * give all the memory to the bootmap allocator,  tell it to put the
	 * boot mem_map at the start of memory
	 */
	bootmap_size = init_bootmem_node(
			NODE_DATA(0),
			memory_start >> PAGE_SHIFT, /* map goes here */
			PAGE_OFFSET >> PAGE_SHIFT,	/* 0 on coldfire */
			memory_end >> PAGE_SHIFT);
	/*
	 * free the usable memory,  we have to make sure we do not free
	 * the bootmem bitmap so we then reserve it after freeing it :-)
	 */
	free_bootmem(memory_start, memory_end - memory_start);
	reserve_bootmem(memory_start, bootmap_size);
	/*
	 * get kmalloc into gear
	 */
	paging_init();
#ifdef DEBUG
	printk("Done setup_arch\n");
#endif
}

int get_cpuinfo(char * buffer)
{
    char *cpu;
    u_long clockfreq;

    cpu = CPU;

    clockfreq = CONFIG_CLK_FREQ;

    return(sprintf(buffer, "CPU:\t\t%s\n"
		   "Clock:\t%lu.%1luMHz\n"
		   "BogoMips:\t%lu.%02lu\n"
		   "Calibration:\t%lu loops\n",
		   cpu,
		   clockfreq/100,clockfreq%100,
		   (loops_per_jiffy*HZ)/500000,((loops_per_jiffy*HZ)/5000)%100,
		   (loops_per_jiffy*HZ)));

}

/*
 *	Get CPU information for use by the procfs.
 */

static int show_cpuinfo(struct seq_file *m, void *v)
{
    char *cpu;
    u_long clockfreq;

    cpu = CPU;

    clockfreq = CONFIG_CLK_FREQ;

    seq_printf(m,  "CPU:\t\t%s\n"
		   "Clock:\t%lu.%1luMHz\n"
		   "BogoMips:\t%lu.%02lu\n"
		   "Calibration:\t%lu loops\n",
		   cpu,
		   clockfreq/100,clockfreq%100,
		   (loops_per_jiffy*HZ)/500000,((loops_per_jiffy*HZ)/5000)%100,
		   (loops_per_jiffy*HZ));

    return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < NR_CPUS ? ((void *) 0x12345678) : NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return c_start(m, pos);
}

static void c_stop(struct seq_file *m, void *v)
{
}

struct seq_operations cpuinfo_op = {
	start:	c_start,
	next:	c_next,
	stop:	c_stop,
	show:	show_cpuinfo,
};
