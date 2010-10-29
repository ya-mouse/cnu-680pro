/*
 * linux/include/asm-armnommu/arch-p52/memory.h
 *
 * Copyright (c) 1999 Nicolas Pitre <nico@cam.org>
 * 2001 Mindspeed
 */
#include <linux/autoconf.h>

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#define TASK_SIZE	(0x01a00000UL)
#define TASK_SIZE_26	TASK_SIZE

#define PHYS_OFFSET	(DRAM_BASE)
#define PAGE_OFFSET 	(PHYS_OFFSET)
#define END_MEM     	(DRAM_BASE + DRAM_SIZE)

#if   defined(CONFIG_INITIAL_DMA_REGION_4MB)
#define ARM940_DMA_REGION_SIZE		0x400000
#elif defined(CONFIG_INITIAL_DMA_REGION_2MB)
#define ARM940_DMA_REGION_SIZE	0x200000
#else
#define ARM940_DMA_REGION_SIZE	0x100000
#endif

#if   defined(CONFIG_REGION_2MB)
#define ARM940_REGION_SIZE		0x200000
#elif defined(CONFIG_REGION_1MB)
#define ARM940_REGION_SIZE		0x100000
#else
#define ARM940_REGION_SIZE		0x080000
#endif
    
#define REGION1_END_ADDR                (DRAM_SIZE)
#define REGION1_BEGIN_ADDR              (DRAM_SIZE - ARM940_DMA_REGION_SIZE)
#define REGION2_END_ADDR                (REGION1_BEGIN_ADDR)
#define REGION2_BEGIN_ADDR              (REGION1_BEGIN_ADDR -ARM940_REGION_SIZE)
#define REGION3_END_ADDR                (REGION2_BEGIN_ADDR)
#define REGION3_BEGIN_ADDR              (REGION2_BEGIN_ADDR -ARM940_REGION_SIZE)
#define REGION4_END_ADDR                (REGION3_BEGIN_ADDR)
#define REGION4_BEGIN_ADDR              (REGION3_BEGIN_ADDR -ARM940_REGION_SIZE)
#define REGION5_END_ADDR                (REGION4_BEGIN_ADDR)
#define REGION5_BEGIN_ADDR              (REGION4_BEGIN_ADDR -ARM940_REGION_SIZE)
#define REGION0_END_ADDR                (REGION5_BEGIN_ADDR)
#define REGION0_BEGIN_ADDR              (0)

#endif
