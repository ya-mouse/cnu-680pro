/* 
   BlueZ - Bluetooth protocol stack for Linux
   Copyright (C) 2000-2001 Qualcomm Incorporated

   Written 2000,2001 by Maxim Krasnyansky <maxk@qualcomm.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation;

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
   IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY
   CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES 
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN 
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF 
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

   ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS, 
   COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS 
   SOFTWARE IS DISCLAIMED.
*/

/*
 *  $Id: l2cap.h,v 1.1.1.1 2003/11/17 02:35:45 jipark Exp $
 */

#ifndef __L2CAP_H
#define __L2CAP_H

/* L2CAP defaults */
#define L2CAP_DEFAULT_MTU 	672
#define L2CAP_DEFAULT_FLUSH_TO	0xFFFF

#define L2CAP_CONN_TIMEOUT 	(HZ * 40)

/* L2CAP socket address */
struct sockaddr_l2 {
	sa_family_t	l2_family;
	unsigned short	l2_psm;
	bdaddr_t	l2_bdaddr;
};

/* Socket options */
#define L2CAP_OPTIONS	0x01
struct l2cap_options {
	__u16 omtu;
	__u16 imtu;
	__u16 flush_to;
};

#define L2CAP_CONNINFO  0x02
struct l2cap_conninfo {
	__u16 hci_handle;
};

#define L2CAP_LM	0x03
#define L2CAP_LM_MASTER		0x0001
#define L2CAP_LM_AUTH		0x0002
#define L2CAP_LM_ENCRYPT	0x0004
#define L2CAP_LM_TRUSTED	0x0008

#define L2CAP_QOS	0x04
struct l2cap_qos {
	__u16 service_type;
	__u32 token_rate;
	__u32 token_bucket_size;
	__u32 peak_bandwidth;
	__u32 latency;
	__u32 delay_variation;
};

#define L2CAP_SERV_NO_TRAFFIC	0x00
#define L2CAP_SERV_BEST_EFFORT	0x01
#define L2CAP_SERV_GUARANTEED	0x02

/* L2CAP command codes */
#define L2CAP_COMMAND_REJ 0x01
#define L2CAP_CONN_REQ    0x02
#define L2CAP_CONN_RSP    0x03
#define L2CAP_CONF_REQ    0x04
#define L2CAP_CONF_RSP    0x05
#define L2CAP_DISCONN_REQ 0x06
#define L2CAP_DISCONN_RSP 0x07
#define L2CAP_ECHO_REQ    0x08
#define L2CAP_ECHO_RSP    0x09
#define L2CAP_INFO_REQ    0x0a
#define L2CAP_INFO_RSP    0x0b

/* L2CAP structures */
typedef struct {
	__u16      len;
	__u16      cid;
} __attribute__ ((packed)) 	l2cap_hdr;
#define L2CAP_HDR_SIZE		4

typedef struct {
	__u8       code;
	__u8       ident;
	__u16      len;
} __attribute__ ((packed))	l2cap_cmd_hdr;
#define L2CAP_CMD_HDR_SIZE	4

typedef struct {
	__u16      reason;
} __attribute__ ((packed))	l2cap_cmd_rej;
#define L2CAP_CMD_REJ_SIZE	2

typedef struct {
	__u16      psm;
	__u16      scid;
} __attribute__ ((packed))	l2cap_conn_req;
#define L2CAP_CONN_REQ_SIZE	4

typedef struct {
	__u16      dcid;
	__u16      scid;
	__u16      result;
	__u16      status;
} __attribute__ ((packed))	l2cap_conn_rsp;
#define L2CAP_CONN_RSP_SIZE	8

/* connect result */
#define L2CAP_CR_SUCCESS    0x0000
#define L2CAP_CR_PEND       0x0001
#define L2CAP_CR_BAD_PSM    0x0002
#define L2CAP_CR_SEC_BLOCK  0x0003
#define L2CAP_CR_NO_MEM     0x0004

/* connect status */
#define L2CAP_CS_NO_INFO      0x0000
#define L2CAP_CS_AUTHEN_PEND  0x0001
#define L2CAP_CS_AUTHOR_PEND  0x0002

typedef struct {
	__u16      dcid;
	__u16      flags;
	__u8       data[0];
} __attribute__ ((packed))	l2cap_conf_req;
#define L2CAP_CONF_REQ_SIZE	4

typedef struct {
	__u16      scid;
	__u16      flags;
	__u16      result;
	__u8       data[0];
} __attribute__ ((packed))	l2cap_conf_rsp;
#define L2CAP_CONF_RSP_SIZE   	6

#define L2CAP_CONF_SUCCESS	0x00
#define L2CAP_CONF_UNACCEPT	0x01

typedef struct {
	__u8       type;
	__u8       len;
	__u8       val[0];
} __attribute__ ((packed))	l2cap_conf_opt;
#define L2CAP_CONF_OPT_SIZE	2

#define L2CAP_CONF_MTU		0x01
#define L2CAP_CONF_FLUSH_TO	0x02
#define L2CAP_CONF_QOS		0x03

#define L2CAP_CONF_MAX_SIZE	22

typedef struct {
	__u16      dcid;
	__u16      scid;
} __attribute__ ((packed)) 	l2cap_disconn_req;
#define L2CAP_DISCONN_REQ_SIZE	4

typedef struct {
	__u16      dcid;
	__u16      scid;
} __attribute__ ((packed)) 	l2cap_disconn_rsp;
#define L2CAP_DISCONN_RSP_SIZE	4

typedef struct {
	__u16       type;
	__u8        data[0];
} __attribute__ ((packed))	l2cap_info_req;
#define L2CAP_INFO_REQ_SIZE	2

typedef struct {
	__u16       type;
	__u16       result;
	__u8        data[0];
} __attribute__ ((packed))	l2cap_info_rsp;
#define L2CAP_INFO_RSP_SIZE	4

/* ----- L2CAP connections ----- */
struct l2cap_chan_list {
	struct sock	*head;
	rwlock_t	lock;
	long		num;
};

struct l2cap_conn {
	struct hci_conn	*hcon;

	bdaddr_t 	*dst;
	bdaddr_t 	*src;
	
	unsigned int    mtu;

	spinlock_t	lock;
	
	struct sk_buff *rx_skb;
	__u32		rx_len;
	__u8		rx_ident;
	__u8		tx_ident;

	struct l2cap_chan_list chan_list;
};

/* ----- L2CAP channel and socket info ----- */
#define l2cap_pi(sk)   ((struct l2cap_pinfo *) &sk->tp_pinfo)

struct l2cap_pinfo {
	__u16		psm;
	__u16		dcid;
	__u16		scid;

	__u16		imtu;
	__u16		omtu;
	__u16		flush_to;
	
	__u32		link_mode;

	__u8		conf_state;
	__u16		conf_mtu;

	__u8		ident;

	struct l2cap_conn 	*conn;
	struct sock 		*next_c;
	struct sock 		*prev_c;
};

#define CONF_REQ_SENT    0x01
#define CONF_INPUT_DONE  0x02
#define CONF_OUTPUT_DONE 0x04

#endif /* __L2CAP_H */
