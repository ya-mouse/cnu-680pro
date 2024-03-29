/*
 *  ebt_log
 *
 *	Authors:
 *	Bart De Schuymer <bart.de.schuymer@pandora.be>
 *
 *  April, 2002
 *
 */

#include <linux/netfilter_bridge/ebtables.h>
#include <linux/netfilter_bridge/ebt_log.h>
#include <linux/module.h>
#include <linux/ip.h>
#include <linux/if_arp.h>
#include <linux/spinlock.h>

static spinlock_t ebt_log_lock = SPIN_LOCK_UNLOCKED;

static int ebt_log_check(const char *tablename, unsigned int hookmask,
   const struct ebt_entry *e, void *data, unsigned int datalen)
{
	struct ebt_log_info *loginfo = (struct ebt_log_info *)data;

	if (datalen != sizeof(struct ebt_log_info))
		return -EINVAL;
	if (loginfo->bitmask & ~EBT_LOG_MASK)
		return -EINVAL;
	if (loginfo->loglevel >= 8)
		return -EINVAL;
	loginfo->prefix[EBT_LOG_PREFIX_SIZE - 1] = '\0';
	return 0;
}

static void ebt_log(const struct sk_buff *skb, const struct net_device *in,
   const struct net_device *out, const void *data, unsigned int datalen,
   const struct ebt_counter *c)
{
	struct ebt_log_info *loginfo = (struct ebt_log_info *)data;
	char level_string[4] = "< >";
	level_string[1] = '0' + loginfo->loglevel;

	spin_lock_bh(&ebt_log_lock);
	printk(level_string);
	// max length: 29 + 10 + 2 * 16
	printk("%s IN=%s OUT=%s ",
	       loginfo->prefix,
	       in ? in->name : "",
	       out ? out->name : "");

	if (skb->dev->hard_header_len) {
		int i;
		unsigned char *p = (skb->mac.ethernet)->h_source;
		printk("MAC source = ");
		for (i = 0; i < ETH_ALEN; i++,p++)
			printk("%02x%c", *p,
			       i == ETH_ALEN - 1
			       ? ' ':':');// length: 31
		printk("MAC dest = ");
		p = (skb->mac.ethernet)->h_dest;
		for (i = 0; i < ETH_ALEN; i++,p++)
			printk("%02x%c", *p,
			       i == ETH_ALEN - 1
			       ? ' ':':');// length: 29
	}
	// length: 14
	printk("proto = 0x%04x", ntohs(((*skb).mac.ethernet)->h_proto));

	if ((loginfo->bitmask & EBT_LOG_IP) && skb->mac.ethernet->h_proto ==
	   htons(ETH_P_IP)){
		struct iphdr *iph = skb->nh.iph;
		// max length: 46
		printk(" IP SRC=%u.%u.%u.%u IP DST=%u.%u.%u.%u,",
		   NIPQUAD(iph->saddr), NIPQUAD(iph->daddr));
		// max length: 26
		printk(" IP tos=0x%02X, IP proto=%d", iph->tos, iph->protocol);
	}

	if ((loginfo->bitmask & EBT_LOG_ARP) &&
	    ((skb->mac.ethernet->h_proto == __constant_htons(ETH_P_ARP)) ||
	    (skb->mac.ethernet->h_proto == __constant_htons(ETH_P_RARP)))) {
		struct arphdr * arph = skb->nh.arph;
		// max length: 40
		printk(" ARP HTYPE=%d, PTYPE=0x%04x, OPCODE=%d",
		   ntohs(arph->ar_hrd), ntohs(arph->ar_pro),
		   ntohs(arph->ar_op));
	}
	printk("\n");
	spin_unlock_bh(&ebt_log_lock);
}

struct ebt_watcher log =
{
	{NULL, NULL}, EBT_LOG_WATCHER, ebt_log, ebt_log_check, NULL,
	THIS_MODULE
};

static int __init init(void)
{
	return ebt_register_watcher(&log);
}

static void __exit fini(void)
{
	ebt_unregister_watcher(&log);
}

module_init(init);
module_exit(fini);
EXPORT_NO_SYMBOLS;
MODULE_LICENSE("GPL");
