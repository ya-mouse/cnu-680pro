/*
 *	Generic parts
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	$Id: br.c,v 1.1.1.1 2003/11/17 02:35:49 jipark Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/if_bridge.h>
#include <linux/brlock.h>
#include <linux/proc_fs.h> // add for SNMP trap
#include <asm/uaccess.h>
#include "br_private.h"

#if defined(CONFIG_ATM_LANE) || defined(CONFIG_ATM_LANE_MODULE)
#include "../atm/lec.h"
#endif

#if defined(CONFIG_BRIDGE_EBT_BROUTE) || \
	defined(CONFIG_BRIDGE_EBT_BROUTE_MODULE)
	unsigned int (*broute_decision) (unsigned int hook, struct sk_buff **pskb,
			const struct net_device *in,
			const struct net_device *out,
			int (*okfn)(struct sk_buff *)) = NULL;
#endif

extern int bridge_status_on;

void br_dec_use_count()
{
	MOD_DEC_USE_COUNT;
}

void br_inc_use_count()
{
	MOD_INC_USE_COUNT;
}

int bridge_read_procmem(char *buf, char** start, off_t offset, int count, int *eof, void *data)
{
	
	int len=0;
	
	if (bridge_status_on)
		len += sprintf(buf+len,"1\n");
	else
		len += sprintf(buf+len,"0\n");
	
	return len;
}

static int __init br_init(void)
{
	printk(KERN_INFO "NET4: Ethernet Bridge 008 for NET4.0\n");

	br_handle_frame_hook = br_handle_frame;
	br_ioctl_hook = br_ioctl_deviceless_stub;
#if defined(CONFIG_ATM_LANE) || defined(CONFIG_ATM_LANE_MODULE)
	br_fdb_get_hook = br_fdb_get;
	br_fdb_put_hook = br_fdb_put;
#endif
	register_netdevice_notifier(&br_device_notifier);
	// add for SNMP trap 
	create_proc_read_entry("bridge",0,NULL,bridge_read_procmem,NULL);
	return 0;
}

static void __br_clear_ioctl_hook(void)
{
	br_ioctl_hook = NULL;
}

static void __exit br_deinit(void)
{
	unregister_netdevice_notifier(&br_device_notifier);
	br_call_ioctl_atomic(__br_clear_ioctl_hook);

	br_write_lock_bh(BR_NETPROTO_LOCK);
	br_handle_frame_hook = NULL;
	br_write_unlock_bh(BR_NETPROTO_LOCK);

#if defined(CONFIG_ATM_LANE) || defined(CONFIG_ATM_LANE_MODULE)
	br_fdb_get_hook = NULL;
	br_fdb_put_hook = NULL;
#endif
}

#if defined(CONFIG_BRIDGE_EBT_BROUTE) || \
	defined(CONFIG_BRIDGE_EBT_BROUTE_MODULE)
	EXPORT_SYMBOL(broute_decision);
#else
	EXPORT_NO_SYMBOLS;
#endif
	
module_init(br_init)
module_exit(br_deinit)
MODULE_LICENSE("GPL");
