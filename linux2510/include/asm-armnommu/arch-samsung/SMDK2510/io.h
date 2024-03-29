/*
 * linux/include/asm-armnommu/arch-dsc21/io.h
 *
 * Copyright (C) 1997-1999 Russell King
 *
 * Modifications:
 *  06-12-1997	RMK	Created.
 *  07-04-1999	RMK	Major cleanup
 *  02-19-2001  gjm     Leveraged for armnommu/dsc21
 */
#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

/*
 * kernel/resource.c uses this to initialize the global ioport_resource struct
 * which is used in all calls to request_resource(), allocate_resource(), etc.
 * --gmcnutt
 */
#define IO_SPACE_LIMIT 0xffffffff

/*
 * If we define __io then asm/io.h will take care of most of the inb & friends
 * macros. It still leaves us some 16bit macros to deal with ourselves, though.
 * We don't have PCI or ISA on the dsc21 so I dropped __mem_pci & __mem_isa.
 * --gmcnutt
 */
#define PCIO_BASE 0
#define __io(a) (PCIO_BASE + (a))
#define __mem_pci(a)    ((unsigned long)(a)) //ryc++
#ifdef CONFIG_PCI
#include <asm/arch/pci.h>
/* 
 * ryc redefined for PCI memory/io I/O fuctions for SMDK2510
 *
 */

#define _MapIOAddress(a)    ((u32)PCIIO_ADDR+ (u32)(a & 0x03ffffff))
#define _MapMemAddress(a)   ((u32)PCIMEM_ADDR+ (u32)(a & 0x0fffffff))

/* pci I/O space access macros */
static inline u8 PCIIORead8(unsigned long offset)
{
    rPCIBATAPI = offset&0xfc000000;
    return *(volatile u8 *)(_MapIOAddress(offset));
}
static inline u16 PCIIORead16(unsigned long offset)
{
    rPCIBATAPI = offset&0xfc000000;
    return *(volatile u16 *)(_MapIOAddress(offset));
}
static inline u32 PCIIORead32(unsigned long offset)
{
    rPCIBATAPI = offset&0xfc000000;
    return *(volatile u32 *)(_MapIOAddress(offset));
}
/* ryc++ for fixing bug of compiler */
static void inline PCIIOWrite32(u32 offset,u32 data)   \
{                               \
    rPCIBATAPI = offset&0xf0000000; \
    *(volatile u32 *)(_MapIOAddress(offset)) = data;\
}
static void inline PCIIOWrite16(u32 offset,u16 data)   \
{                               \
    rPCIBATAPI = offset&0xf0000000; \
    *(volatile u16 *)(_MapIOAddress(offset)) = data;\
}
static void inline PCIIOWrite8(u32 offset,u8 data)      \
{                               \
    rPCIBATAPI = offset&0xf0000000; \
    *(volatile u8 *)(_MapIOAddress(offset)) = data; \
}
/* pci memory space access macros */
static inline u8 PCIMemRead8(unsigned long offset)
{
    rPCIBATAPM = offset&0xf0000000;
    return *(volatile u8 *)(_MapMemAddress(offset));
}
static inline u16 PCIMemRead16(unsigned long offset)
{
    rPCIBATAPM = offset&0xf0000000;
    return *(volatile u16 *)(_MapMemAddress(offset));
}
static inline u32 PCIMemRead32(unsigned long offset)
{
    rPCIBATAPM = offset&0xf0000000;
    return *(volatile u32 *)(_MapMemAddress(offset));
}
static void inline PCIMemWrite32(u32 offset,u32 data)   \
{                               \
    rPCIBATAPM = offset&0xf0000000; \
    *(volatile u32 *)(_MapMemAddress(offset)) = data;\
}
static void inline PCIMemWrite16(u32 offset,u16 data)   \
{                               \
    rPCIBATAPM = offset&0xf0000000; \
    *(volatile u16 *)(_MapMemAddress(offset)) = data;\
}
static void inline PCIMemWrite8(u32 offset,u8 data)         \
{                               \
    rPCIBATAPM = offset&0xf0000000; \
    *(volatile u8 *)(_MapMemAddress(offset)) = data;    \
}
#endif /* CONFIG_PCI */



#define __arch_getw(a) (*(volatile unsigned short *)(a))
#define __arch_putw(v,a) (*(volatile unsigned short *)(a) = (v))

/*
 * Defining these two gives us ioremap for free. See asm/io.h.
 * --gmcnutt
 */
#define iomem_valid_addr(iomem,sz) (1)
#define iomem_to_phys(iomem) (iomem)

#endif
