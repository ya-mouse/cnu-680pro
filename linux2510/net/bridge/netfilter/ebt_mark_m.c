/*
 *  ebt_mark_m
 *
 *	Authors:
 *	Bart De Schuymer <bart.de.schuymer@pandora.be>
 *
 *  July, 2002
 *
 */

#include <linux/netfilter_bridge/ebtables.h>
#include <linux/netfilter_bridge/ebt_mark_m.h>
#include <linux/module.h>

static int ebt_filter_mark(const struct sk_buff *skb,
   const struct net_device *in, const struct net_device *out, const void *data,
   unsigned int datalen, const struct ebt_counter *c)
{
	struct ebt_mark_m_info *info = (struct ebt_mark_m_info *) data;

	if (info->bitmask & EBT_MARK_OR)
		return !(!!(skb->nfmark & info->mask) ^ info->invert);
	return !(((skb->nfmark & info->mask) == info->mark) ^ info->invert);
}

static int ebt_mark_check(const char *tablename, unsigned int hookmask,
   const struct ebt_entry *e, void *data, unsigned int datalen)
{
        struct ebt_mark_m_info *info = (struct ebt_mark_m_info *) data;

	if (info->bitmask & ~EBT_MARK_MASK)
		return -EINVAL;
	if ((info->bitmask & EBT_MARK_OR) && (info->bitmask & EBT_MARK_AND))
		return -EINVAL;
	if (!info->bitmask)
		return -EINVAL;
	if (datalen != sizeof(struct ebt_mark_m_info)) {
		return -EINVAL;
	}
	return 0;
}

static struct ebt_match filter_mark =
{
	{NULL, NULL}, EBT_MARK_MATCH, ebt_filter_mark, ebt_mark_check, NULL,
	THIS_MODULE
};

static int __init init(void)
{
	return ebt_register_match(&filter_mark);
}

static void __exit fini(void)
{
	ebt_unregister_match(&filter_mark);
}

module_init(init);
module_exit(fini);
EXPORT_NO_SYMBOLS;
MODULE_LICENSE("GPL");
