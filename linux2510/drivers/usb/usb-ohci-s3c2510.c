/*
 *  linux/drivers/usb/usb-ohci-s3c2510.c
 *  Modified for S3C2510
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/pci.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

#include "usb-ohci.h"

extern int __devinit hc_found_ohci (struct pci_dev *dev, int irq,void *mem_base, const struct pci_device_id *id);
extern void __devexit ohci_pci_remove(struct pci_dev *dev);

static struct pci_dev dev;		// 20020911 by drsohn
static void __init s3c2510_ohci_configure(void)
{

	/*
	 * Configure the power sense and control lines.  Place the USB
	 * host controller in reset.
	 * Now, carefully enable the USB clock, and take
	 * the USB host controller out of reset.
	 */

}

static int __init s3c2510_ohci_init(void)
{
	int ret;

	//memcpy(dev.name, "usb-ohci", strlen("usb-ohci"));
	//memcpy(dev.slot_name,"s3c2510", strlen("s3c2510"));
	strcpy(dev.name,"usb-ohci");
	strcpy(dev.slot_name,"s3c2510");

	s3c2510_ohci_configure();

	/*
	 * Initialise the generic OHCI driver.
	 */
	
	ret = hc_found_ohci(&dev, IRQ_USBH,(void *)VA_USB_BASE, (struct pci_device_id *)1);
	
	return ret;
}

static void __exit s3c2510_ohci_exit(void)
{
	ohci_pci_remove((struct pci_dev *)1);
	

	/*
	 * Put the USB host controller into reset.
	 */

	/*
	 * Stop the USB clock.
	 */

	/*
	 * Release memory resources.
	 */

}

module_init(s3c2510_ohci_init);
module_exit(s3c2510_ohci_exit);
