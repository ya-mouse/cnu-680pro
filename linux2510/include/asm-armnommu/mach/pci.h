/*
 *  linux/include/asm-arm/mach/pci.h
 *
 *  Copyright (C) 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * history:
 * 	2002.11.13 modified hw_pci structure for 2510 host pci by
 * 	Roh you chang(terius90@samsung.com)
 */
#define MAX_NR_BUS	2

struct arm_bus_sysdata {
	/*
	 * bitmask of features we can turn.
	 * See PCI command register for more info.
	 */
	u16		features;
	/*
	 * Maximum devsel for this bus.
	 */
	u16		maxdevsel;
	/*
	 * The maximum latency that devices on this
	 * bus can withstand.
	 */
	u8		max_lat;
};

struct arm_pci_sysdata {
	struct arm_bus_sysdata bus[MAX_NR_BUS];
};
/* ryc-- original structure 
struct hw_pci {
	void		(*init)(struct arm_pci_sysdata *);
	u8		(*swizzle)(struct pci_dev *dev, u8 *pin);
	int		(*map_irq)(struct pci_dev *dev, u8 slot, u8 pin);
};
*/
/* ryc modified for 2510 host pci*/
struct hw_pci {
   /* void        (*init)(struct arm_pci_sysdata *); */
    void    (*init)(void *); 
    u8      (*swizzle)(struct pci_dev *dev, u8 *pin);
    int     (*map_irq)(struct pci_dev *dev, u8 slot, u8 pin);
    void    (*setup_resources)(struct resource **); 
    unsigned long mem_offset;
};

extern u8 no_swizzle(struct pci_dev *dev, u8 *pin);

void __init dc21285_init(struct arm_pci_sysdata *);
void __init plx90x0_init(struct arm_pci_sysdata *);

extern void __init smdk2510_pci_setup_resources(struct resource **resource);
extern void __init smdk2510_pci_init(struct arm_pci_sysdata*);

