/*
 * linux/include/asm-armnommu/arch-p52/memory.h
 *
 * Copyright (c) 1999 Nicolas Pitre <nico@cam.org>
 * 2001 Mindspeed
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#define TASK_SIZE	(0x01a00000UL)
#define TASK_SIZE_26	TASK_SIZE

#define PHYS_OFFSET	(DRAM_BASE)
#define PAGE_OFFSET PHYS_OFFSET
#define END_MEM     (DRAM_BASE + DRAM_SIZE)
#endif
