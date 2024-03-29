/* $Id: head.S,v 1.1.1.1 2003/11/17 02:33:22 jipark Exp $
 *
 *  arch/sh/kernel/head.S
 *
 *  Copyright (C) 1999, 2000  Niibe Yutaka & Kaz Kojima
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Head.S contains the SH exception handlers and startup code.
 */
#include <linux/linkage.h>

	.section	.empty_zero_page, "aw"
ENTRY(empty_zero_page)
#ifdef CONFIG_BLK_DEV_INITRD
	.long	1		/* MOUNT_ROOT_RDONLY */
	.long	0x4000		/* RAMDISK_FLAGS */
	.long	0x0100		/* ORIG_ROOT_DEV */
	.long	1		/* LOADER_TYPE */
#if defined(CONFIG_SH_SNAPGEAR)
	.long	_end		/* INITRD_START */
	.long	0x000000	/* INITRD_SIZE */
#else
	.long	0x00360000	/* INITRD_START */
	.long	0x000a0000	/* INITRD_SIZE */
#endif
	.long	0
#else
	.long	1		/* MOUNT_ROOT_RDONLY */
	.long	0		/* RAMDISK_FLAGS */
	.long	0x0200		/* ORIG_ROOT_DEV */
	.long	1		/* LOADER_TYPE */
	.long	0x00360000	/* INITRD_START */
	.long	0x000a0000	/* INITRD_SIZE */
	.long	0
#endif
#if defined(CONFIG_SH_SNAPGEAR)
	.balign 256,0,256
cmdline:
#if defined(CONFIG_SH_SECUREEDGE5410)
# if defined(CONFIG_SH_CONSOLE_TTYSC0)
	.ascii	"mem=16M console=ttySC0,57600\0"
# else
	.ascii	"mem=16M console=ttySC1,57600\0"
# endif
#else
	.ascii	"mem=32M console=ttySC1,57600\0"
#endif
#endif

	.balign 4096,0,4096
	.text	
/*
 * Condition at the entry of _stext:
 *
 *   BSC has already been initialized.
 *   INTC may or may not be initialized.
 *   VBR may or may not be initialized.
 *   MMU may or may not be initialized.
 *   Cache may or may not be initialized.
 *   Hardware (including on-chip modules) may or may not be initialized. 
 *
 */
ENTRY(_stext)
	!			Initialize Status Register
	mov.l	1f, r0		! MD=1, RB=0, BL=0, IMASK=0xF
	ldc	r0, sr
	!			Initialize global interrupt mask
	mov	#0, r0
	ldc	r0, r6_bank
	!
	mov.l	2f, r0
	mov	r0, r15		! Set initial r15 (stack pointer)
	mov	#0x20, r1	!
	shll8	r1		! r1 = 8192
	sub	r1, r0		!
	ldc	r0, r7_bank	! ... and init_task
	!
#if defined(__SH4__)
	!                       Initialize fpu
	mov.l   7f, r0
	jsr     @r0
	nop
#endif
	!			Enable cache
	mov.l	6f, r0
	jsr	@r0
	nop
#if defined(CONFIG_SH_SNAPGEAR)
	! move the romfs to after the bss
	mov.l	fix_romfs, r0
	jsr @r0
#endif
	nop
	!			Clear BSS area
	mov.l	3f, r1
	add	#4, r1
	mov.l	4f, r2
	mov	#0, r0
9:
    cmp/hs	r2, r1
	bf/s	9b		! while (r1 < r2)
	mov.l	r0,@-r2
	!			Start kernel
	mov.l	5f, r0
	jmp	@r0
	nop

	.balign 4
1:	.long	0x400000F0		! MD=1, RB=0, BL=0, FD=0, IMASK=0xF
2:	.long	SYMBOL_NAME(stack)
3:	.long	SYMBOL_NAME(__bss_start)
4:	.long	SYMBOL_NAME(_end)
5:	.long	SYMBOL_NAME(start_kernel)
6:	.long	SYMBOL_NAME(cache_init)
#if defined(__SH4__)
7:	.long   SYMBOL_NAME(fpu_init)
#endif
#if defined(CONFIG_SH_SNAPGEAR)
fix_romfs: .long copy_romfs
#endif
