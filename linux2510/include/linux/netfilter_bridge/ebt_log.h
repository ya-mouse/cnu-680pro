#ifndef __LINUX_BRIDGE_EBT_LOG_H
#define __LINUX_BRIDGE_EBT_LOG_H

#define EBT_LOG_IP 0x01 // if the frame is made by ip, log the ip information
#define EBT_LOG_ARP 0x02
#define EBT_LOG_MASK (EBT_LOG_IP | EBT_LOG_ARP)
#define EBT_LOG_PREFIX_SIZE 30
#define EBT_LOG_WATCHER "log"

struct ebt_log_info
{
	__u8 loglevel;
	__u8 prefix[EBT_LOG_PREFIX_SIZE];
	__u32 bitmask;
};

#endif
