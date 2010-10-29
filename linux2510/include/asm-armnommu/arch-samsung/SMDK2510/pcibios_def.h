/*
 * pcibios_def.h 
 *
 * Copyright (C) 2002 Arcturus Networks Inc. 
 * by Oleksandr Zhadan <oleks@arcturusnetworks.com>
 *
 * This file includes the pcibios_def definitions 
 * for the S3C2510X RISC microcontroller
 *
 */
#ifndef __ASM_ARCH_PCIBIOS_DEF_H
#define __ASM_ARCH_PCIBIOS_DEF_H

#ifdef CONFIG_PCI
    #define pcibios_assign_all_busses() 0
	#define PCIBIOS_MIN_IO      0x2000000
	#define PCIBIOS_MIN_MEM     0x00000000
#else
    #define pcibios_assign_all_busses() 0
#endif

#endif /* __ASM_ARCH_PCIBIOS_DEF_H */
