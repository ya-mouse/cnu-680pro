/*
 *  linux/arch/arm/kernel/setup.c
 *
 *  Copyright (C) 1995-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/utsname.h>
#include <linux/blk.h>
#include <linux/console.h>
#include <linux/bootmem.h>
#include <linux/init.h>
#include <linux/seq_file.h>

#include <asm/elf.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/procinfo.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>

#ifndef MEM_SIZE

//#define MEM_SIZE	(64*1024*1024)
//#define MEM_SIZE	(8*1024*1024)
#define MEM_SIZE (END_MEM-PAGE_OFFSET)	// FIXME

#endif

#ifndef CONFIG_CMDLINE
#define CONFIG_CMDLINE "root=/dev/rom0"
//#define CONFIG_CMDLINE "root=/dev/mtdblock3 ro"  //coreyliu 20040206
#endif
  
#ifdef CONFIG_ARCH_NETARM
#include <asm/arch/netarm_registers.h>
#include <asm/arch/netarm_mmap.h>
#include <asm/arch/netarm_nvram.h>

extern void _netarm_led_blink(void);
#endif
 
extern void paging_init(struct meminfo *, struct machine_desc *desc);
extern void bootmem_init(struct meminfo *);
extern void reboot_setup(char *str);
extern unsigned long long memparse(char *ptr, char **retptr);
extern unsigned long _end_kernel;

extern int root_mountflags;
extern int _stext, _text, _etext, _edata, _end;

unsigned int processor_id;
unsigned int compat;
unsigned int __machine_arch_type;
unsigned int system_rev;
unsigned int system_serial_low;
unsigned int system_serial_high;
unsigned int mem_fclk_21285 = 50000000;
unsigned int elf_hwcap;

#ifdef MULTI_CPU
struct processor processor;
#endif
  
struct drive_info_struct { char dummy[32]; } drive_info;

struct screen_info screen_info = {
 orig_video_lines:	30,
 orig_video_cols:	80,
 orig_video_mode:	0,
 orig_video_ega_bx:	0,
 orig_video_isVGA:	1,
 orig_video_points:	8
};

unsigned char aux_device_present;
char elf_platform[ELF_PLATFORM_SIZE];
char saved_command_line[COMMAND_LINE_SIZE];

static struct meminfo meminfo __initdata = { 0, };
static struct proc_info_item proc_info;
static const char *machine_name;
static char command_line[COMMAND_LINE_SIZE] = "root=/dev/rom0";

static char default_command_line[COMMAND_LINE_SIZE] __initdata = CONFIG_CMDLINE;
static union { char c[4]; unsigned long l; } endian_test __initdata = { { 'l', '?', '?', 'b' } };
#define ENDIANNESS ((char)endian_test.l)

/*
 * Standard memory resources
 */
static struct resource mem_res[] = {
	{ "Video RAM",   0,     0,     IORESOURCE_MEM			},
	{ "Kernel code", 0,     0,     IORESOURCE_MEM			},
	{ "Kernel data", 0,     0,     IORESOURCE_MEM			}
};

#define video_ram   mem_res[0]
#define kernel_code mem_res[1]
#define kernel_data mem_res[2]

static struct resource io_res[] = {
	{ "reserved",    0x3bc, 0x3be, IORESOURCE_IO | IORESOURCE_BUSY },
	{ "reserved",    0x378, 0x37f, IORESOURCE_IO | IORESOURCE_BUSY },
	{ "reserved",    0x278, 0x27f, IORESOURCE_IO | IORESOURCE_BUSY }
};

#define lp0 io_res[0]
#define lp1 io_res[1]
#define lp2 io_res[2]

static void __init setup_processor(void)
{
	extern struct proc_info_list __proc_info_begin, __proc_info_end;
	struct proc_info_list *list;

	/*
	 * locate processor in the list of supported processor
	 * types.  The linker builds this table for us from the
	 * entries in arch/arm/mm/proc-*.S
	 */
	for (list = &__proc_info_begin; list < &__proc_info_end ; list++)
		if ((processor_id & list->cpu_mask) == list->cpu_val)
			break;

	/*
	 * If processor type is unrecognised, then we
	 * can do nothing...
	 */
	if (list >= &__proc_info_end) {
		printk("CPU configuration botched (ID %08x), unable "
		       "to continue.\n", processor_id);
		while (1);
	}

	proc_info = *list->info;

#ifdef MULTI_CPU
	processor = *list->proc;
#endif

	printk("Processor: %s %s revision %d\n",
	       proc_info.manufacturer, proc_info.cpu_name,
	       (int)processor_id & 15);

	sprintf(system_utsname.machine, "%s%c", list->arch_name, ENDIANNESS);
	sprintf(elf_platform, "%s%c", list->elf_name, ENDIANNESS);
	elf_hwcap = list->elf_hwcap;

	cpu_proc_init();
}

static struct machine_desc * __init setup_architecture(unsigned int nr)
{
	extern struct machine_desc __arch_info_begin, __arch_info_end;
	struct machine_desc *list;

	/*
	 * locate architecture in the list of supported architectures.
	 */
	for (list = &__arch_info_begin; list < &__arch_info_end; list++)
		if (list->nr == nr)
			break;

	/*
	 * If the architecture type is not recognised, then we
	 * can do nothing...
	 */
	if (list >= &__arch_info_end) {
		printk("Architecture configuration botched (nr %d), unable "
		       "to continue.\n", nr);
		while (1);
	}

	printk("Architecture: %s\n", list->name);
	if (compat)
		printk(KERN_WARNING "Using compatibility code "
			"scheduled for removal in v%d.%d.%d\n",
			compat >> 24, (compat >> 12) & 0x3ff,
			compat & 0x3ff);

	return list;
}

/*
 * Initial parsing of the command line.  We need to pick out the
 * memory size.  We look for mem=size@start, where start and size
 * are "size[KkMm]"
 */
static void __init
parse_cmdline(struct meminfo *mi, char **cmdline_p, char *from)
{
	char c = ' ', *to = command_line;
	int usermem = 0, len = 0;

	for (;;) {
		if (c == ' ' && !memcmp(from, "mem=", 4)) {
			unsigned long start;
			unsigned long long size;

			if (to != command_line)
				to -= 1;

			/*
			 * If the user specifies memory size, we
			 * blow away any automatically generated
			 * size.
			 */
			if (usermem == 0) {
				usermem = 1;
				mi->nr_banks = 0;
			}

			start = PHYS_OFFSET;
			size  = memparse(from + 4, &from);
			if (*from == '@')
				start = memparse(from + 1, &from);

			mi->bank[mi->nr_banks].start = start;
			mi->bank[mi->nr_banks].size  = size;
			mi->bank[mi->nr_banks].node  = 0;
			mi->nr_banks += 1;
		}
		c = *from++;
		if (!c)
			break;
		if (COMMAND_LINE_SIZE <= ++len)
			break;
		*to++ = c;
	}
	*to = '\0';
	*cmdline_p = command_line;
}

void __init
setup_ramdisk(int doload, int prompt, int image_start, unsigned int rd_sz)
{
#ifdef CONFIG_BLK_DEV_RAM
#ifdef CONFIG_BLK_DEV_RAMDISK_DATA
	extern void __ramdisk_data;

	rd_image_start	= ((int)&__ramdisk_data) / BLOCK_SIZE ;
#else
	extern int rd_doload, rd_prompt, rd_image_start, rd_size;

	rd_image_start = image_start;
	rd_prompt = prompt;
	rd_doload = doload;

	if (rd_sz)
		rd_size = rd_sz;
#endif
#endif
}

/*
 * initial ram disk
 */
void __init setup_initrd(unsigned int start, unsigned int size)
{
#ifdef CONFIG_BLK_DEV_INITRD
	if (start == 0)
		size = 0;
	initrd_start = start;
	initrd_end   = start + size;
#endif
}

static void __init
request_standard_resources(struct meminfo *mi, struct machine_desc *mdesc)
{
	struct resource *res;
	int i;

	kernel_code.start  = __virt_to_bus(init_mm.start_code);
	kernel_code.end    = __virt_to_bus(init_mm.end_code - 1);
	kernel_data.start  = __virt_to_bus(init_mm.end_code);
	kernel_data.end    = __virt_to_bus(init_mm.brk - 1);

	for (i = 0; i < mi->nr_banks; i++) {
		unsigned long virt_start, virt_end;

		if (mi->bank[i].size == 0)
			continue;

		virt_start = __phys_to_virt(mi->bank[i].start);
		virt_end   = virt_start + mi->bank[i].size - 1;

		res = alloc_bootmem_low(sizeof(*res));
		res->name  = "System RAM";
		res->start = __virt_to_bus(virt_start);
		res->end   = __virt_to_bus(virt_end);
		res->flags = IORESOURCE_MEM | IORESOURCE_BUSY;

		request_resource(&iomem_resource, res);

		if (kernel_code.start >= res->start &&
		    kernel_code.end <= res->end)
			request_resource(res, &kernel_code);
		if (kernel_data.start >= res->start &&
		    kernel_data.end <= res->end)
			request_resource(res, &kernel_data);
	}

	if (mdesc->video_start) {
		video_ram.start = mdesc->video_start;
		video_ram.end   = mdesc->video_end;
		request_resource(&iomem_resource, &video_ram);
	}

	/*
	 * Some machines don't have the possibility of ever
	 * possessing lp0, lp1 or lp2
	 */
	if (mdesc->reserve_lp0)
		request_resource(&ioport_resource, &lp0);
	if (mdesc->reserve_lp1)
		request_resource(&ioport_resource, &lp1);
	if (mdesc->reserve_lp2)
		request_resource(&ioport_resource, &lp2);
}

/*
 *  Tag parsing.
 *
 * This is the new way of passing data to the kernel at boot time.  Rather
 * than passing a fixed inflexible structure to the kernel, we pass a list
 * of variable-sized tags to the kernel.  The first tag must be a ATAG_CORE
 * tag for the list to be recognised (to distinguish the tagged list from
 * a param_struct).  The list is terminated with a zero-length tag (this tag
 * is not parsed in any way).
 */
static int __init parse_tag_core(const struct tag *tag)
{
	if ((tag->u.core.flags & 1) == 0)
		root_mountflags &= ~MS_RDONLY;
	ROOT_DEV = to_kdev_t(tag->u.core.rootdev);
	return 0;
}

static int __init parse_tag_mem32(const struct tag *tag)
{
	if (meminfo.nr_banks >= NR_BANKS) {
		printk(KERN_WARNING
		       "Ignoring memory bank 0x%08x size %dKB\n",
			tag->u.mem.start, tag->u.mem.size / 1024);
		return -EINVAL;
	}
	meminfo.bank[meminfo.nr_banks].start = tag->u.mem.start;
	meminfo.bank[meminfo.nr_banks].size  = tag->u.mem.size;
	meminfo.bank[meminfo.nr_banks].node  = 0;
	meminfo.nr_banks += 1;

	return 0;
}

static int __init parse_tag_videotext(const struct tag *tag)
{
	screen_info.orig_x            = tag->u.videotext.x;
	screen_info.orig_y            = tag->u.videotext.y;
	screen_info.orig_video_page   = tag->u.videotext.video_page;
	screen_info.orig_video_mode   = tag->u.videotext.video_mode;
	screen_info.orig_video_cols   = tag->u.videotext.video_cols;
	screen_info.orig_video_ega_bx = tag->u.videotext.video_ega_bx;
	screen_info.orig_video_lines  = tag->u.videotext.video_lines;
	screen_info.orig_video_isVGA  = tag->u.videotext.video_isvga;
	screen_info.orig_video_points = tag->u.videotext.video_points;
	return 0;
}

static int __init parse_tag_ramdisk(const struct tag *tag)
{
	setup_ramdisk((tag->u.ramdisk.flags & 1) == 0,
		      (tag->u.ramdisk.flags & 2) == 0,
		      tag->u.ramdisk.start, tag->u.ramdisk.size);
	return 0;
}

static int __init parse_tag_initrd(const struct tag *tag)
{
	setup_initrd(tag->u.initrd.start, tag->u.initrd.size);
	return 0;
}

static int __init parse_tag_serialnr(const struct tag *tag)
{
	system_serial_low = tag->u.serialnr.low;
	system_serial_high = tag->u.serialnr.high;
	return 0;
}

static int __init parse_tag_revision(const struct tag *tag)
{
	system_rev = tag->u.revision.rev;
	return 0;
}

static int __init parse_tag_cmdline(const struct tag *tag)
{
	strncpy(default_command_line, tag->u.cmdline.cmdline, COMMAND_LINE_SIZE);
	default_command_line[COMMAND_LINE_SIZE - 1] = '\0';
	return 0;
}

/*
 * This is the core tag table; these are the tags
 * that we recognise for any machine type.
 */
//static const struct tagtable core_tagtable[] __init = {
static const struct tagtable core_tagtable[] = {
	{ ATAG_CORE,      parse_tag_core      },
	{ ATAG_MEM,       parse_tag_mem32     },
	{ ATAG_VIDEOTEXT, parse_tag_videotext },
	{ ATAG_RAMDISK,   parse_tag_ramdisk   },
	{ ATAG_INITRD,    parse_tag_initrd    },
	{ ATAG_SERIAL,    parse_tag_serialnr  },
	{ ATAG_REVISION,  parse_tag_revision  },
	{ ATAG_CMDLINE,   parse_tag_cmdline   }
};

/*
 * Scan one tag table for this tag, and call its parse function.
 */
static int __init
parse_tag(const struct tagtable *tbl, int size, const struct tag *t)
{
	int i;

	for (i = 0; i < size; i++, tbl++)
		if (t->hdr.tag == tbl->tag) {
			tbl->parse(t);
			break;
		}

	return i < size;
}

/*
 * Parse all tags in the list, checking both the global and architecture
 * specific tag tables.
 */
static void __init
parse_tags(const struct tagtable *tbl, int size, const struct tag *t)
{
	/*
	 * The tag list is terminated with a zero-sized tag.  Size is
	 * defined to be in units of 32-bit quantities.
	 */
	for (; t->hdr.size; t = (struct tag *)((u32 *)t + t->hdr.size)) {
		if (parse_tag(core_tagtable, ARRAY_SIZE(core_tagtable), t))
			continue;

		if (tbl && parse_tag(tbl, size, t))
			continue;

		printk(KERN_WARNING
		       "Ignoring unrecognised tag 0x%08x\n", t->hdr.tag);
	}
}

static void __init parse_params(struct param_struct *params)
{
	if (params->u1.s.page_size != PAGE_SIZE) {
		printk(KERN_WARNING "Warning: bad configuration page, "
		       "trying to continue\n");
		return;
	}

	ROOT_DEV	   = to_kdev_t(params->u1.s.rootdev);
	system_rev	   = params->u1.s.system_rev;
	system_serial_low  = params->u1.s.system_serial_low;
	system_serial_high = params->u1.s.system_serial_high;

	if (params->u1.s.mem_fclk_21285 > 0)
		mem_fclk_21285 = params->u1.s.mem_fclk_21285;  

	setup_ramdisk((params->u1.s.flags & FLAG_RDLOAD) == 0,
		      (params->u1.s.flags & FLAG_RDPROMPT) == 0,
		      params->u1.s.rd_start,
		      params->u1.s.ramdisk_size);

	setup_initrd(params->u1.s.initrd_start,
		     params->u1.s.initrd_size);

	if (!(params->u1.s.flags & FLAG_READONLY))
		root_mountflags &= ~MS_RDONLY;

	strncpy(default_command_line, params->commandline, COMMAND_LINE_SIZE);
	default_command_line[COMMAND_LINE_SIZE - 1] = '\0';

	if (meminfo.nr_banks == 0) {
		meminfo.nr_banks      = 1;
		meminfo.bank[0].start = PHYS_OFFSET;
		meminfo.bank[0].size  = params->u1.s.nr_pages << PAGE_SHIFT;
	}
}


/*
 * Tell the kernel about any console devices we may have,  the user
 * can use bootargs select which one they get.  The default will be
 * the console you register first.
 */



void __init setup_arch(char **cmdline_p)
{
	struct param_struct *params = NULL;
	struct machine_desc *mdesc;
	char *from = default_command_line;
	int bootmap_size;
	unsigned long memory_start = (unsigned long)&_end_kernel;
#ifdef CONFIG_ARCH_CNXT
	unsigned long memory_end = END_MEM;
#endif
	
	ROOT_DEV = MKDEV(0, 255);

#if defined(CONFIG_BOARD_SMDK2510) | defined(CONFIG_BOARD_2510REF)
	config_BSP();
#endif
		    
		
	setup_processor();
	mdesc = setup_architecture(machine_arch_type);
	machine_name = mdesc->name;

	if (mdesc->soft_reboot)
		reboot_setup("s");

	if (mdesc->param_offset)
		params = (struct param_struct*)
			(phys_to_virt(mdesc->param_offset));

	/*
	 * Do the machine-specific fixups before we parse the
	 * parameters or tags.
	 */
	if (mdesc->fixup)
		mdesc->fixup(mdesc, params, &from, &meminfo);

	if (params) {
		struct tag *tag = (struct tag *)params;

		/*
		 * Is the first tag the CORE tag?  This differentiates
		 * between the tag list and the parameter table.
		 */
		if (tag->hdr.tag == ATAG_CORE)
			parse_tags(mdesc->tagtable, mdesc->tagsize, tag);
		else
			parse_params(params);
	} 

	if (meminfo.nr_banks == 0) {
		meminfo.nr_banks      = 1;
		meminfo.bank[0].start = PAGE_OFFSET;//PHYS_OFFSET;
		meminfo.bank[0].size  = MEM_SIZE;
	}

	init_mm.start_code = (unsigned long) &_text;
	init_mm.end_code   = (unsigned long) &_etext;
	init_mm.end_data   = (unsigned long) &_edata;
	init_mm.brk	   = (unsigned long) &_end;

	memcpy(saved_command_line, from, COMMAND_LINE_SIZE);
	saved_command_line[COMMAND_LINE_SIZE-1] = '\0';
	parse_cmdline(&meminfo, cmdline_p, from);

#if 1
	bootmem_init(&meminfo);
#else
	bootmap_size= init_bootmem_node(
			  NODE_DATA(0),
			  memory_start >> PAGE_SHIFT,
			  PAGE_OFFSET >> PAGE_SHIFT,
			  END_MEM >> PAGE_SHIFT);
 
	free_bootmem(memory_start, END_MEM - memory_start);
	reserve_bootmem(memory_start, bootmap_size);
#endif
	paging_init(&meminfo, mdesc);		// mem_map is set up here! 
	request_standard_resources(&meminfo, mdesc);

	/*
	 * Set up various architecture-specific pointers
	 */
	init_arch_irq = mdesc->init_irq;

#ifdef CONFIG_VT
#if defined(CONFIG_VGA_CONSOLE)
	conswitchp = &vga_con;
#elif defined(CONFIG_DUMMY_CONSOLE)
	conswitchp = &dummy_con;
#endif
#endif
}

int get_cpuinfo(char * buffer)
{
	char *p = buffer;

	p += sprintf(p, "Processor\t: %s %s rev %d (%s)\n",
		     proc_info.manufacturer, proc_info.cpu_name,
		     (int)processor_id & 15, elf_platform);

	p += sprintf(p, "BogoMIPS\t: %lu.%02lu\n",
		     loops_per_jiffy / (500000/HZ),
		     (loops_per_jiffy / (5000/HZ)) % 100);

	p += sprintf(p, "Hardware\t: %s\n", machine_name);

	p += sprintf(p, "Revision\t: %04x\n",
		     system_rev);

	p += sprintf(p, "Serial\t\t: %08x%08x\n",
		     system_serial_high,
		     system_serial_low);

	return p - buffer;
}


/*
 *	Get CPU information for use by the procfs.
 */

static int show_cpuinfo(struct seq_file *m, void *v)
{
	seq_printf(m, "Processor\t: %s %s rev %d (%s)\n",
		     proc_info.manufacturer, proc_info.cpu_name,
		     (int)processor_id & 15, elf_platform);

	seq_printf(m, "BogoMIPS\t: %lu.%02lu\n",
		     loops_per_jiffy / (500000/HZ),
		     (loops_per_jiffy / (5000/HZ)) % 100);

	seq_printf(m, "Hardware\t: %s\n", machine_name);

	seq_printf(m, "Revision\t: %04x\n",
		     system_rev);

	seq_printf(m, "Serial\t\t: %08x%08x\n",
		     system_serial_high,
		     system_serial_low);

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

