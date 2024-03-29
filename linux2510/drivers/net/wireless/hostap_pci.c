#define PRISM2_PCI

/* Host AP driver's support for Intersil Prism2.5 PCI cards is based on
 * driver patches from Reyk Floeter <reyk@vantronix.net> and
 * Andy Warner <andyw@pobox.com> */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/if.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,44))
#include <linux/tqueue.h>
#else
#include <linux/workqueue.h>
#endif
#include <linux/wireless.h>
#if WIRELESS_EXT > 12
#include <net/iw_handler.h>
#endif /* WIRELESS_EXT > 12 */

#include <linux/ioport.h>
#include <linux/pci.h>

#include "hostap_wlan.h"

static char *version = PRISM2_VERSION " (Jouni Malinen <jkmaline@cc.hut.fi>)";
/*static char *rev_version = SEC_VERSION " (Roh you-chang <terius90@samsung.com>)";*/
static char *dev_info = "hostap_pci";

MODULE_AUTHOR("SSH Communications Security Corp, Jouni Malinen");
MODULE_DESCRIPTION("Support for Intersil Prism2.5-based 802.11 wireless LAN "
		   "PCI cards.");
MODULE_SUPPORTED_DEVICE("Intersil Prism2.5-based WLAN PCI cards");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0))
/* PCI initialization uses Linux 2.4.x version and older kernels do not support
 * this */
#error Prism2.5 PCI version requires at least Linux kernel version 2.4.0
#endif /* kernel < 2.4.0 */


/* FIX: do we need mb/wmb/rmb with memory operations? */


static struct pci_device_id prism2_pci_id_table[] __devinitdata = {
	/* Intersil Prism2.5 ISL3874 11Mb/s WLAN Controller */
	{ 0x1260, 0x3873, PCI_ANY_ID, PCI_ANY_ID },
	/* Samsung MagicLAN SWL-2210P */
	{ 0x167d, 0xa000, PCI_ANY_ID, PCI_ANY_ID },
/*	{ PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID },*/
	{ 0 }
};

//Radicalis_hans_begin (03.12.29)
//#if defined(CONFIG_BOARD_SMDK2510) | defined(CONFIG_BOARD_2510REF) //ryc++
#if 0
#define HFA384X_OUTB(v,a) PCIMemWrite8(__mem_pci(dev->mem_start + (a)),v)
#define HFA384X_INB(a) (u8) PCIMemRead8(__mem_pci(dev->mem_start + (a)))
#define HFA384X_OUTW(v,a) PCIMemWrite16(__mem_pci(dev->mem_start + (a)),(v))
#define HFA384X_INW(a) (u16) PCIMemRead16(__mem_pci(dev->mem_start + (a)))
#define HFA384X_OUTW_DATA(v,a) PCIMemWrite16(__mem_pci(dev->mem_start + (a)),cpu_to_le16(v))
#define HFA384X_INW_DATA(a) (u16) le16_to_cpu(PCIMemRead16(__mem_pci(dev->mem_start + (a))))
//Radicalis_hans_begin (03.12.29)
#else
#define HFA384X_OUTB(v,a) writeb((v), dev->mem_start + (a))
#define HFA384X_INB(a) (u8) readb(dev->mem_start + (a))
#define HFA384X_OUTW(v,a) writew((v), dev->mem_start + (a))
#define HFA384X_INW(a) (u16) readw(dev->mem_start + (a))
#define HFA384X_OUTW_DATA(v,a) writew(cpu_to_le16(v), dev->mem_start + (a))
#define HFA384X_INW_DATA(a) (u16) le16_to_cpu(readw(dev->mem_start + (a)))
#endif

#ifdef PRISM2_IO_DEBUG

static inline void hfa384x_outb_debug(struct net_device *dev, int a, u8 v)
{
	local_info_t *local = dev->priv;
	unsigned long flags;

	spin_lock_irqsave(&local->lock, flags);
	prism2_io_debug_add(dev, PRISM2_IO_DEBUG_CMD_OUTB, a, v);
	writeb(v, dev->mem_start + a);
	spin_unlock_irqrestore(&local->lock, flags);
}

static inline u8 hfa384x_inb_debug(struct net_device *dev, int a)
{
	local_info_t *local = dev->priv;
	unsigned long flags;
	u8 v;

	spin_lock_irqsave(&local->lock, flags);
	v = readb(dev->mem_start + a);
	prism2_io_debug_add(dev, PRISM2_IO_DEBUG_CMD_INB, a, v);
	spin_unlock_irqrestore(&local->lock, flags);
	return v;
}

static inline void hfa384x_outw_debug(struct net_device *dev, int a, u16 v)
{
	local_info_t *local = dev->priv;
	unsigned long flags;

	spin_lock_irqsave(&local->lock, flags);
	prism2_io_debug_add(dev, PRISM2_IO_DEBUG_CMD_OUTW, a, v);
	writew(v, dev->mem_start + a);
	spin_unlock_irqrestore(&local->lock, flags);
}

static inline u16 hfa384x_inw_debug(struct net_device *dev, int a)
{
	local_info_t *local = dev->priv;
	unsigned long flags;
	u16 v;

	spin_lock_irqsave(&local->lock, flags);
	v = readw(dev->mem_start + a);
	prism2_io_debug_add(dev, PRISM2_IO_DEBUG_CMD_INW, a, v);
	spin_unlock_irqrestore(&local->lock, flags);
	return v;
}

#define HFA384X_OUTB(v,a) hfa384x_outb_debug(dev, (a), (v))
#define HFA384X_INB(a) hfa384x_inb_debug(dev, (a))
#define HFA384X_OUTW(v,a) hfa384x_outw_debug(dev, (a), (v))
#define HFA384X_INW(a) hfa384x_inw_debug(dev, (a))
#define HFA384X_OUTW_DATA(v,a) hfa384x_outw_debug(dev, (a), cpu_to_le16((v)))
#define HFA384X_INW_DATA(a) (u16) le16_to_cpu(hfa384x_inw_debug(dev, (a)))
/*
#else // PRISM2_IO_DEBUG

#define HFA384X_OUTB(v,a) writeb((v), dev->mem_start + (a))
#define HFA384X_INB(a) (u8) readb(dev->mem_start + (a))
#define HFA384X_OUTW(v,a) writew((v), dev->mem_start + (a))
#define HFA384X_INW(a) (u16) readw(dev->mem_start + (a))
#define HFA384X_OUTW_DATA(v,a) writew(cpu_to_le16(v), dev->mem_start + (a))
#define HFA384X_INW_DATA(a) (u16) le16_to_cpu(readw(dev->mem_start + (a)))
*/
#endif /* PRISM2_IO_DEBUG */


static int hfa384x_from_bap(struct net_device *dev, u16 bap, void *buf,
			    int len)
{
	u16 d_off;
	u16 *pos;

	d_off = (bap == 1) ? HFA384X_DATA1_OFF : HFA384X_DATA0_OFF;
	pos = (u16 *) buf;

	for ( ; len > 1; len -= 2)
		*pos++ = HFA384X_INW_DATA(d_off);

	if (len & 1)
		*((char *) pos) = HFA384X_INB(d_off);

	return 0;
}


static int hfa384x_to_bap(struct net_device *dev, u16 bap, void *buf, int len)
{
	u16 d_off;
	u16 *pos;

	d_off = (bap == 1) ? HFA384X_DATA1_OFF : HFA384X_DATA0_OFF;
	pos = (u16 *) buf;

	for ( ; len > 1; len -= 2)
		HFA384X_OUTW_DATA(*pos++, d_off);

	if (len & 1)
		HFA384X_OUTB(*((char *) pos), d_off);

	return 0;
}


/* FIX: This might change at some point.. */
#include "hostap_hw.c"

static void prism2_pci_cor_sreset(local_info_t *local)
{
	struct net_device *dev = local->dev;

	/* linux-wlan-ng uses extremely long hold and settle times for
	 * COR sreset. A comment in the driver code mentions that the long
	 * delays appear to be necessary. However, at least IBM 22P6901 seems
	 * to work fine with shorter delays.
	 *
	 * Longer delays can be configured by uncommenting following line: */
/* #define PRISM2_PCI_USE_LONG_DELAYS */

#ifdef PRISM2_PCI_USE_LONG_DELAYS
	int i;

	HFA384X_OUTW(0x0080, HFA384X_PCICOR_OFF);
	mdelay(250);

	HFA384X_OUTW(0x0, HFA384X_PCICOR_OFF);
	mdelay(500);

	/* Wait for f/w to complete initialization (CMD:BUSY == 0) */
	i = 2000000 / 10;
	while ((HFA384X_INW(HFA384X_CMD_OFF) & HFA384X_CMD_BUSY) && --i)
		udelay(10);

#else /* PRISM2_PCI_USE_LONG_DELAYS */

	HFA384X_OUTW(0x0080, HFA384X_PCICOR_OFF);
	mdelay(1);
	HFA384X_OUTW(0x0, HFA384X_PCICOR_OFF);
	mdelay(1);

#endif /* PRISM2_PCI_USE_LONG_DELAYS */

	if (HFA384X_INW(HFA384X_CMD_OFF) & HFA384X_CMD_BUSY) {
		printk("%s: COR sreset timeout\n", dev->name);
	}
}


static struct prism2_helper_functions prism2_pci_funcs =
{
	.card_present	= NULL,
	.cor_sreset	= prism2_pci_cor_sreset,
	.dev_open	= NULL,
	.dev_close	= NULL
};


static int prism2_pci_probe(struct pci_dev *pdev,
			    const struct pci_device_id *id)
{
	unsigned long phymem;
	unsigned long mem = 0;
	local_info_t *local = NULL;
	struct net_device *dev = NULL;
	static int cards_found /* = 0 */;
	int irq_registered = 0;

	if (pci_enable_device(pdev)){
		printk("pci_enable_device failed\n"); //ryc++
		return -EIO;
	}

	phymem = pci_resource_start(pdev, 0);

// Radicalis_hans_begin
//	if (!request_mem_pci_region(phymem, pci_resource_len(pdev, 0), "Prism2")) {
	if (!request_mem_region(phymem, pci_resource_len(pdev, 0), "Prism2")) {
// Radicalis_hans_end
		printk(KERN_ERR "prism2: Cannot reserve PCI memory region\n");
		goto err_out_disable;
	}

	mem = (unsigned long) ioremap(phymem, pci_resource_len(pdev, 0));
	if (!mem) {
		printk(KERN_ERR "prism2: Cannot remap PCI memory region\n") ;
		goto fail;
	}

#ifdef PRISM2_BUS_MASTER
	pci_set_master(pdev);
#endif /* PRISM2_BUS_MASTER */

	local = prism2_init_local_data(&prism2_pci_funcs, cards_found);
	printk("cards_found = %d\n",cards_found); //ryc++
	if (local == NULL)
		goto fail;
	cards_found++;

	dev = local->dev;

        dev->irq = pdev->irq;
        dev->mem_start = mem;
        dev->mem_end = mem + pci_resource_len(pdev, 0);

	if (prism2_init_dev(local)){
		printk("prism2_init_dev failed\n"); //ryc++
		goto fail;
	}

	prism2_pci_cor_sreset(local); 

	pci_set_drvdata(pdev, dev);

	if (request_irq(dev->irq, prism2_interrupt, SA_SHIRQ, dev->name,
			dev)) {
		printk(KERN_WARNING "%s: request_irq failed\n", dev->name);
		goto fail;
	} else
		irq_registered = 1;

	if (prism2_hw_config(dev, 1)) {
		printk("%s: hardware initialization failed\n",
		       dev_info);
		goto fail;
	}

	printk("%s: Intersil Prism2.5 PCI: "
	       "mem=0x%lx, irq=%d\n", dev->name, phymem, dev->irq);

	return 0;

 fail:
	prism2_free_local_data(local);

	if (irq_registered && dev)
		free_irq(dev->irq, dev);

	if (mem)
		iounmap((void *) mem);

// Radicalis_hans_begin
//	release_mem_pci_region(phymem, pci_resource_len(pdev, 0));
	release_mem_region(phymem, pci_resource_len(pdev, 0));
// Radicalis_hans_end

 err_out_disable:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,4))
	pci_disable_device(pdev);
#endif

	return -ENODEV;
}


static void prism2_pci_remove(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	local_info_t *local = (local_info_t *) dev->priv;
	unsigned long mem_start;

	/* Reset the hardware, and ensure interrupts are disabled. */
	prism2_pci_cor_sreset(local);
	hfa384x_disable_interrupts(dev);

	if (dev->irq)
		free_irq(dev->irq, dev);

	mem_start = dev->mem_start;
	prism2_free_local_data(local);

	iounmap((void *) mem_start);

// Radicalis_hans_begin
//	release_mem_pci_region(pci_resource_start(pdev, 0),
	release_mem_region(pci_resource_start(pdev, 0),
// Radicalis_hans_end
			   pci_resource_len(pdev, 0));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,4))
	pci_disable_device(pdev);
#endif
}


#ifdef CONFIG_PM
static int prism2_pci_suspend(struct pci_dev *pdev, u32 state)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	local_info_t *local = (local_info_t *) dev->priv;

	if (netif_running(dev)) {
		hostap_netif_stop_queues(dev);
		netif_device_detach(dev);
	}
	prism2_hw_shutdown(dev, 0);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,6))
	pci_save_state(pdev, local->pci_save_state);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,4))
	pci_disable_device(pdev);
#endif
	pci_set_power_state(pdev, 3);

	return 0;
}

static int prism2_pci_resume(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	local_info_t *local = (local_info_t *) dev->priv;

	pci_enable_device(pdev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,6))
	pci_restore_state(pdev, local->pci_save_state);
#endif
	prism2_hw_config(dev, 0);
	if (netif_running(dev)) {
		netif_device_attach(dev);
		netif_start_queue(dev);
	}

	return 0;
}
#endif /* CONFIG_PM */


MODULE_DEVICE_TABLE(pci, prism2_pci_id_table);

static struct pci_driver prism2_pci_drv_id = {
	.name		= "prism2_pci",
	.id_table	= prism2_pci_id_table,
	.probe		= prism2_pci_probe,
	.remove		= prism2_pci_remove,
#ifdef CONFIG_PM
	.suspend	= prism2_pci_suspend,
	.resume		= prism2_pci_resume,
#endif /* CONFIG_PM */
	/* Linux 2.4.6 added save_state and enable_wake that are not used here
	 */
};


static int __init init_prism2_pci(void)
{
	printk(KERN_INFO "%s: %s\n", dev_info, version);
	/*
	printk(KERN_INFO "%s: %s\n", dev_info, rev_version); //ryc++ for management
	*/

	if (pci_register_driver(&prism2_pci_drv_id) <= 0) {
		printk("hostap_pci: No devices found, driver not "
		       "installed.\n");
		pci_unregister_driver(&prism2_pci_drv_id);
		return -ENODEV;
	}

	return 0;
}


static void __exit exit_prism2_pci(void)
{
	pci_unregister_driver(&prism2_pci_drv_id);
	printk(KERN_INFO "%s: Driver unloaded\n", dev_info);
}


module_init(init_prism2_pci);
module_exit(exit_prism2_pci);
