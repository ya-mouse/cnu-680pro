/*
 * pcibios_def.h 
 *
 * Copyright (C) 2003 samsung electronics Inc. 
 * by Roh you-chang <terius90@samsung.com>
 *
 * This file includes the pcibios_def definitions 
 * for the S3C2510A RISC microcontroller
 *
 */
#ifndef __ASM_ARCH_PCIBIOS_DEF_H
#define __ASM_ARCH_PCIBIOS_DEF_H

#ifdef CONFIG_PCI
    #define pcibios_assign_all_busses() 0
// Radicalis_hans_begin (03.12.24)
//	#define PCIBIOS_MIN_IO      0x2000000
	#define PCIBIOS_MIN_IO      0xdc000000
// Radicalis_hans_end (03.12.24)
	#define PCIBIOS_MIN_MEM     0x00000000
#else
    #define pcibios_assign_all_busses() 0
#endif

#endif /* __ASM_ARCH_PCIBIOS_DEF_H */
