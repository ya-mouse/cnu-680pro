#ifndef __LINUX_ISIL_NETLINK_H
#define __LINUX_ISIL_NETLINK_H

#include <linux/types.h>

/*
 * Intersil specific netlink defines 
 */

#define NETLINK_TYPE_PIMFOR			1

/*
 * PIMFOR (Proprietary Intersil Mechanism For Object Relay). PIMFOR is a simple
 * request-response protocol used to query and set items of management information
 * residing in the MVC. It  is used to provide an object interface to device firmware.
 */

/* 
 * PIMFOR header structure. 
 * Used in the communication between kernel-space MVC driver and user-space applications.
 */
struct pimfor_hdr
{
	__u8	version;    /* PIMFOR version. */
	__u8	operation;  /* Operation to perform, i.e. PIMFOR_OP_GET. */
	__u32	oid;        /* Object Identifier, uniquely identifies
                               a variable or managed object in the system. */
	__u8	device_id;  /* Identifies the (MVC) device to access. */
	__u8	flags;      /* Flags: bit 0:    PIMFOR_FLAG_APPLIC_ORIGIN, 
                                      bit 1:    PIMFOR_FLAG_LITTLE_ENDIAN,
                                      bits 2-8: reserved */
	__u32	length;     /* Length of the data following the PIMFOR header */
} __attribute__ ((packed));


#define PIMFOR_DATA(phdr)  ((void*)(((char*)phdr) + sizeof(struct pimfor_hdr)))
#define PIMFOR_HEADER_SIZE                      12

/* PIMFOR package definitions */
#define	PIMFOR_ETHERTYPE			0x8828
#define	PIMFOR_VERSION_1			1

/* PIMFOR operations */
#define	PIMFOR_OP_GET				0
#define	PIMFOR_OP_SET				1
#define	PIMFOR_OP_RESPONSE			2
#define	PIMFOR_OP_ERROR				3
#define	PIMFOR_OP_TRAP				4

/* PIMFOR device IDs */
#define	PIMFOR_DEV_ID_MHLI_MIB			0

/* PIMFOR flags */
#define	PIMFOR_FLAG_APPLIC_ORIGIN		0x01
#define	PIMFOR_FLAG_LITTLE_ENDIAN		0x02

/* PIMFOR Traps that are sent via an ISIL netlink socket are always sent to a
 * netlink multicast group. The following multicast groups are defined
 */
#define TRAPGRP_ETH_TRAPS	1	/* Traps from Ethernet interfaces */
#define TRAPGRP_WLAN_TRAPS	2	/* Traps from Wireless interfaces */
#define TRAPGRP_BLOBDEV_TRAPS	4	/* Generic BLOB device traps */

#endif	/* __LINUX_ISIL_NETLINK_H */
