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
 * $Id: hci_core.h,v 1.1.1.1 2003/11/17 02:35:45 jipark Exp $ 
 */

#ifndef __HCI_CORE_H
#define __HCI_CORE_H

#include <net/bluetooth/hci.h>

/* HCI upper protocols */
#define HCI_PROTO_L2CAP	0
#define HCI_PROTO_SCO	1

#define HCI_INIT_TIMEOUT (HZ * 10)

/* HCI Core structures */

struct inquiry_entry {
	struct inquiry_entry 	*next;
	__u32			timestamp;
	inquiry_info		info;
};

struct inquiry_cache {
	spinlock_t 		lock;
	__u32			timestamp;
	struct inquiry_entry 	*list;
};

struct conn_hash {
	struct list_head list;
	spinlock_t       lock;
	unsigned int     num;
};

struct hci_dev {
	struct list_head list;
	spinlock_t	lock;
	atomic_t 	refcnt;

	char		name[8];
	unsigned long	flags;
	__u16		id;
	__u8	 	type;
	bdaddr_t	bdaddr;
	__u8		features[8];

	__u16		pkt_type;
	__u16		link_policy;
	__u16		link_mode;
	
	atomic_t 	cmd_cnt;
	unsigned int 	acl_cnt;
	unsigned int 	sco_cnt;

	unsigned int	acl_mtu;
	unsigned int 	sco_mtu;
	unsigned int	acl_pkts;
	unsigned int	sco_pkts;

	unsigned long   cmd_last_tx;
	unsigned long   acl_last_tx;
	unsigned long   sco_last_tx;
	
	struct tasklet_struct 	cmd_task;
	struct tasklet_struct	rx_task;
	struct tasklet_struct 	tx_task;

	struct sk_buff_head	rx_q;
	struct sk_buff_head 	raw_q;
	struct sk_buff_head 	cmd_q;

	struct sk_buff     	*sent_cmd;

	struct semaphore	req_lock;
	wait_queue_head_t	req_wait_q;
	__u32			req_status;
	__u32			req_result;

	struct inquiry_cache 	inq_cache;
	struct conn_hash 	conn_hash;

	struct hci_dev_stats 	stat;

	void			*driver_data;
	void			*core_data;

	atomic_t 		promisc;

	int (*open)(struct hci_dev *hdev);
	int (*close)(struct hci_dev *hdev);
	int (*flush)(struct hci_dev *hdev);
	int (*send)(struct sk_buff *skb);
	void (*destruct)(struct hci_dev *hdev);
	int (*ioctl)(struct hci_dev *hdev, unsigned int cmd, unsigned long arg);
};

struct hci_conn {
	struct list_head list;

	atomic_t	 refcnt;
	spinlock_t	 lock;

	bdaddr_t         dst;
	__u16            handle;
	__u16            state;
	__u8		 type;
	__u8		 out;
	__u32		 link_mode;
	unsigned long	 pend;
	
	unsigned int	 sent;
	
	struct sk_buff_head data_q;

	struct timer_list timer;
	
	struct hci_dev 	*hdev;
	void		*l2cap_data;
	void		*sco_data;
	void		*priv;

	struct hci_conn *link;
};

extern struct hci_proto *hci_proto[];
extern struct list_head hdev_list;
extern rwlock_t hdev_list_lock;

/* ----- Inquiry cache ----- */
#define INQUIRY_CACHE_AGE_MAX   (HZ*30)   // 30 seconds
#define INQUIRY_ENTRY_AGE_MAX   (HZ*60)   // 60 seconds

#define inquiry_cache_lock(c)		spin_lock(&c->lock)
#define inquiry_cache_unlock(c)		spin_unlock(&c->lock)
#define inquiry_cache_lock_bh(c)	spin_lock_bh(&c->lock)
#define inquiry_cache_unlock_bh(c)	spin_unlock_bh(&c->lock)

static inline void inquiry_cache_init(struct hci_dev *hdev)
{
	struct inquiry_cache *c = &hdev->inq_cache;
	spin_lock_init(&c->lock);
	c->list = NULL;
}

static inline long inquiry_cache_age(struct hci_dev *hdev)
{
	struct inquiry_cache *c = &hdev->inq_cache;
	return jiffies - c->timestamp;
}

static inline long inquiry_entry_age(struct inquiry_entry *e)
{
	return jiffies - e->timestamp;
}

struct inquiry_entry *inquiry_cache_lookup(struct hci_dev *hdev, bdaddr_t *bdaddr);
void inquiry_cache_update(struct hci_dev *hdev, inquiry_info *info);
void inquiry_cache_flush(struct hci_dev *hdev);
int  inquiry_cache_dump(struct hci_dev *hdev, int num, __u8 *buf);

/* ----- HCI Connections ----- */
enum {
	HCI_CONN_AUTH_PEND,
	HCI_CONN_ENCRYPT_PEND
};

#define hci_conn_lock(c)	spin_lock(&c->lock)
#define hci_conn_unlock(c)	spin_unlock(&c->lock)
#define hci_conn_lock_bh(c)	spin_lock_bh(&c->lock)
#define hci_conn_unlock_bh(c)	spin_unlock_bh(&c->lock)

#define conn_hash_lock(d)	spin_lock(&d->conn_hash->lock)
#define conn_hash_unlock(d)	spin_unlock(&d->conn_hash->lock)
#define conn_hash_lock_bh(d)	spin_lock_bh(&d->conn_hash->lock)
#define conn_hash_unlock_bh(d)	spin_unlock_bh(&d->conn_hash->lock)

static inline void conn_hash_init(struct hci_dev *hdev)
{
	struct conn_hash *h = &hdev->conn_hash;
	INIT_LIST_HEAD(&h->list);
	spin_lock_init(&h->lock);
	h->num = 0;	
}

static inline void conn_hash_add(struct hci_dev *hdev, struct hci_conn *c)
{
	struct conn_hash *h = &hdev->conn_hash;
	list_add(&c->list, &h->list);
	h->num++;
}

static inline void conn_hash_del(struct hci_dev *hdev, struct hci_conn *c)
{
	struct conn_hash *h = &hdev->conn_hash;
	list_del(&c->list);
	h->num--;
}

static inline struct hci_conn *conn_hash_lookup_handle(struct hci_dev *hdev,
	       				__u16 handle)
{
	register struct conn_hash *h = &hdev->conn_hash;
	register struct list_head *p;
	register struct hci_conn  *c;

	list_for_each(p, &h->list) {
		c = list_entry(p, struct hci_conn, list);
		if (c->handle == handle)
			return c;
	}
        return NULL;
}

static inline struct hci_conn *conn_hash_lookup_ba(struct hci_dev *hdev,
					__u8 type, bdaddr_t *ba)
{
	register struct conn_hash *h = &hdev->conn_hash;
	register struct list_head *p;
	register struct hci_conn  *c;

	list_for_each(p, &h->list) {
		c = list_entry(p, struct hci_conn, list);
		if (c->type == type && !bacmp(&c->dst, ba))
			return c;
	}
        return NULL;
}

void hci_acl_connect(struct hci_conn *conn);
void hci_acl_disconn(struct hci_conn *conn, __u8 reason);
void hci_add_sco(struct hci_conn *conn, __u16 handle);

struct hci_conn *hci_conn_add(struct hci_dev *hdev, int type, bdaddr_t *dst);
int    hci_conn_del(struct hci_conn *conn);
void   hci_conn_hash_flush(struct hci_dev *hdev);

struct hci_conn *hci_connect(struct hci_dev *hdev, int type, bdaddr_t *src);
int hci_conn_auth(struct hci_conn *conn);
int hci_conn_encrypt(struct hci_conn *conn);

static inline void hci_conn_set_timer(struct hci_conn *conn, long timeout)
{
	mod_timer(&conn->timer, jiffies + timeout);
}

static inline void hci_conn_del_timer(struct hci_conn *conn)
{
	del_timer(&conn->timer);
}

static inline void hci_conn_hold(struct hci_conn *conn)
{
	atomic_inc(&conn->refcnt);
	hci_conn_del_timer(conn);
}

static inline void hci_conn_put(struct hci_conn *conn)
{
	if (atomic_dec_and_test(&conn->refcnt) && conn->out)
		hci_conn_set_timer(conn, HCI_DISCONN_TIMEOUT);
}

/* ----- HCI Devices ----- */
static inline void hci_dev_put(struct hci_dev *d)
{ 
	if (atomic_dec_and_test(&d->refcnt))
		d->destruct(d);
}
#define hci_dev_hold(d)		atomic_inc(&d->refcnt)

#define hci_dev_lock(d)		spin_lock(&d->lock)
#define hci_dev_unlock(d)	spin_unlock(&d->lock)
#define hci_dev_lock_bh(d)	spin_lock_bh(&d->lock)
#define hci_dev_unlock_bh(d)	spin_unlock_bh(&d->lock)

struct hci_dev *hci_dev_get(int index);
struct hci_dev *hci_get_route(bdaddr_t *src, bdaddr_t *dst);
int hci_register_dev(struct hci_dev *hdev);
int hci_unregister_dev(struct hci_dev *hdev);
int hci_dev_open(__u16 dev);
int hci_dev_close(__u16 dev);
int hci_dev_reset(__u16 dev);
int hci_dev_reset_stat(__u16 dev);
int hci_dev_cmd(unsigned int cmd, unsigned long arg);
int hci_get_dev_list(unsigned long arg);
int hci_get_dev_info(unsigned long arg);
int hci_get_conn_list(unsigned long arg);
int hci_get_conn_info(struct hci_dev *hdev, unsigned long arg);
int hci_inquiry(unsigned long arg);

int  hci_recv_frame(struct sk_buff *skb);
void hci_event_packet(struct hci_dev *hdev, struct sk_buff *skb);

/* ----- LMP capabilities ----- */
#define lmp_rswitch_capable(dev) (dev->features[0] & LMP_RSWITCH)
#define lmp_encrypt_capable(dev) (dev->features[0] & LMP_ENCRYPT)

/* ----- HCI tasks ----- */
static inline void hci_sched_cmd(struct hci_dev *hdev)
{
	tasklet_schedule(&hdev->cmd_task);
}

static inline void hci_sched_rx(struct hci_dev *hdev)
{
	tasklet_schedule(&hdev->rx_task);
}

static inline void hci_sched_tx(struct hci_dev *hdev)
{
	tasklet_schedule(&hdev->tx_task);
}

/* ----- HCI protocols ----- */
struct hci_proto {
	char 		*name;
	unsigned int 	id;
	unsigned long	flags;

	void		*priv;

	int (*connect_ind) 	(struct hci_dev *hdev, bdaddr_t *bdaddr, __u8 type);
	int (*connect_cfm)	(struct hci_conn *conn, __u8 status);
	int (*disconn_ind)	(struct hci_conn *conn, __u8 reason);
	int (*recv_acldata)	(struct hci_conn *conn, struct sk_buff *skb, __u16 flags);
	int (*recv_scodata)	(struct hci_conn *conn, struct sk_buff *skb);
	int (*auth_cfm)		(struct hci_conn *conn, __u8 status);
	int (*encrypt_cfm)	(struct hci_conn *conn, __u8 status);
};

static inline int hci_proto_connect_ind(struct hci_dev *hdev, bdaddr_t *bdaddr, __u8 type)
{
	register struct hci_proto *hp;
	int mask = 0;
	
	hp = hci_proto[HCI_PROTO_L2CAP];
	if (hp && hp->connect_ind)
		mask |= hp->connect_ind(hdev, bdaddr, type);

	hp = hci_proto[HCI_PROTO_SCO];
	if (hp && hp->connect_ind)
		mask |= hp->connect_ind(hdev, bdaddr, type);

	return mask;
}

static inline void hci_proto_connect_cfm(struct hci_conn *conn, __u8 status)
{
	register struct hci_proto *hp;

	hp = hci_proto[HCI_PROTO_L2CAP];
	if (hp && hp->connect_cfm)
		hp->connect_cfm(conn, status);

	hp = hci_proto[HCI_PROTO_SCO];
	if (hp && hp->connect_cfm)
		hp->connect_cfm(conn, status);
}

static inline void hci_proto_disconn_ind(struct hci_conn *conn, __u8 reason)
{
	register struct hci_proto *hp;

	hp = hci_proto[HCI_PROTO_L2CAP];
	if (hp && hp->disconn_ind)
		hp->disconn_ind(conn, reason);

	hp = hci_proto[HCI_PROTO_SCO];
	if (hp && hp->disconn_ind)
		hp->disconn_ind(conn, reason);
}

static inline void hci_proto_auth_cfm(struct hci_conn *conn, __u8 status)
{
	register struct hci_proto *hp;

	hp = hci_proto[HCI_PROTO_L2CAP];
	if (hp && hp->auth_cfm)
		hp->auth_cfm(conn, status);

	hp = hci_proto[HCI_PROTO_SCO];
	if (hp && hp->auth_cfm)
		hp->auth_cfm(conn, status);
}

static inline void hci_proto_encrypt_cfm(struct hci_conn *conn, __u8 status)
{
	register struct hci_proto *hp;

	hp = hci_proto[HCI_PROTO_L2CAP];
	if (hp && hp->encrypt_cfm)
		hp->encrypt_cfm(conn, status);

	hp = hci_proto[HCI_PROTO_SCO];
	if (hp && hp->encrypt_cfm)
		hp->encrypt_cfm(conn, status);
}

int hci_register_proto(struct hci_proto *hproto);
int hci_unregister_proto(struct hci_proto *hproto);
int hci_register_notifier(struct notifier_block *nb);
int hci_unregister_notifier(struct notifier_block *nb);

int hci_send_cmd(struct hci_dev *hdev, __u16 ogf, __u16 ocf, __u32 plen, void *param);
int hci_send_acl(struct hci_conn *conn, struct sk_buff *skb, __u16 flags);
int hci_send_sco(struct hci_conn *conn, struct sk_buff *skb);
int hci_send_raw(struct sk_buff *skb);

void *hci_sent_cmd_data(struct hci_dev *hdev, __u16 ogf, __u16 ocf);

void hci_si_event(struct hci_dev *hdev, int type, int dlen, void *data);

/* ----- HCI Sockets ----- */
void hci_send_to_sock(struct hci_dev *hdev, struct sk_buff *skb);

/* HCI info for socket */
#define hci_pi(sk)	((struct hci_pinfo *) &sk->tp_pinfo)
struct hci_pinfo {
	struct hci_dev 	  *hdev;
	struct hci_filter filter;
	__u32             cmsg_mask;
};

/* HCI security filter */
#define HCI_SFLT_MAX_OGF 4

struct hci_sec_filter {
	__u32 type_mask;
	__u32 event_mask[2];
	__u32 ocf_mask[HCI_SFLT_MAX_OGF + 1][4];
};


/* ----- HCI requests ----- */
#define HCI_REQ_DONE	  0
#define HCI_REQ_PEND	  1
#define HCI_REQ_CANCELED  2

#define hci_req_lock(d)		down(&d->req_lock)
#define hci_req_unlock(d)	up(&d->req_lock)

void hci_req_complete(struct hci_dev *hdev, int result);
void hci_req_cancel(struct hci_dev *hdev, int err);

#endif /* __HCI_CORE_H */
