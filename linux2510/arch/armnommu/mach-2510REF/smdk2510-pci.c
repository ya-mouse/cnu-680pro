/*
 * linux/arch/armnommu/mach-2510REF/smdk2510-pci.c
 *
 * PCI bios-type initialisation for PCI machines
 *
 * Bits taken from various places.
 *
 * made by Roh you-chang(terius90@samsung.com)
 *
 */
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/irq.h>
#include <asm/mach/pci.h>

#ifdef CONFIG_PCI
#define SMDK2510_PCI_MEM 0x00000000 

/* irqmap table */
static int irqmap_smdk2510[] __initdata = { SRC_IRQ_PCI_PCCARD };

static u8 __init smdk2510_swizzle(struct pci_dev *dev, u8 *pin)
{
	return PCI_SLOT(dev->devfn);
}

static int __init smdk2510_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	return irqmap_smdk2510[0];
}

/* The structure for initialization, setup resource... of S3C2510 hardware */
struct hw_pci smdk2510_pci __initdata = {
	setup_resources:	smdk2510_pci_setup_resources,
	init:			smdk2510_pci_init,
	mem_offset:		SMDK2510_PCI_MEM,
	swizzle:		smdk2510_swizzle,
	map_irq:		smdk2510_map_irq,
};
#endif
