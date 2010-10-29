/*
 *  linux/arch/arm/mm/arm940T.S: Protection Unit (PU) functions for ARM940T
 *
 *  Copyright (C) 2002,2003 Arcturus Networks Inc.
 *  Copyright (C) 2002,2003 Oleksandr Zhadan <oleks@arcturusnetworks.com>
 *
 */

#include <linux/linkage.h>
#include <linux/config.h>
#include <asm/assembler.h>
#include <asm/constants.h>
#include <asm/procinfo.h>
#include <asm/hardware.h>

#ifdef	CONFIG_UNCACHED_MEM

#include <asm/arch/memory.h>

#define REGION_SIZE_512KB       (0x12<<1)
#define REGION_SIZE_1MB         (0x13<<1)
#define REGION_SIZE_2MB         (0x14<<1)
#define REGION_SIZE_4MB         (0x15<<1)
#define REGION_SIZE_8MB         (0x16<<1)
#define REGION_SIZE_16MB        (0x17<<1)
#define REGION_SIZE_32MB        (0x18<<1)

#if   defined(CONFIG_INITIAL_DMA_REGION_4MB)
#define DMA_REGION_SIZE         REGION_SIZE_4MB
#elif defined(CONFIG_INITIAL_DMA_REGION_2MB)
#define DMA_REGION_SIZE         REGION_SIZE_2MB
#else
#define DMA_REGION_SIZE         REGION_SIZE_1MB
#endif

#if   defined(CONFIG_REGION_2MB)
#define RAM_REGION_SIZE         REGION_SIZE_2MB
#elif defined(CONFIG_REGION_1MB)
#define RAM_REGION_SIZE         REGION_SIZE_1MB
#else
#define RAM_REGION_SIZE         REGION_SIZE_512KB
#endif

#if   DRAM_SIZE==0x00800000
#define RAM_REGION0_SIZE         REGION_SIZE_8MB
#elif DRAM_SIZE==0x01000000
#define RAM_REGION0_SIZE         REGION_SIZE_16MB
#elif DRAM_SIZE==0x02000000
#define RAM_REGION0_SIZE         REGION_SIZE_32MB
#endif

#if   FLASH_SIZE==0x0100000
#define FLASH_REGION_SIZE       REGION_SIZE_1MB
#elif FLASH_SIZE==0x0200000
#define FLASH_REGION_SIZE       REGION_SIZE_2MB
#elif FLASH_SIZE==0x0400000
#define FLASH_REGION_SIZE       REGION_SIZE_4MB
#elif FLASH_SIZE==0x0800000
#define FLASH_REGION_SIZE       REGION_SIZE_8MB
#endif

#endif	/* CONFIG_UNCACHED_MEM */

/*
 * This definitions are not used but it may answer some
 * questions about ARM940T architecture.
 *
 * The size of the I or D cache:
 * #define MAX_CACHE_SIZE	4096
 *
 * the cache line size of the I or D cache:
 * #define CACHELINESIZE	16
 *
 * the segment size:
 * #define SEG_SIZE		1024
 *
 * the number of segments:
 * #define SEG_NUM		4
 *
 * the number of entries
 * #define ENTRIES_NUM		64
 *
 */

	.text

/*
 * cpu_arm940_data_abort()
 *
 * "obtain information about current aborted instruction
 * Note: we read user space.  This means we might cause a data
 * abort here if the I-TLB and D-TLB aren't seeing the same
 * picture.  Unfortunately, this does happen.  We live with it."
 *				Doesn't work for ARM940.   OZH.
 *  Input:
 *  r2 = address of aborted instruction
 *  r3 = cpsr
 *
 * Returns:
 *  r0 = address of aborted instruction
 *  r1 != 0 if writing
 *  r3 = FSR
 *  r4 = corrupted
 */
 
	.align	5
ENTRY(cpu_arm940_data_abort)
	mov	r3, #0			@ FIXME: ARM940 does not have the fault
					@ status register. We set 0 status to
					@ get "Unhandled fault" with SIGKILL
					@ signal if it is not from the kernel.
	mov	r0, r2			@ FAR = real address
	ldr	r1, [r2]		@ read aborted instruction
	and	r3, r3, #255
	tst	r1, r1, lsr #21		@ C = bit 20
	sbc	r1, r1, r1		@ r1 = C - 1
	mov	pc, lr

/*
 * cpu_arm940_check_bugs()
 */
ENTRY(cpu_arm940_check_bugs)
	mrs	ip, cpsr
	bic	ip, ip, #F_BIT
	msr	cpsr, ip
	mov	pc, lr

/*
 * cpu_arm940_proc_init()
 */
ENTRY(cpu_arm940_proc_init)
#ifdef	CONFIG_UNCACHED_MEM
        /* Disable D-cache */
        mrc     p15, 0, r0, c1, c0, 0
        bic     r0, r0, #0x0004                                         @ disable D-cache
	bic	r0, r0, #0x1000						@ disable I-cache
        mcr     p15, 0, r0, c1, c0, 0

        /* Flush D-cache */
        ldr     r1, =0
        mcr     p15, 0, r1, c7, c6, 0                                   @ flush(invalidate) D-cache

        ldr     r0, =0x0000003F						@ region 0 4Gbyte - background
        mcr     p15, 0, r0, c6, c0, 0					@ D-cache region 0
        mcr     p15, 0, r0, c6, c0, 1					@ I-cache region 0
        ldr     r0, =(REGION0_BEGIN_ADDR | RAM_REGION0_SIZE | 1)
        mcr     p15, 0, r0, c6, c1, 0					@ D-cache region 1/REGION0
        mcr     p15, 0, r0, c6, c1, 1					@ I-cache region 1 - SDRAM
        ldr     r0, =(REGION1_BEGIN_ADDR | DMA_REGION_SIZE | 1)
        mcr     p15, 0, r0, c6, c2, 0					@ D-cache region 2/REGION1
        mcr     p15, 0, r1, c6, c2, 1					@ I-cache region 2 - disable
        ldr     r0, =(REGION2_BEGIN_ADDR | RAM_REGION_SIZE | 1)
        mcr     p15, 0, r0, c6, c3, 0					@ D-cache region 3/REGION2
        mcr     p15, 0, r1, c6, c3, 1					@ I-cache region 3 - disable
        ldr     r0, =(REGION3_BEGIN_ADDR | RAM_REGION_SIZE | 1)
        mcr     p15, 0, r0, c6, c4, 0					@ D-cache region 4/REGION3
        mcr     p15, 0, r1, c6, c4, 1					@ I-cache region 4 - disable
        ldr     r0, =(REGION4_BEGIN_ADDR | RAM_REGION_SIZE | 1)
        mcr     p15, 0, r0, c6, c5, 0					@ D-cache region 5/REGION4
        mcr     p15, 0, r1, c6, c5, 1					@ I-cache region 5 - disable
        ldr     r0, =(REGION5_BEGIN_ADDR | RAM_REGION_SIZE | 1)
        mcr     p15, 0, r0, c6, c6, 0					@ D-cache region 6/REGION5
        mcr     p15, 0, r1, c6, c6, 1					@ I-cache region 6 - disable
@coreyliu 20040202 masked for MTD to work
@        ldr     r0, =(FLASH_MEM_BASE | FLASH_REGION_SIZE | 1)           @ REGION7
@        mcr     p15, 0, r0, c6, c7, 0					@ D-cache region 7/REGION6
@        mcr     p15, 0, r0, c6, c7, 1					@ I-cache region 7 - FLASH

        /* initial cacheable bit setting */
	/* DATA CACHE */
        ldr     r0, =0x62                       /* 0110_0010b - region 1/5/6 is cacheable area*/
        mcr     p15, 0, r0, c2, c0, 0           /* cacheable bit setting */
        ldr     r0, =0  
        mcr     p15, 0, r0, c3, c0, 0           /* write buffer setting */

	/* INSTRUCTION CACHE */
        ldr     r0, =0x02                       /* 0000_0010b - region 1 is cacheable area*/
        mcr     p15, 0, r0, c2, c0, 1           /* cacheable bit setting */

        /* protection setting */
        /* protection setting should be done in bootloader */

#endif	/* CONFIG_UNCACHED_MEM */

        /* Enable D/I-cache */
        mrc     p15, 0, r0, c1, c0, 0
        orr     r0, r0, #0x0004                                         @ enable D-cache
	orr	r0, r0, #0x1000						@ enable I-cache
        mcr     p15, 0, r0, c1, c0, 0
        mov     pc, lr

#ifdef	CONFIG_UNCACHED_MEM
/*
 * cpu_arm940_change_region(region setting, cacheable)
 * region number: 1<< region number
 * cacheable: 0 : DMA area
 *            1 : cacheable area
 */
ENTRY(arm940_change_region)
	/* Make mask for region number */
	mov	r3, #2				@ (1<<1) region0 is background region
	mov	r3, r3, lsl r0

	/* Make cacheable mask */
	add	r0, r0, #1			@ keep in mind region0 is background region
	mov	r1, r1, lsl r0
	mov	r0, r3

	/* if cacheable is same, just return */
	mrc	p15, 0, r3, c2, c0, 0
	and	r3, r3, r0
	eors	r3, r3, r1
	beq	5f
	
	/* if need cacheable,
	 *	flush(invalidate) D-cache, and make cacheable
	 * else
	 *	clean D-cache, and make non-cacheable
	 */
	tsts	r1, #0
	beq	1f

	/* DMA area was not cached, so clean&flushing cache is not necessary */

	/* make cacheable */
	mrc	p15, 0, r3, c2, c0, 0		@ enable D-cache region		
	orr	r3, r3, r0
	mcr	p15, 0, r3, c2, c0, 0 
	ldr	r0, =1
	b	6f

1:	/* clean D-cache, and make non-cacheable */
	mov	r1, #0
2:
	mov	r3, #0
3:
	orr	r2, r1, r3
	mcr	p15, 0, r2, c7, c14, 2
	add	r3, r3, #0x10
	cmps	r3, #0x40
	bne	3b
	add	r1, r1, #0x04000000
	cmps	r1, #0x0
	bne	2b
	
	mrc	p15, 0, r3, c2, c0, 0		@ disable D-cache region
	bic	r3, r3, r0
	mcr	p15, 0, r3, c2, c0, 0
	ldr	r0, =2
	b	6f
5:
	ldr	r0, =0
6:
	mov	pc, lr
#endif	/* CONFIG_UNCACHED_MEM */

/*
 * cpu_arm940_proc_fin()
 */
ENTRY(cpu_arm940_proc_fin)
	stmfd	sp!, {lr}
	mov	ip, #F_BIT | I_BIT | SVC_MODE
	msr	cpsr_c, ip
	bl	cpu_arm940_cache_clean_invalidate_all
	mrc	p15, 0, r0, c1, c0, 0		@ ctrl register
	bic	r0, r0, #0x1000			@ ...i............
	bic	r0, r0, #0x0004			@ .............d..
	mcr	p15, 0, r0, c1, c0, 0		@ disable caches
	ldmfd	sp!, {pc}

/*
 * cpu_arm940_reset(loc)
 *
 * Perform a soft reset of the system.  Put the CPU into the
 * same state as it would be if it had been reset, and branch
 * to what would be the reset vector.
 *
 * loc: location to jump to for soft reset
 */
	.align	5
ENTRY(cpu_arm940_reset)
	mov	ip, #0
	mcr	p15, 0, ip, c7, c5, 0		@ invalidate(flush) I cache
	mcr	p15, 0, ip, c7, c6, 0		@ invalidate(flush) D cache
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	mrc	p15, 0, ip, c1, c0, 0		@ ctrl register
	bic	ip, ip, #0x1000			@ ...i............
	bic	ip, ip, #0x0004			@ .............d..
	bic	ip, ip, #0x0001			@ ...............p
	mcr	p15, 0, ip, c1, c0, 0		@ ctrl register
#if 0	/* soft-reboot using pc resetting */
	mov	r0, #SYSCFG			@ System Configuration Reg
	ldr	ip, [r0]
	bic	ip, ip, #0x100			@ No remap
	str	ip, [r0]
	mov	pc, #0				@ Start from 0 address
#else	/* soft-reboot using watch-dog timer */
	ldr	r0, =0xF0040008
	ldr	r1, =0xE0000008
	str	r1, [r0]
1:	mov	r0, #0
	b	1b

#endif
/*
 * cpu_arm940_do_idle()
 */
	.align	5
ENTRY(cpu_arm940_do_idle)
#if defined(CONFIG_CPU_ARM940_CPU_IDLE)
	mcr	p15, 0, r0, c7, c0, 4		@ Wait for interrupt
#endif
	mov	pc, lr

/* ================================= CACHE ================================ */
/*
 * cpu_arm940_cache_clean_invalidate_all()
 *
 * clean and invalidate all cache lines
 *
 * Note:
 *  1. we should preserve r0 at all times
 */
	.align	5
ENTRY(cpu_arm940_cache_clean_invalidate_all)
ENTRY(cpu_arm940_flush_ram_page)
	mov	r2, #1

cpu_arm940_cache_clean_invalidate_all_r2:

	mrc	p15, 0, ip, c3, c0, 0
	tst	ip, #0xff			@ is Wbuffer enabled?
	beq	3f
/*
 * 'Clean & Invalidate whole DCache'
 * Re-written to use Index Ops.
 * Uses registers r1, r3 and ip
 */
	mov	r1, #3 << 4			@ 4 segments
1:	orr	r3, r1, #63 << 26		@ 64 entries
2:	mcr	p15, 0, r3, c7, c14, 2		@ clean & invalidate(flush) D entry
	subs	r3, r3, #1 << 26
	bcs	2b				@ entries 63 to 0
	subs	r1, r1, #1 << 3
	bcs	1b				@ segments 3 to 0
	mov	ip, #0
	b	4f
3:
	mov	ip, #0
	mcr	p15, 0, ip, c7, c6, 0		@ invalidate(flush) D cache
	nop;nop;nop
4:
	teq	r2, #0
	mcrne	p15, 0, ip, c7, c5, 0		@ invalidate(flush) I cache
	nop;nop;nop
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	mov	pc, lr

/*
 * cpu_arm940_cache_clean_invalidate_range(start, end, flags)
 *
 * clean and invalidate all cache lines associated with this area of memory
 *
 * start: Area start address
 * end:   Area end address
 * flags: nonzero for I cache as well
 */
		.align	5
ENTRY(cpu_arm940_cache_clean_invalidate_range)
	b	cpu_arm940_cache_clean_invalidate_all_r2
	nop;nop;nop

/* ================================ D-CACHE =============================== */

/*
 * cpu_arm940_dcache_invalidate_all()
 *
 */
	.align	5
ENTRY(cpu_arm940_dcache_invalidate_clean_all)
ENTRY(cpu_arm940_dcache_invalidate_all)
ENTRY(cpu_arm940_dcache_invalidate_range)
	mov	r2, #0
	b	cpu_arm940_cache_clean_invalidate_all_r2
	nop;nop;nop

/*
 * cpu_arm940_dcache_clean_all()
 *
 */
	.align	5
ENTRY(cpu_arm940_dcache_clean_all)
ENTRY(cpu_arm940_dcache_clean_range)
ENTRY(cpu_arm940_dcache_clean_page)
ENTRY(cpu_arm940_dcache_clean_entry)
	mrc	p15, 0, r1, c3, c0, 0
	tst	r1, #0xff			@ is Wbuffer enabled?
	beq	3f
	mov	r1, #3 << 4			@ 4 segments
1:	orr	r2, r1, #63 << 26		@ 64 entries
2:	mcr	p15, 0, r2, c7, c10, 2		@ clean D entry
	subs	r2, r2, #1 << 26
	bcs	2b				@ entries 63 to 0
	subs	r1, r1, #1 << 3
	bcs	1b				@ segments 3 to 0
3:
	mov	pc, lr

	.align	5
ENTRY(cpu_arm940_dcache_enable)
	mrc	15, 0, r0, c1, c0, 0
	orr	r0, r0, #0x4
	mcr	15, 0, r0, c1, c0, 0
	mov	pc, lr

ENTRY(cpu_arm940_dcache_disable)
	mrc	15, 0, r0, c1, c0, 0
	bic	r0, r0, #0x4
	mcr	15, 0, r0, c1, c0, 0
	mov	pc, lr

ENTRY(cpu_arm940_dcache_flush)
	mov	r0, #0
	mcr	15, 0, r0, c7, c6, 0
	nop;nop;nop
	mov	pc, lr

/* ================================ I-CACHE =============================== */

	.align	5
ENTRY(cpu_arm940_icache_invalidate_all)
ENTRY(cpu_arm940_icache_invalidate_range)
ENTRY(cpu_arm940_icache_invalidate_page)

	b	cpu_arm940_cache_clean_invalidate_all
	nop;nop;nop

/* ================================== TLB & PageTable ===================== */

	.align	5
ENTRY(cpu_arm940_tlb_invalidate_all)
ENTRY(cpu_arm940_tlb_invalidate_range)
ENTRY(cpu_arm940_tlb_invalidate_page)
ENTRY(cpu_arm940_set_pgd)
ENTRY(cpu_arm940_set_pmd)
ENTRY(cpu_arm940_set_pte)

	mov	r0, #0
	mcr	p15, 0, r0, c7, c10, 4		@ drain WB
	mov	pc, lr

cpu_manu_name:
	.asciz	"Samsung"
ENTRY(cpu_arm940_name)
	.ascii	"Arm940T"
#if defined(CONFIG_CPU_ARM940_CPU_IDLE)
	.ascii	"s"
#endif

#if defined(CONFIG_CPU_ARM940_I_CACHE_ON)
	.ascii	"i"
#endif
#if defined(CONFIG_CPU_ARM940_D_CACHE_ON)
	.ascii	"d"
#if defined(CONFIG_CPU_ARM940_WRITETHROUGH)
	.ascii	"(wt)"
#else
	.ascii	"(wb)"
#endif
#endif
	.ascii	"\0"
	.align

	.section ".text.init", #alloc, #execinstr

__arm940_setup:
	mov	r0, #0
	MCR	p15, 0, r0, c7, c5, 0		@ invalidate ICache on v4
	MCR	p15, 0, r0, c7, c6, 0		@ invalidate DCache on v4
	MCR	p15, 0, r0, c7, c10, 4		@ drain write buffer on v4
	MRC	p15, 0, r0, c1, c0, 0		@ get control register v4
	orr	r0, r0, #1			@ Enable PU
	mov	pc, lr
	nop;nop;nop
	
	.text

/*
 * Purpose : Function pointers used to access above functions - all calls
 *	     come through these
 */
	.type	arm940_processor_functions, #object
arm940_processor_functions:
	.word	cpu_arm940_data_abort
	.word	cpu_arm940_check_bugs
	.word	cpu_arm940_proc_init
	.word	cpu_arm940_proc_fin
	.word	cpu_arm940_reset
	.word   cpu_arm940_do_idle

	/* cache */
	.word	cpu_arm940_cache_clean_invalidate_all
	.word	cpu_arm940_cache_clean_invalidate_range
	.word	cpu_arm940_flush_ram_page

	/* dcache */
	.word	cpu_arm940_dcache_invalidate_range
	.word	cpu_arm940_dcache_clean_range
	.word	cpu_arm940_dcache_clean_page
	.word	cpu_arm940_dcache_clean_entry

	/* icache */
	.word	cpu_arm940_icache_invalidate_range
	.word	cpu_arm940_icache_invalidate_page

	/* tlb */
	.word	cpu_arm940_tlb_invalidate_all
	.word	cpu_arm940_tlb_invalidate_range
	.word	cpu_arm940_tlb_invalidate_page

	/* pgtable */
	.word	cpu_arm940_set_pgd
	.word	cpu_arm940_set_pmd
	.word	cpu_arm940_set_pte
	.size	arm940_processor_functions, . - arm940_processor_functions

	.type	cpu_arm940_info, #object
cpu_arm940_info:
	.long	cpu_manu_name
	.long	cpu_arm940_name
	.size	cpu_arm940_info, . - cpu_arm940_info

	.type	cpu_arch_name, #object
cpu_arch_name:
	.asciz	"armv4"
	.size	cpu_arch_name, . - cpu_arch_name

	.type	cpu_elf_name, #object
cpu_elf_name:
	.asciz	"v4"
	.size	cpu_elf_name, . - cpu_elf_name
	.align

	.section ".proc.info", #alloc, #execinstr

	.type	__arm940_proc_info,#object
__arm940_proc_info:
	.long	0x41009400
	.long	0xff00fff0
	.long	0x00000c1e			@ mmuflags
	b	__arm940_setup
	.long	cpu_arch_name
	.long	cpu_elf_name
	.long	HWCAP_SWP | HWCAP_HALF | HWCAP_26BIT
	.long	cpu_arm940_info
	.long	arm940_processor_functions
	.size	__arm940_proc_info, . - __arm940_proc_info
