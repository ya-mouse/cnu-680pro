/*
 * Intersil Prism2 driver with Host AP (software access point) support
 * Copyright (c) 2001-2002, SSH Communications Security Corp and Jouni Malinen
 * <jkmaline@cc.hut.fi>
 * Copyright (c) 2002-2003, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This file is to be included into hostap.c when S/W AP functionality is
 * compiled.
 *
 * AP:  FIX:
 * - if unicast Class 2 (assoc,reassoc,disassoc) frame received from
 *   unauthenticated STA, send deauth. frame (8802.11: 5.5)
 * - if unicast Class 3 (data with to/from DS,deauth,pspoll) frame received
 *   from authenticated, but unassoc STA, send disassoc frame (8802.11: 5.5)
 * - if unicast Class 3 received from unauthenticated STA, send deauth. frame
 *   (8802.11: 5.5)
 */

static int other_ap_policy[MAX_PARM_DEVICES] = { AP_OTHER_AP_SKIP_ALL,
						 DEF_INTS };
MODULE_PARM(other_ap_policy, PARM_MIN_MAX "i");
MODULE_PARM_DESC(other_ap_policy, "Other AP beacon monitoring policy (0-3)");

static int ap_max_inactivity[MAX_PARM_DEVICES] = { AP_MAX_INACTIVITY / HZ,
						   DEF_INTS };
MODULE_PARM(ap_max_inactivity, PARM_MIN_MAX "i");
MODULE_PARM_DESC(ap_max_inactivity, "AP timeout (in seconds) for station "
		 "inactivity");

static int ap_bridge_packets[MAX_PARM_DEVICES] = { 1, DEF_INTS };
MODULE_PARM(ap_bridge_packets, PARM_MIN_MAX "i");
MODULE_PARM_DESC(ap_bridge_packets, "Bridge packets directly between "
		 "stations");

static int autom_ap_wds[MAX_PARM_DEVICES] = { 0, DEF_INTS };
MODULE_PARM(autom_ap_wds, PARM_MIN_MAX "i");
MODULE_PARM_DESC(autom_ap_wds, "Add WDS connections to other APs "
		 "automatically");


static void prism2_ap_update_sq(struct sta_info *sta,
				struct hfa384x_rx_frame *rxdesc);
static struct sta_info* ap_get_sta(struct ap_data *ap, u8 *sta);
static void hostap_event_expired_sta(struct net_device *dev,
				     struct sta_info *sta);
static void handle_add_proc_queue(void *data);

#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
static void handle_add_wds_queue(void *data);
static void prism2_send_mgmt(struct net_device *dev,
			     int type, int subtype, char *body,
			     int body_len, int txevent, u8 *addr,
			     u16 tx_cb_idx);
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */


#ifndef PRISM2_NO_PROCFS_DEBUG
static int ap_debug_proc_read(char *page, char **start, off_t off,
			      int count, int *eof, void *data)
{
	char *p = page;
	struct ap_data *ap = (struct ap_data *) data;

	if (off != 0) {
		*eof = 1;
		return 0;
	}

	p += sprintf(p, "BridgedUnicastFrames=%u\n", ap->bridged_unicast);
	p += sprintf(p, "BridgedMulticastFrames=%u\n", ap->bridged_multicast);
	p += sprintf(p, "max_inactivity=%u\n", ap->max_inactivity / HZ);
	p += sprintf(p, "bridge_packets=%u\n", ap->bridge_packets);
	p += sprintf(p, "nullfunc_ack=%u\n", ap->nullfunc_ack);
	p += sprintf(p, "autom_ap_wds=%u\n", ap->autom_ap_wds);
	p += sprintf(p, "auth_algs=%u\n", ap->auth_algs);

	return (p - page);
}
#endif /* PRISM2_NO_PROCFS_DEBUG */


static void ap_sta_hash_add(struct ap_data *ap, struct sta_info *sta)
{
	sta->hnext = ap->sta_hash[STA_HASH(sta->addr)];
	ap->sta_hash[STA_HASH(sta->addr)] = sta;
}

static void ap_sta_hash_del(struct ap_data *ap, struct sta_info *sta)
{
	struct sta_info *s;

	s = ap->sta_hash[STA_HASH(sta->addr)];
	if (s == NULL) return;
	if (memcmp(s->addr, sta->addr, 6) == 0) {
		ap->sta_hash[STA_HASH(sta->addr)] = s->hnext;
		return;
	}

	while (s->hnext != NULL && memcmp(s->hnext->addr, sta->addr, 6) != 0)
		s = s->hnext;
	if (s->hnext != NULL)
		s->hnext = s->hnext->hnext;
	else
		printk("AP: could not remove STA " MACSTR " from hash table\n",
		       MAC2STR(sta->addr));
}

static void ap_free_sta(struct ap_data *ap, struct sta_info *sta)
{
	struct sk_buff *skb;

	if (sta->ap && sta->local)
		hostap_event_expired_sta(sta->local->dev, sta);

	if (ap->proc != NULL) {
		char name[20];
		sprintf(name, MACSTR, MAC2STR(sta->addr));
		remove_proc_entry(name, ap->proc);
	}

	if (sta->crypt) {
		sta->crypt->ops->deinit(sta->crypt->priv);
		kfree(sta->crypt);
		sta->crypt = NULL;
	}

	while ((skb = skb_dequeue(&sta->tx_buf)) != NULL)
		dev_kfree_skb(skb);

	ap->num_sta--;
#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
	if (sta->aid > 0)
		ap->sta_aid[sta->aid - 1] = NULL;

	if (!sta->ap && sta->u.sta.challenge)
		kfree(sta->u.sta.challenge);
	del_timer(&sta->timer);
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */

	kfree(sta);
}


struct set_tim_data {
	struct list_head list;
	int aid;
	int set;
};

static void hostap_set_tim(local_info_t *local, int aid, int set)
{
	struct list_head *ptr;
	struct set_tim_data *new_entry;

	new_entry = (struct set_tim_data *)
		kmalloc(sizeof(*new_entry), GFP_ATOMIC);
	if (new_entry == NULL) {
		printk(KERN_DEBUG "%s: hostap_set_tim: kmalloc failed\n",
		       local->dev->name);
		return;
	}
	memset(new_entry, 0, sizeof(*new_entry));
	new_entry->aid = aid;
	new_entry->set = set;

	spin_lock_bh(&local->ap->set_tim_lock);
	for (ptr = local->ap->set_tim_list.next;
	     ptr != &local->ap->set_tim_list;
	     ptr = ptr->next) {
		struct set_tim_data *entry = (struct set_tim_data *) ptr;
		if (entry->aid == aid) {
			PDEBUG(DEBUG_PS2, "%s: hostap_set_tim: aid=%d "
			       "set=%d ==> %d\n",
			       local->dev->name, aid, entry->set, set);
			entry->set = set;
			kfree(new_entry);
			new_entry = NULL;
			break;
		}
	}
	if (new_entry)
		list_add_tail(&new_entry->list, &local->ap->set_tim_list);
	spin_unlock_bh(&local->ap->set_tim_lock);

	PRISM2_SCHEDULE_TASK(&local->ap->set_tim_queue);
}


static void handle_set_tim_queue(void *data)
{
	local_info_t *local = (local_info_t *) data;
	struct set_tim_data *entry;
	u16 val;

	for (;;) {
		entry = NULL;
		spin_lock_bh(&local->ap->set_tim_lock);
		if (!list_empty(&local->ap->set_tim_list)) {
			entry = list_entry(local->ap->set_tim_list.next,
					   struct set_tim_data, list);
			list_del(&entry->list);
		}
		spin_unlock_bh(&local->ap->set_tim_lock);
		if (!entry)
			break;

		PDEBUG(DEBUG_PS2, "%s: hostap_set_tim_queue: aid=%d set=%d\n",
		       local->dev->name, entry->aid, entry->set);

		val = entry->aid;
		if (entry->set)
			val |= 0x8000;
		if (hostap_set_word(local->dev, HFA384X_RID_CNFTIMCTRL, val)) {
			printk(KERN_DEBUG "%s: set_tim failed (aid=%d "
			       "set=%d)\n",
			       local->dev->name, entry->aid, entry->set);
		}

		kfree(entry);
	}

#ifndef NEW_MODULE_CODE
	MOD_DEC_USE_COUNT;
#endif
}


static void hostap_event_new_sta(struct net_device *dev, struct sta_info *sta)
{
#if WIRELESS_EXT >= 15
	union iwreq_data wrqu;
	memset(&wrqu, 0, sizeof(wrqu));
	memcpy(wrqu.addr.sa_data, sta->addr, ETH_ALEN);
	wrqu.addr.sa_family = ARPHRD_ETHER;
	wireless_send_event(dev, IWEVREGISTERED, &wrqu, NULL);
#endif /* WIRELESS_EXT >= 15 */
}


static void hostap_event_expired_sta(struct net_device *dev,
				     struct sta_info *sta)
{
#if WIRELESS_EXT >= 15
	union iwreq_data wrqu;
	memset(&wrqu, 0, sizeof(wrqu));
	memcpy(wrqu.addr.sa_data, sta->addr, ETH_ALEN);
	wrqu.addr.sa_family = ARPHRD_ETHER;
	wireless_send_event(dev, IWEVEXPIRED, &wrqu, NULL);
#endif /* WIRELESS_EXT >= 15 */
}


#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT

static void ap_handle_timer(unsigned long data)
{
	struct sta_info *sta = (struct sta_info *) data;
	local_info_t *local;
	struct ap_data *ap;
	unsigned long next_time = 0;
	int was_assoc;

	if (sta == NULL || sta->local == NULL || sta->local->ap == NULL) {
		PDEBUG(DEBUG_AP, "ap_handle_timer() called with NULL data\n");
		return;
	}

	local = sta->local;
	ap = local->ap;
	was_assoc = sta->flags & WLAN_STA_ASSOC;

	if (atomic_read(&sta->users) != 0)
		next_time = jiffies + HZ;
	else if ((sta->flags & WLAN_STA_PERM) && !(sta->flags & WLAN_STA_AUTH))
		next_time = jiffies + ap->max_inactivity;

	if (sta->last_rx + ap->max_inactivity > jiffies) {
		/* station activity detected; reset timeout state */
		sta->timeout_next = STA_NULLFUNC;
		next_time = sta->last_rx + ap->max_inactivity;
	} else if (sta->timeout_next == STA_DISASSOC && sta->txexc == 0) {
		/* data nullfunc frame poll did not produce TX errors; assume
		 * station ACKed it */
		sta->timeout_next = STA_NULLFUNC;
		next_time = jiffies + ap->max_inactivity;
	}

	if (next_time) {
		sta->timer.expires = next_time;
		add_timer(&sta->timer);
		return;
	}

	if (sta->ap)
		sta->timeout_next = STA_DEAUTH;

	if (sta->timeout_next == STA_DEAUTH && !(sta->flags & WLAN_STA_PERM)) {
		spin_lock(&ap->sta_table_lock);
		ap_sta_hash_del(ap, sta);
		list_del(&sta->list);
		spin_unlock(&ap->sta_table_lock);
		sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC);
	} else if (sta->timeout_next == STA_DISASSOC)
		sta->flags &= ~WLAN_STA_ASSOC;

	if (was_assoc && !(sta->flags & WLAN_STA_ASSOC) && !sta->ap)
		hostap_event_expired_sta(local->dev, sta);

	if (sta->timeout_next == STA_DEAUTH && sta->aid > 0 &&
	    !skb_queue_empty(&sta->tx_buf)) {
		hostap_set_tim(local, sta->aid, 0);
		sta->flags &= ~WLAN_STA_TIM;
	}

	if (sta->ap) {
		if (ap->autom_ap_wds) {
			PDEBUG(DEBUG_AP, "%s: removing automatic WDS "
			       "connection to AP " MACSTR "\n",
			       local->dev->name, MAC2STR(sta->addr));
			prism2_wds_del(local, sta->addr, 0, 1);
		}
	} else if (sta->timeout_next == STA_NULLFUNC) {
		/* send data frame to poll STA and check whether this frame
		 * is ACKed */
		sta->txexc = 0;
		/* FIX: WLAN_FC_STYPE_NULLFUNC would be more appropriate, but
		 * it is apparently not retried so TX Exc events are not
		 * received for it */
		prism2_send_mgmt(local->dev, WLAN_FC_TYPE_DATA,
				 WLAN_FC_STYPE_DATA, NULL, 0, 1,
				 sta->addr, 0);
	} else {
		int deauth = sta->timeout_next == STA_DEAUTH;
		u16 resp;
		PDEBUG(DEBUG_AP, "%s: sending %s info to STA " MACSTR
		       "(last=%lu, jiffies=%lu)\n",
		       local->dev->name,
		       deauth ? "deauthentication" : "disassociation",
		       MAC2STR(sta->addr), sta->last_rx, jiffies);

		resp = cpu_to_le16(deauth ? WLAN_REASON_PREV_AUTH_NOT_VALID :
				   WLAN_REASON_DISASSOC_DUE_TO_INACTIVITY);
		prism2_send_mgmt(local->dev, WLAN_FC_TYPE_MGMT,
				 (deauth ? WLAN_FC_STYPE_DEAUTH :
				  WLAN_FC_STYPE_DISASSOC),
				 (char *) &resp, 2, 1, sta->addr, 0);
	}

	if (sta->timeout_next == STA_DEAUTH) {
		if (sta->flags & WLAN_STA_PERM) {
			PDEBUG(DEBUG_AP, "%s: STA " MACSTR " would have been "
			       "removed, but it has 'perm' flag\n",
			       local->dev->name, MAC2STR(sta->addr));
		} else
			ap_free_sta(ap, sta);
		return;
	}

	if (sta->timeout_next == STA_NULLFUNC) {
		sta->timeout_next = STA_DISASSOC;
		sta->timer.expires = jiffies + AP_DISASSOC_DELAY;
	} else {
		sta->timeout_next = STA_DEAUTH;
		sta->timer.expires = jiffies + AP_DEAUTH_DELAY;
	}

	add_timer(&sta->timer);
}


void hostap_deauth_all_stas(struct net_device *dev, struct ap_data *ap,
			    int resend)
{
	u8 addr[ETH_ALEN];
	u16 resp;
	int i;

	PDEBUG(DEBUG_AP, "%s: Deauthenticate all stations\n", dev->name);
	memset(addr, 0xff, ETH_ALEN);

	resp = __constant_cpu_to_le16(WLAN_REASON_PREV_AUTH_NOT_VALID);

	/* deauth message sent; try to resend it few times; the message is
	 * broadcast, so it may be delayed until next DTIM; there is not much
	 * else we can do at this point since the driver is going to be shut
	 * down */
	for (i = 0; i < 5; i++) {
		prism2_send_mgmt(dev, WLAN_FC_TYPE_MGMT, WLAN_FC_STYPE_DEAUTH,
				 (char *) &resp, 2, 1, addr, 0);

		if (!resend || ap->num_sta <= 0)
			return;

		mdelay(50);
	}
}


static int ap_control_proc_read(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	char *p = page;
	struct ap_data *ap = (struct ap_data *) data;
	char *policy_txt;
	struct list_head *ptr;
	struct mac_entry *entry;

	if (off != 0) {
		*eof = 1;
		return 0;
	}

	switch (ap->mac_restrictions.policy) {
	case MAC_POLICY_OPEN:
		policy_txt = "open";
		break;
	case MAC_POLICY_ALLOW:
		policy_txt = "allow";
		break;
	case MAC_POLICY_DENY:
		policy_txt = "deny";
		break;
	default:
		policy_txt = "unknown";
		break;
	};
	p += sprintf(p, "MAC policy: %s\n", policy_txt);
	p += sprintf(p, "MAC entries: %u\n", ap->mac_restrictions.entries);
	p += sprintf(p, "MAC list:\n");
	spin_lock_bh(&ap->mac_restrictions.lock);
	for (ptr = ap->mac_restrictions.mac_list.next;
	     ptr != &ap->mac_restrictions.mac_list; ptr = ptr->next) {
		if (p - page > PAGE_SIZE - 80) {
			p += sprintf(p, "All entries did not fit one page.\n");
			break;
		}

		entry = list_entry(ptr, struct mac_entry, list);
		p += sprintf(p, MACSTR "\n", MAC2STR(entry->addr));
	}
	spin_unlock_bh(&ap->mac_restrictions.lock);

	return (p - page);
}


static int ap_control_add_mac(struct mac_restrictions *mac_restrictions,
			      u8 *mac)
{
	struct mac_entry *entry;

	entry = kmalloc(sizeof(struct mac_entry), GFP_KERNEL);
	if (entry == NULL)
		return -1;

	memcpy(entry->addr, mac, 6);

	spin_lock_bh(&mac_restrictions->lock);
	list_add_tail(&entry->list, &mac_restrictions->mac_list);
	mac_restrictions->entries++;
	spin_unlock_bh(&mac_restrictions->lock);

	return 0;
}


static int ap_control_del_mac(struct mac_restrictions *mac_restrictions,
			      u8 *mac)
{
	struct list_head *ptr;
	struct mac_entry *entry;

	spin_lock_bh(&mac_restrictions->lock);
	for (ptr = mac_restrictions->mac_list.next;
	     ptr != &mac_restrictions->mac_list; ptr = ptr->next) {
		entry = list_entry(ptr, struct mac_entry, list);

		if (memcmp(entry->addr, mac, 6) == 0) {
			list_del(ptr);
			kfree(entry);
			mac_restrictions->entries--;
			spin_unlock_bh(&mac_restrictions->lock);
			return 0;
		}
	}
	spin_unlock_bh(&mac_restrictions->lock);
	return -1;
}


static int ap_control_mac_deny(struct mac_restrictions *mac_restrictions,
			       u8 *mac)
{
	struct list_head *ptr;
	struct mac_entry *entry;
	int found = 0;

	if (mac_restrictions->policy == MAC_POLICY_OPEN)
		return 0;

	spin_lock_bh(&mac_restrictions->lock);
	for (ptr = mac_restrictions->mac_list.next;
	     ptr != &mac_restrictions->mac_list; ptr = ptr->next) {
		entry = list_entry(ptr, struct mac_entry, list);

		if (memcmp(entry->addr, mac, 6) == 0) {
			found = 1;
			break;
		}
	}
	spin_unlock_bh(&mac_restrictions->lock);

	if (mac_restrictions->policy == MAC_POLICY_ALLOW)
		return !found;
	else
		return found;
}


static void ap_control_flush_macs(struct mac_restrictions *mac_restrictions)
{
	struct list_head *ptr, *n;
	struct mac_entry *entry;

	if (mac_restrictions->entries == 0)
		return;

	spin_lock_bh(&mac_restrictions->lock);
	for (ptr = mac_restrictions->mac_list.next, n = ptr->next;
	     ptr != &mac_restrictions->mac_list;
	     ptr = n, n = ptr->next) {
		entry = list_entry(ptr, struct mac_entry, list);
		list_del(ptr);
		kfree(entry);
	}
	mac_restrictions->entries = 0;
	spin_unlock_bh(&mac_restrictions->lock);
}


static int ap_control_kick_mac(struct ap_data *ap, struct net_device *dev,
			       u8 *mac)
{
	struct sta_info *sta;
	u16 resp;

	spin_lock_bh(&ap->sta_table_lock);
	sta = ap_get_sta(ap, mac);
	if (sta) {
		ap_sta_hash_del(ap, sta);
		list_del(&sta->list);
	}
	spin_unlock_bh(&ap->sta_table_lock);

	if (!sta)
		return -EINVAL;

	resp = cpu_to_le16(WLAN_REASON_PREV_AUTH_NOT_VALID);
	prism2_send_mgmt(dev, WLAN_FC_TYPE_MGMT, WLAN_FC_STYPE_DEAUTH,
			 (char *) &resp, 2, 1, sta->addr, 0);

	if ((sta->flags & WLAN_STA_ASSOC) && !sta->ap)
		hostap_event_expired_sta(dev, sta);

	ap_free_sta(ap, sta);

	return 0;
}

#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */


static void ap_control_kickall(struct ap_data *ap)
{
	struct list_head *ptr, *n;
	struct sta_info *sta;
  
	spin_lock_bh(&ap->sta_table_lock);
	for (ptr = ap->sta_list.next, n = ptr->next; ptr != &ap->sta_list;
	     ptr = n, n = ptr->next) {
		sta = list_entry(ptr, struct sta_info, list);
		ap_sta_hash_del(ap, sta);
		list_del(&sta->list);
		if ((sta->flags & WLAN_STA_ASSOC) && !sta->ap && sta->local)
			hostap_event_expired_sta(sta->local->dev, sta);
		ap_free_sta(ap, sta);
	}
	spin_unlock_bh(&ap->sta_table_lock);
}


#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT

#define PROC_LIMIT (PAGE_SIZE - 80)

static int prism2_ap_proc_read(char *page, char **start, off_t off,
			       int count, int *eof, void *data)
{
	char *p = page;
	struct ap_data *ap = (struct ap_data *) data;
	struct list_head *ptr;
	int i;

	if (off > PROC_LIMIT) {
		*eof = 1;
		return 0;
	}

	p += sprintf(p, "# BSSID CHAN SIGNAL NOISE RATE SSID FLAGS\n");
	spin_lock_bh(&ap->sta_table_lock);
	for (ptr = ap->sta_list.next; ptr != &ap->sta_list; ptr = ptr->next) {
		struct sta_info *sta = (struct sta_info *) ptr;

		if (!sta->ap)
			continue;

		p += sprintf(p, MACSTR " %d %d %d %d '", MAC2STR(sta->addr),
			     sta->u.ap.channel, sta->last_rx_signal,
			     sta->last_rx_silence, sta->last_rx_rate);
		for (i = 0; i < sta->u.ap.ssid_len; i++)
			p += sprintf(p, ((sta->u.ap.ssid[i] >= 32 &&
					  sta->u.ap.ssid[i] < 127) ?
					 "%c" : "<%02x>"),
				     sta->u.ap.ssid[i]);
		p += sprintf(p, "'");
		if (sta->capability & WLAN_CAPABILITY_ESS)
			p += sprintf(p, " [ESS]");
		if (sta->capability & WLAN_CAPABILITY_IBSS)
			p += sprintf(p, " [IBSS]");
		if (sta->capability & WLAN_CAPABILITY_PRIVACY)
			p += sprintf(p, " [WEP]");
		p += sprintf(p, "\n");

		if ((p - page) > PROC_LIMIT) {
			printk(KERN_DEBUG "hostap: ap proc did not fit\n");
			break;
		}
	}
	spin_unlock_bh(&ap->sta_table_lock);

	if ((p - page) <= off) {
		*eof = 1;
		return 0;
	}

	*start = page + off;

	return (p - page - off);
}
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */


void hostap_check_sta_fw_version(struct ap_data *ap, int major, int minor,
				 int variant)
{
	if (!ap)
		return;

	if (major == 0 && minor == 8 && variant == 0) {
		PDEBUG(DEBUG_AP, "Using data::nullfunc ACK workaround - "
		       "firmware upgrade recommended\n");
		ap->nullfunc_ack = 1;
	} else
		ap->nullfunc_ack = 0;

	if (major == 1 && minor == 4 && variant == 2) {
		printk(KERN_WARNING "%s: Warning: secondary station firmware "
		       "version 1.4.2 does not seem to work in Host AP mode\n",
		       ap->local->dev->name);
	}
}


/* Called only as a tasklet (software IRQ) */
static void hostap_ap_tx_cb(struct sk_buff *skb, int ok, void *data)
{
	struct ap_data *ap = data;
	u16 fc;
	struct hostap_ieee80211_hdr *hdr;

	if (!ap->local->hostapd || !ap->local->apdev) {
		dev_kfree_skb(skb);
		return;
	}

	hdr = (struct hostap_ieee80211_hdr *) skb->data;
	fc = le16_to_cpu(hdr->frame_control);

	/* Pass the TX callback frame to the hostapd; use 802.11 header version
	 * 1 to indicate failure (no ACK) and 2 success (frame ACKed) */

	fc &= ~WLAN_FC_PVER;
	fc |= ok ? BIT(1) : BIT(0);
	hdr->frame_control = cpu_to_le16(fc);

	skb->dev = ap->local->apdev;
	skb_pull(skb, hostap_80211_get_hdrlen(fc));
	skb->pkt_type = PACKET_OTHERHOST;
	skb->protocol = __constant_htons(ETH_P_802_2);
	memset(skb->cb, 0, sizeof(skb->cb));
	netif_rx(skb);
}


#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
/* Called only as a tasklet (software IRQ) */
static void hostap_ap_tx_cb_auth(struct sk_buff *skb, int ok, void *data)
{
	struct ap_data *ap = data;
	struct net_device *dev = ap->local->dev;
	struct hostap_ieee80211_hdr *hdr;
	u16 fc, *pos, auth_alg, auth_transaction, status;
	struct sta_info *sta = NULL;
	char *txt = NULL;

	if (ap->local->hostapd) {
		dev_kfree_skb(skb);
		return;
	}

	hdr = (struct hostap_ieee80211_hdr *) skb->data;
	fc = le16_to_cpu(hdr->frame_control);
	if (WLAN_FC_GET_TYPE(fc) != WLAN_FC_TYPE_MGMT ||
	    WLAN_FC_GET_STYPE(fc) != WLAN_FC_STYPE_AUTH ||
	    skb->len < 24 + 6) {
		printk(KERN_DEBUG "%s: hostap_ap_tx_cb_auth received invalid "
		       "frame\n", dev->name);
		dev_kfree_skb(skb);
		return;
	}

	if (!ok) {
		txt = "frame was not ACKed";
		goto done;
	}

	spin_lock(&ap->sta_table_lock);
	sta = ap_get_sta(ap, hdr->addr1);
	if (sta)
		atomic_inc(&sta->users);
	spin_unlock(&ap->sta_table_lock);

	if (!sta) {
		txt = "STA not found";
		goto done;
	}

	pos = (u16 *) (skb->data + 24);
	auth_alg = le16_to_cpu(*pos++);
	auth_transaction = le16_to_cpu(*pos++);
	status = le16_to_cpu(*pos++);
	if (status == WLAN_STATUS_SUCCESS &&
	    ((auth_alg == WLAN_AUTH_OPEN && auth_transaction == 2) ||
	     (auth_alg == WLAN_AUTH_SHARED_KEY && auth_transaction == 4))) {
		txt = "STA authenticated";
		sta->flags |= WLAN_STA_AUTH;
		sta->last_auth = jiffies;
	} else if (status != WLAN_STATUS_SUCCESS)
		txt = "authentication failed";

 done:
	if (sta)
		atomic_dec(&sta->users);
	if (txt) {
		PDEBUG(DEBUG_AP, "%s: " MACSTR " auth_cb - %s\n",
		       dev->name, MAC2STR(hdr->addr1), txt);
	}
	dev_kfree_skb(skb);
}


/* Called only as a tasklet (software IRQ) */
static void hostap_ap_tx_cb_assoc(struct sk_buff *skb, int ok, void *data)
{
	struct ap_data *ap = data;
	struct net_device *dev = ap->local->dev;
	struct hostap_ieee80211_hdr *hdr;
	u16 fc, *pos, status;
	struct sta_info *sta = NULL;
	char *txt = NULL;

	if (ap->local->hostapd) {
		dev_kfree_skb(skb);
		return;
	}

	hdr = (struct hostap_ieee80211_hdr *) skb->data;
	fc = le16_to_cpu(hdr->frame_control);
	if (WLAN_FC_GET_TYPE(fc) != WLAN_FC_TYPE_MGMT ||
	    (WLAN_FC_GET_STYPE(fc) != WLAN_FC_STYPE_ASSOC_RESP &&
	     WLAN_FC_GET_STYPE(fc) != WLAN_FC_STYPE_REASSOC_RESP) ||
	    skb->len < 24 + 4) {
		printk(KERN_DEBUG "%s: hostap_ap_tx_cb_assoc received invalid "
		       "frame\n", dev->name);
		dev_kfree_skb(skb);
		return;
	}

	if (!ok) {
		txt = "frame was not ACKed";
		goto done;
	}

	spin_lock(&ap->sta_table_lock);
	sta = ap_get_sta(ap, hdr->addr1);
	if (sta)
		atomic_inc(&sta->users);
	spin_unlock(&ap->sta_table_lock);

	if (!sta) {
		txt = "STA not found";
		goto done;
	}

	pos = (u16 *) (skb->data + 24);
	pos++;
	status = le16_to_cpu(*pos++);
	if (status == WLAN_STATUS_SUCCESS) {
		if (!(sta->flags & WLAN_STA_ASSOC))
			hostap_event_new_sta(dev, sta);
		txt = "STA associated";
		sta->flags |= WLAN_STA_ASSOC;
		sta->last_assoc = jiffies;
	} else
		txt = "association failed";

 done:
	if (sta)
		atomic_dec(&sta->users);
	if (txt) {
		PDEBUG(DEBUG_AP, "%s: " MACSTR " assoc_cb - %s\n",
		       dev->name, MAC2STR(hdr->addr1), txt);
	}
	dev_kfree_skb(skb);
}
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */


void hostap_init_data(local_info_t *local)
{
	struct ap_data *ap = local->ap;

	if (ap == NULL) {
		printk(KERN_WARNING "hostap_init_data: ap == NULL\n");
		return;
	}
	memset(ap, 0, sizeof(struct ap_data));
	ap->local = local;

	ap->ap_policy = GET_INT_PARM(other_ap_policy, local->card_idx);
	ap->proc = local->proc;
	ap->bridge_packets = GET_INT_PARM(ap_bridge_packets, local->card_idx);
	ap->max_inactivity =
		GET_INT_PARM(ap_max_inactivity, local->card_idx) * HZ;
	ap->auth_algs = PRISM2_AUTH_OPEN | PRISM2_AUTH_SHARED_KEY;
	ap->autom_ap_wds = GET_INT_PARM(autom_ap_wds, local->card_idx);

	spin_lock_init(&ap->sta_table_lock);
	INIT_LIST_HEAD(&ap->sta_list);

#ifndef PRISM2_NO_PROCFS_DEBUG
	if (ap->proc != NULL) {
		create_proc_read_entry("ap_debug", 0, ap->proc,
				       ap_debug_proc_read, ap);
	}
#endif /* PRISM2_NO_PROCFS_DEBUG */

	/* Initialize task queue structure for AP management */
	HOSTAP_QUEUE_INIT(&local->ap->set_tim_queue, handle_set_tim_queue,
			  local);
	INIT_LIST_HEAD(&ap->set_tim_list);
	spin_lock_init(&ap->set_tim_lock);

	HOSTAP_QUEUE_INIT(&local->ap->add_sta_proc_queue,
			  handle_add_proc_queue, ap);

	ap->tx_callback_idx =
		hostap_tx_callback_register(local, hostap_ap_tx_cb, ap);
	if (ap->tx_callback_idx == 0)
		printk("%s: failed to register TX callback for "
		       "AP\n", local->dev->name);
#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
	HOSTAP_QUEUE_INIT(&local->ap->add_wds_queue,
			  handle_add_wds_queue, local);

	ap->tx_callback_auth =
		hostap_tx_callback_register(local, hostap_ap_tx_cb_auth, ap);
	ap->tx_callback_assoc =
		hostap_tx_callback_register(local, hostap_ap_tx_cb_assoc, ap);
	if (ap->tx_callback_auth == 0 || ap->tx_callback_assoc == 0)
		printk("%s: failed to register TX callback for "
		       "AP\n", local->dev->name);

	spin_lock_init(&ap->mac_restrictions.lock);
	INIT_LIST_HEAD(&ap->mac_restrictions.mac_list);
	if (ap->proc != NULL) {
		create_proc_read_entry("ap_control", 0, ap->proc,
				       ap_control_proc_read, ap);
	}

	create_proc_read_entry("ap", 0, ap->proc,
			       prism2_ap_proc_read, ap);
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */

	ap->initialized = 1;
}

void hostap_free_data(struct ap_data *ap)
{
	struct list_head *ptr, *n;

	if (ap == NULL || !ap->initialized) {
		printk(KERN_DEBUG "hostap_free_data: ap has not yet been "
		       "initialized - skip resource freeing\n");
		return;
	}

#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
	if (ap->crypt)
		ap->crypt->deinit(ap->crypt_priv);
	ap->crypt = ap->crypt_priv = NULL;
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */

	ptr = ap->sta_list.next;
	while (ptr != NULL && ptr != &ap->sta_list) {
		struct sta_info *sta = (struct sta_info *) ptr;
		ptr = ptr->next;
		ap_sta_hash_del(ap, sta);
		list_del(&sta->list);
		if ((sta->flags & WLAN_STA_ASSOC) && !sta->ap && sta->local)
			hostap_event_expired_sta(sta->local->dev, sta);
		ap_free_sta(ap, sta);
	}

	for (ptr = ap->set_tim_list.next, n = ptr->next;
	     ptr != &ap->set_tim_list; ptr = n, n = ptr->next) {
		struct set_tim_data *entry;
		entry = list_entry(ptr, struct set_tim_data, list);
		list_del(&entry->list);
		kfree(entry);
	}

#ifndef PRISM2_NO_PROCFS_DEBUG
	if (ap->proc != NULL) {
		remove_proc_entry("ap_debug", ap->proc);
	}
#endif /* PRISM2_NO_PROCFS_DEBUG */

#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
	if (ap->proc != NULL) {
	  remove_proc_entry("ap", ap->proc);
		remove_proc_entry("ap_control", ap->proc);
	}
	ap_control_flush_macs(&ap->mac_restrictions);
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */

	ap->initialized = 0;
}


/* caller should have mutex for AP STA list handling */
static struct sta_info* ap_get_sta(struct ap_data *ap, u8 *sta)
{
	struct sta_info *s;

	s = ap->sta_hash[STA_HASH(sta)];
	while (s != NULL && memcmp(s->addr, sta, 6) != 0)
		s = s->hnext;
	return s;
}


#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT

/* Called from timer handler and from scheduled AP queue handlers */
static void prism2_send_mgmt(struct net_device *dev,
			     int type, int subtype, char *body,
			     int body_len, int txevent, u8 *addr,
			     u16 tx_cb_idx)
{
	local_info_t *local = (local_info_t *) dev->priv;
	struct hfa384x_tx_frame *txdesc;
	u16 fc, tx_control;
	struct sk_buff *skb;

	if (!(dev->flags & IFF_UP)) {
		PDEBUG(DEBUG_AP, "%s: prism2_send_mgmt - device is not UP - "
		       "cannot send frame\n", dev->name);
		return;
	}

	skb = dev_alloc_skb(sizeof(*txdesc) + body_len);
	if (skb == NULL) {
		PDEBUG(DEBUG_AP, "%s: prism2_send_mgmt failed to allocate "
		       "skb\n", dev->name);
		return;
	}

	txdesc = (struct hfa384x_tx_frame *) skb_put(skb, sizeof(*txdesc));
	if (body)
		memcpy(skb_put(skb, body_len), body, body_len);

	memset(txdesc, 0, sizeof(*txdesc));
	/* FIX: set tx_rate if f/w does not know how to do it */
	tx_control = txevent ? local->tx_control : HFA384X_TX_CTRL_802_11;
	if (tx_cb_idx)
		tx_control |= HFA384X_TX_CTRL_TX_OK;
	txdesc->sw_support = cpu_to_le16(tx_cb_idx);
	txdesc->tx_control = cpu_to_le16(tx_control);
	txdesc->data_len = cpu_to_le16(body_len);

	fc = (type << 2) | (subtype << 4);

	memcpy(txdesc->addr1, addr, ETH_ALEN); /* DA / RA */
	if (type == WLAN_FC_TYPE_DATA) {
		fc |= WLAN_FC_FROMDS;
		memcpy(txdesc->addr2, dev->dev_addr, ETH_ALEN); /* BSSID */
		memcpy(txdesc->addr3, dev->dev_addr, ETH_ALEN); /* SA */
	} else if (type == WLAN_FC_TYPE_CTRL) {
		/* control:ACK does not have addr2 or addr3 */
		memset(txdesc->addr2, 0, ETH_ALEN);
		memset(txdesc->addr3, 0, ETH_ALEN);
	} else {
		memcpy(txdesc->addr2, dev->dev_addr, ETH_ALEN); /* SA */
		memcpy(txdesc->addr3, dev->dev_addr, ETH_ALEN); /* BSSID */
	}

	txdesc->frame_control = cpu_to_le16(fc);

	/* FIX: is it OK to call dev_queue_xmit() here? This can be called in
	 * interrupt context, but not in hard interrupt (like prism2_rx() that
	 * required bridge_list. If needed, bridge_list could be used also here
	 * when prism2_send_mgmt is called in interrupt context. */

	skb->protocol = __constant_htons(ETH_P_HOSTAP);
	skb->dev = dev;
	skb->mac.raw = skb->nh.raw = skb->data;
	dev_queue_xmit(skb);
}
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */


static int prism2_sta_proc_read(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	char *p = page;
	struct sta_info *sta = (struct sta_info *) data;
	int i;

	/* FIX: possible race condition.. the STA data could have just expired,
	 * but proc entry was still here so that the read could have started;
	 * some locking should be done here.. */

	if (off != 0) {
		*eof = 1;
		return 0;
	}

	p += sprintf(p, "%s=" MACSTR "\nusers=%d\naid=%d\n"
		     "flags=0x%04x%s%s%s%s%s%s\n"
		     "capability=0x%02x\nlisten_interval=%d\nsupported_rates=",
		     sta->ap ? "AP" : "STA",
		     MAC2STR(sta->addr), atomic_read(&sta->users), sta->aid,
		     sta->flags,
		     sta->flags & WLAN_STA_AUTH ? " AUTH" : "",
		     sta->flags & WLAN_STA_ASSOC ? " ASSOC" : "",
		     sta->flags & WLAN_STA_PS ? " PS" : "",
		     sta->flags & WLAN_STA_TIM ? " TIM" : "",
		     sta->flags & WLAN_STA_PERM ? " PERM" : "",
		     sta->flags & WLAN_STA_AUTHORIZED ? " AUTHORIZED" : "",
		     sta->capability, sta->listen_interval);
	/* supported_rates: 500 kbit/s units with msb ignored */
	for (i = 0; i < sizeof(sta->supported_rates); i++)
		if (sta->supported_rates[i] != 0)
			p += sprintf(p, "%d%sMbps ",
				     (sta->supported_rates[i] & 0x7f) / 2,
				     sta->supported_rates[i] & 1 ? ".5" : "");
	p += sprintf(p, "\njiffies=%lu\nlast_auth=%lu\nlast_assoc=%lu\n"
		     "last_rx=%lu\nlast_tx=%lu\nrx_packets=%lu\n"
		     "tx_packets=%lu\n"
		     "rx_bytes=%lu\ntx_bytes=%lu\nbuffer_count=%d\n"
		     "last_rx: silence=%d signal=%d rate=%d flow=%d\n"
		     "tx_rate=%d\ntx[1M]=%d\ntx[2M]=%d\ntx[5.5M]=%d\n"
		     "tx[11M]=%d\n"
		     "rx[1M]=%d\nrx[2M]=%d\nrx[5.5M]=%d\nrx[11M]=%d\n"
		     "txexc=%d\n",
		     jiffies, sta->last_auth, sta->last_assoc, sta->last_rx,
		     sta->last_tx,
		     sta->rx_packets, sta->tx_packets, sta->rx_bytes,
		     sta->tx_bytes, skb_queue_len(&sta->tx_buf),
		     sta->last_rx_silence,
		     sta->last_rx_signal, sta->last_rx_rate,
		     sta->last_rx_flow,
		     sta->tx_rate, sta->tx_count[0], sta->tx_count[1],
		     sta->tx_count[2], sta->tx_count[3],  sta->rx_count[0],
		     sta->rx_count[1], sta->rx_count[2], sta->rx_count[3],
		     sta->txexc);
	if (sta->crypt && sta->crypt->ops)
		p += sprintf(p, "crypt=%s\n", sta->crypt->ops->name);
#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
	if (sta->ap) {
		if (sta->u.ap.channel >= 0)
			p += sprintf(p, "channel=%d\n", sta->u.ap.channel);
		p += sprintf(p, "ssid=");
		for (i = 0; i < sta->u.ap.ssid_len; i++)
			p += sprintf(p, ((sta->u.ap.ssid[i] >= 32 &&
					  sta->u.ap.ssid[i] < 127) ?
					 "%c" : "<%02x>"),
				     sta->u.ap.ssid[i]);
		p += sprintf(p, "\n");
	}
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */

	return (p - page);
}


static void handle_add_proc_queue(void *data)
{
	struct ap_data *ap = (struct ap_data *) data;
	struct sta_info *sta;
	char name[20];
	struct add_sta_proc_data *entry, *prev;

	entry = ap->add_sta_proc_entries;
	ap->add_sta_proc_entries = NULL;

	while (entry) {
		spin_lock_bh(&ap->sta_table_lock);
		sta = ap_get_sta(ap, entry->addr);
		if (sta)
			atomic_inc(&sta->users);
		spin_unlock_bh(&ap->sta_table_lock);

		if (sta) {
			sprintf(name, MACSTR, MAC2STR(sta->addr));
			sta->proc = create_proc_read_entry(
				name, 0, ap->proc,
				prism2_sta_proc_read, sta);

			atomic_dec(&sta->users);
		}

		prev = entry;
		entry = entry->next;
		kfree(prev);
	}

#ifndef NEW_MODULE_CODE
	MOD_DEC_USE_COUNT;
#endif
}


static struct sta_info * ap_add_sta(struct ap_data *ap, u8 *addr)
{
	struct sta_info *sta;

	sta = (struct sta_info *)
		kmalloc(sizeof(struct sta_info), GFP_ATOMIC);
	if (sta == NULL) {
		PDEBUG(DEBUG_AP, "AP: kmalloc failed\n");
		return NULL;
	}

	/* initialize STA info data */
	memset(sta, 0, sizeof(struct sta_info));
	sta->local = ap->local;
	skb_queue_head_init(&sta->tx_buf);
	memcpy(sta->addr, addr, ETH_ALEN);

	atomic_inc(&sta->users);
	spin_lock_bh(&ap->sta_table_lock);
	list_add(&sta->list, &ap->sta_list);
	ap->num_sta++;
	ap_sta_hash_add(ap, sta);
	spin_unlock_bh(&ap->sta_table_lock);

	if (ap->proc) {
		struct add_sta_proc_data *entry;
		/* schedule a non-interrupt context process to add a procfs
		 * entry for the STA since procfs code use GFP_KERNEL */
		entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
		if (entry) {
			memcpy(entry->addr, sta->addr, ETH_ALEN);
			entry->next = ap->add_sta_proc_entries;
			ap->add_sta_proc_entries = entry;
			PRISM2_SCHEDULE_TASK(&ap->add_sta_proc_queue);
		} else
			printk(KERN_DEBUG "Failed to add STA proc data\n");
	}

#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
	init_timer(&sta->timer);
	sta->timer.expires = jiffies + ap->max_inactivity;
	sta->timer.data = (unsigned long) sta;
	sta->timer.function = ap_handle_timer;
	if (!ap->local->hostapd)
		add_timer(&sta->timer);
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */

	return sta;
}


static int ap_tx_rate_ok(int rateidx, struct sta_info *sta,
			 local_info_t *local)
{
	if (rateidx > sta->tx_max_rate ||
	    !(sta->tx_supp_rates & (1 << rateidx)))
		return 0;

	if (local->tx_rate_control != 0 &&
	    !(local->tx_rate_control & (1 << rateidx)))
		return 0;

	return 1;
}


static void prism2_check_tx_rates(struct sta_info *sta)
{
	int i;

	sta->tx_supp_rates = 0;
	for (i = 0; i < sizeof(sta->supported_rates); i++) {
		if ((sta->supported_rates[i] & 0x7f) == 2)
			sta->tx_supp_rates |= WLAN_RATE_1M;
		if ((sta->supported_rates[i] & 0x7f) == 4)
			sta->tx_supp_rates |= WLAN_RATE_2M;
		if ((sta->supported_rates[i] & 0x7f) == 11)
			sta->tx_supp_rates |= WLAN_RATE_5M5;
		if ((sta->supported_rates[i] & 0x7f) == 22)
			sta->tx_supp_rates |= WLAN_RATE_11M;
	}
	sta->tx_max_rate = sta->tx_rate = sta->tx_rate_idx = 0;
	if (sta->tx_supp_rates & WLAN_RATE_1M) {
		sta->tx_max_rate = 0;
		if (ap_tx_rate_ok(0, sta, sta->local)) {
			sta->tx_rate = 10;
			sta->tx_rate_idx = 0;
		}
	}
	if (sta->tx_supp_rates & WLAN_RATE_2M) {
		sta->tx_max_rate = 1;
		if (ap_tx_rate_ok(1, sta, sta->local)) {
			sta->tx_rate = 20;
			sta->tx_rate_idx = 1;
		}
	}
	if (sta->tx_supp_rates & WLAN_RATE_5M5) {
		sta->tx_max_rate = 2;
		if (ap_tx_rate_ok(2, sta, sta->local)) {
			sta->tx_rate = 55;
			sta->tx_rate_idx = 2;
		}
	}
	if (sta->tx_supp_rates & WLAN_RATE_11M) {
		sta->tx_max_rate = 3;
		if (ap_tx_rate_ok(3, sta, sta->local)) {
			sta->tx_rate = 110;
			sta->tx_rate_idx = 3;
		}
	}
}


#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT

static void ap_crypt_init(struct ap_data *ap)
{
	ap->crypt = hostap_get_crypto_ops("WEP");

	if (ap->crypt) {
		if (ap->crypt->init) {
			ap->crypt_priv = ap->crypt->init();
			if (ap->crypt_priv == NULL)
				ap->crypt = NULL;
			else {
				u8 key[WEP_KEY_LEN];
				get_random_bytes(key, WEP_KEY_LEN);
				ap->crypt->set_key(0, key, WEP_KEY_LEN,
						   ap->crypt_priv);
			}
		}
	}

	if (ap->crypt == NULL) {
		printk(KERN_WARNING "AP could not initialize WEP: load module "
		       "hostap_crypt_wep.o\n");
	}
}


/* Generate challenge data for shared key authentication. IEEE 802.11 specifies
 * that WEP algorithm is used for generating challange. This should be unique,
 * but otherwise there is not really need for randomness etc. Initialize WEP
 * with pseudo random key and then use increasing IV to get unique challenge
 * streams.
 *
 * Called only as a scheduled task for pending AP frames.
 */
static char * ap_auth_make_challenge(struct ap_data *ap)
{
	char *tmpbuf;
	int olen;

	if (ap->crypt == NULL) {
		ap_crypt_init(ap);
		if (ap->crypt == NULL)
			return NULL;
	}

	tmpbuf = (char *) kmalloc(WLAN_AUTH_CHALLENGE_LEN +
				  ap->crypt->extra_prefix_len +
				  ap->crypt->extra_postfix_len,
				  GFP_ATOMIC);
	if (tmpbuf == NULL) {
		PDEBUG(DEBUG_AP, "AP: kmalloc failed for challenge\n");
		return NULL;
	}
	memset(tmpbuf, 0, WLAN_AUTH_CHALLENGE_LEN +
	       ap->crypt->extra_prefix_len + ap->crypt->extra_postfix_len);
	olen = ap->crypt->encrypt(tmpbuf, WLAN_AUTH_CHALLENGE_LEN,
				  ap->crypt_priv);
	if (olen < 0) {
		kfree(tmpbuf);
		return NULL;
	}
	memmove(tmpbuf, tmpbuf + 4, WLAN_AUTH_CHALLENGE_LEN);
	return tmpbuf;
}


/* Called only as a scheduled task for pending AP frames. */
static void handle_authen(local_info_t *local, struct hfa384x_rx_frame *rxdesc)
{
	struct net_device *dev = local->dev;
	struct ap_data *ap = local->ap;
	char body[8 + WLAN_AUTH_CHALLENGE_LEN], *challenge = NULL;
	int len, olen;
	u16 auth_alg, auth_transaction, status_code, *pos;
	u16 resp = WLAN_STATUS_SUCCESS, fc;
	struct sta_info *sta = NULL;
	struct prism2_crypt_data *crypt;
	char *txt = "";

	len = __le16_to_cpu(rxdesc->data_len);

	fc = le16_to_cpu(rxdesc->frame_control);

	if (len < 6) {
		PDEBUG(DEBUG_AP, "%s: handle_authen - too short payload "
		       "(len=%d) from " MACSTR "\n", dev->name, len,
		       MAC2STR(rxdesc->addr2));
		return;
	}

	spin_lock_bh(&local->ap->sta_table_lock);
	sta = ap_get_sta(local->ap, rxdesc->addr2);
	if (sta)
		atomic_inc(&sta->users);
	spin_unlock_bh(&local->ap->sta_table_lock);

	if (sta && sta->crypt)
		crypt = sta->crypt;
	else
		crypt = local->crypt;

	if (crypt && local->host_decrypt && (fc & WLAN_FC_ISWEP)) {
		atomic_inc(&crypt->refcnt);
		olen = crypt->ops->decrypt((u8 *) (rxdesc + 1), len,
					   crypt->priv);
		atomic_dec(&crypt->refcnt);
		if (olen < 0) {
			if (sta)
				atomic_dec(&sta->users);
			PDEBUG(DEBUG_AP, "%s: handle_authen: auth frame from "
			       "STA " MACSTR " could not be decrypted\n",
			       dev->name, MAC2STR(rxdesc->addr2));
			return;
		}
		if (olen < 6) {
			PDEBUG(DEBUG_AP, "%s: handle_authen - too short "
			       "payload (len=%d, decrypted len=%d) from "
			       MACSTR "\n",
			       dev->name, len, olen, MAC2STR(rxdesc->addr2));
			return;
		}
		len = olen;
	}

	pos = (u16 *) (rxdesc + 1);
	auth_alg = __le16_to_cpu(*pos);
	pos++;
	auth_transaction = __le16_to_cpu(*pos);
	pos++;
	status_code = __le16_to_cpu(*pos);
	pos++;

	if (ap_control_mac_deny(&ap->mac_restrictions, rxdesc->addr2)) {
		txt = "authentication denied";
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	if (((ap->auth_algs & PRISM2_AUTH_OPEN) &&
	     auth_alg == WLAN_AUTH_OPEN) ||
	    ((ap->auth_algs & PRISM2_AUTH_SHARED_KEY) &&
	     crypt && auth_alg == WLAN_AUTH_SHARED_KEY)) {
	} else {
		txt = "unsupported algorithm";
		resp = WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG;
		goto fail;
	}

	if (len >= 8) {
		u8 *u = (u8 *) pos;
		if (*u == WLAN_EID_CHALLENGE) {
			if (*(u + 1) != WLAN_AUTH_CHALLENGE_LEN) {
				txt = "invalid challenge len";
				resp = WLAN_STATUS_CHALLENGE_FAIL;
				goto fail;
			}
			if (len - 8 < WLAN_AUTH_CHALLENGE_LEN) {
				txt = "challenge underflow";
				resp = WLAN_STATUS_CHALLENGE_FAIL;
				goto fail;
			}
			challenge = (char *) (u + 2);
		}
	}

	if (sta && sta->ap) {
		if (jiffies > sta->u.ap.last_beacon +
		    (10 * sta->listen_interval * HZ) / 1024) {
			PDEBUG(DEBUG_AP, "%s: no beacons received for a while,"
			       " assuming AP " MACSTR " is now STA\n",
			       dev->name, MAC2STR(sta->addr));
			sta->ap = 0;
			sta->flags = 0;
			sta->u.sta.challenge = NULL;
		} else {
			txt = "AP trying to authenticate?";
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
	}

	if ((auth_alg == WLAN_AUTH_OPEN && auth_transaction == 1) ||
	    (auth_alg == WLAN_AUTH_SHARED_KEY &&
	     (auth_transaction == 1 ||
	      (auth_transaction == 3 && sta != NULL &&
	       sta->u.sta.challenge != NULL)))) {
	} else {
		txt = "unknown authentication transaction number";
		resp = WLAN_STATUS_UNKNOWN_AUTH_TRANSACTION;
		goto fail;
	}

	if (sta == NULL) {
		txt = "new STA";

		if (local->ap->num_sta >= MAX_STA_COUNT) {
			/* FIX: might try to remove some old STAs first? */
			txt = "no more room for new STAs";
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}

		sta = ap_add_sta(local->ap, rxdesc->addr2);
		if (sta == NULL) {
			txt = "ap_add_sta failed";
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
	}

	prism2_ap_update_sq(sta, rxdesc);

	switch (auth_alg) {
	case WLAN_AUTH_OPEN:
		txt = "authOK";
		/* STA will be authenticated, if it ACKs authentication frame
		 */
		break;

	case WLAN_AUTH_SHARED_KEY:
		if (auth_transaction == 1) {
			if (sta->u.sta.challenge == NULL) {
				sta->u.sta.challenge =
					ap_auth_make_challenge(local->ap);
				if (sta->u.sta.challenge == NULL) {
					resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
					goto fail;
				}
			}
		} else {
			if (sta->u.sta.challenge == NULL ||
			    challenge == NULL ||
			    memcmp(sta->u.sta.challenge, challenge,
				   WLAN_AUTH_CHALLENGE_LEN) != 0 ||
			    !(fc & WLAN_FC_ISWEP)) {
				txt = "challenge response incorrect";
				resp = WLAN_STATUS_CHALLENGE_FAIL;
				goto fail;
			}

			txt = "challenge OK - authOK";
			/* STA will be authenticated, if it ACKs authentication
			 * frame */
			kfree(sta->u.sta.challenge);
			sta->u.sta.challenge = NULL;
		}
		break;
	}

 fail:
	pos = (u16 *) body;
	*pos = cpu_to_le16(auth_alg);
	pos++;
	*pos = cpu_to_le16(auth_transaction + 1);
	pos++;
	*pos = cpu_to_le16(resp); /* status_code */
	pos++;
	olen = 6;

	if (resp == WLAN_STATUS_SUCCESS && sta != NULL &&
	    sta->u.sta.challenge != NULL &&
	    auth_alg == WLAN_AUTH_SHARED_KEY && auth_transaction == 1) {
		u8 *tmp = (u8 *) pos;
		*tmp++ = WLAN_EID_CHALLENGE;
		*tmp++ = WLAN_AUTH_CHALLENGE_LEN;
		pos++;
		memcpy(pos, sta->u.sta.challenge, WLAN_AUTH_CHALLENGE_LEN);
		olen += 2 + WLAN_AUTH_CHALLENGE_LEN;
	}

	prism2_send_mgmt(dev, WLAN_FC_TYPE_MGMT, WLAN_FC_STYPE_AUTH,
			 body, olen, 1, rxdesc->addr2, ap->tx_callback_auth);

	if (sta) {
		sta->last_rx = jiffies;
		atomic_dec(&sta->users);
	}

#if 0
	PDEBUG(DEBUG_AP, "%s: " MACSTR " auth (alg=%d trans#=%d stat=%d len=%d"
	       " fc=%04x) ==> %d (%s)\n", dev->name, MAC2STR(rxdesc->addr2),
	       auth_alg, auth_transaction, status_code, len, fc, resp, txt);
#endif
}


/* Called only as a scheduled task for pending AP frames. */
static void handle_assoc(local_info_t *local, struct hfa384x_rx_frame *rxdesc,
			 int reassoc)
{
	struct net_device *dev = local->dev;
	char body[12], *p, *lpos;
	int len, left;
	u16 *pos;
	u16 resp = WLAN_STATUS_SUCCESS;
	struct sta_info *sta = NULL;
	int send_deauth = 0;
	char *txt = "";
	u8 prev_ap[ETH_ALEN];

	left = len = __le16_to_cpu(rxdesc->data_len);

	if (len < (reassoc ? 10 : 4)) {
		PDEBUG(DEBUG_AP, "%s: handle_assoc - too short payload "
		       "(len=%d, reassoc=%d) from " MACSTR "\n",
		       dev->name, len, reassoc, MAC2STR(rxdesc->addr2));
		return;
	}

	spin_lock_bh(&local->ap->sta_table_lock);
	sta = ap_get_sta(local->ap, rxdesc->addr2);
	if (sta == NULL || (sta->flags & WLAN_STA_AUTH) == 0) {
		spin_unlock_bh(&local->ap->sta_table_lock);
		txt = "trying to associate before authentication";
		send_deauth = 1;
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}
	atomic_inc(&sta->users);
	spin_unlock_bh(&local->ap->sta_table_lock);

	prism2_ap_update_sq(sta, rxdesc);

	pos = (u16 *) (rxdesc + 1);
	sta->capability = __le16_to_cpu(*pos);
	pos++; left -= 2;
	sta->listen_interval = __le16_to_cpu(*pos);
	pos++; left -= 2;

	if (reassoc) {
		memcpy(prev_ap, pos, ETH_ALEN);
		pos++; pos++; pos++; left -= 6;
	} else
		memset(prev_ap, 0, ETH_ALEN);

	if (left >= 2) {
		unsigned int ileft;
		unsigned char *u = (unsigned char *) pos;

		if (*u == WLAN_EID_SSID) {
			u++; left--;
			ileft = *u;
			u++; left--;

			if (ileft > left || ileft > MAX_SSID_LEN) {
				txt = "SSID overflow";
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto fail;
			}

			if (ileft != strlen(local->essid) ||
			    memcmp(local->essid, u, ileft) != 0) {
				txt = "not our SSID";
				resp = WLAN_STATUS_ASSOC_DENIED_UNSPEC;
				goto fail;
			}

			u += ileft;
			left -= ileft;
		}

		if (left >= 2 && *u == WLAN_EID_SUPP_RATES) {
			u++; left--;
			ileft = *u;
			u++; left--;
			
			if (ileft > left || ileft == 0 ||
			    ileft > WLAN_SUPP_RATES_MAX) {
				txt = "SUPP_RATES len error";
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto fail;
			}

			memset(sta->supported_rates, 0,
			       sizeof(sta->supported_rates));
			memcpy(sta->supported_rates, u, ileft);
			prism2_check_tx_rates(sta);

			u += ileft;
			left -= ileft;
		}

		if (left > 0) {
			PDEBUG(DEBUG_AP, "%s: assoc from " MACSTR " with extra"
			       " data (%d bytes) [",
			       dev->name, MAC2STR(rxdesc->addr2), left);
			while (left > 0) {
				PDEBUG2(DEBUG_AP, "<%02x>", *u);
				u++; left--;
			}
			PDEBUG2(DEBUG_AP, "]\n");
		}
	} else {
		txt = "frame underflow";
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	/* get a unique AID */
	if (sta->aid > 0)
		txt = "OK, old AID";
	else {
		spin_lock_bh(&local->ap->sta_table_lock);
		for (sta->aid = 1; sta->aid <= MAX_AID_TABLE_SIZE; sta->aid++)
			if (local->ap->sta_aid[sta->aid - 1] == NULL)
				break;
		if (sta->aid > MAX_AID_TABLE_SIZE) {
			sta->aid = 0;
			spin_unlock_bh(&local->ap->sta_table_lock);
			resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
			txt = "no room for more AIDs";
		} else {
			local->ap->sta_aid[sta->aid - 1] = sta;
			spin_unlock_bh(&local->ap->sta_table_lock);
			txt = "OK, new AID";
		}
	}

 fail:
	pos = (u16 *) body;

	if (send_deauth) {
		*pos = __constant_cpu_to_le16(
			WLAN_REASON_STA_REQ_ASSOC_WITHOUT_AUTH);
		pos++;
	} else {
		/* FIX: CF-Pollable and CF-PollReq should be set to match the
		 * values in beacons/probe responses */
		/* FIX: how about privacy and WEP? */
		/* capability */
		*pos = __constant_cpu_to_le16(WLAN_CAPABILITY_ESS);
		pos++;

		/* status_code */
		*pos = __cpu_to_le16(resp);
		pos++;

		*pos = __cpu_to_le16((sta && sta->aid > 0 ? sta->aid : 0) |
				     BIT(14) | BIT(15)); /* AID */
		pos++;

		/* Supported rates (Information element) */
		p = (char *) pos;
		*p++ = WLAN_EID_SUPP_RATES;
		lpos = p;
		*p++ = 0; /* len */
		if (local->tx_rate_control & WLAN_RATE_1M) {
			*p++ = local->basic_rates & WLAN_RATE_1M ? 0x82 : 0x02;
			(*lpos)++;
		}
		if (local->tx_rate_control & WLAN_RATE_2M) {
			*p++ = local->basic_rates & WLAN_RATE_2M ? 0x84 : 0x04;
			(*lpos)++;
		}
		if (local->tx_rate_control & WLAN_RATE_5M5) {
			*p++ = local->basic_rates & WLAN_RATE_5M5 ?
				0x8b : 0x0b;
			(*lpos)++;
		}
		if (local->tx_rate_control & WLAN_RATE_11M) {
			*p++ = local->basic_rates & WLAN_RATE_11M ?
				0x96 : 0x16;
			(*lpos)++;
		}
		pos = (u16 *) p;
	}

	prism2_send_mgmt(dev, WLAN_FC_TYPE_MGMT,
			 (send_deauth ? WLAN_FC_STYPE_DEAUTH :
			  (reassoc ? WLAN_FC_STYPE_REASSOC_RESP :
			   WLAN_FC_STYPE_ASSOC_RESP)),
			 body, (u8 *) pos - (u8 *) body, 1,
			 rxdesc->addr2,
			 send_deauth ? 0 : local->ap->tx_callback_assoc);

	if (sta) {
		if (resp == WLAN_STATUS_SUCCESS) {
			sta->last_rx = jiffies;
			/* STA will be marked associated from TX callback, if
			 * AssocResp is ACKed */
		}
		atomic_dec(&sta->users);
	}

#if 0
	PDEBUG(DEBUG_AP, "%s: " MACSTR " %sassoc (len=%d prev_ap=" MACSTR
	       ") => %d(%d) (%s)\n",
	       dev->name, MAC2STR(rxdesc->addr2), reassoc ? "re" : "", len,
	       MAC2STR(prev_ap), resp, send_deauth, txt);
#endif
}


/* Called only as a scheduled task for pending AP frames. */
static void handle_deauth(local_info_t *local, struct hfa384x_rx_frame *rxdesc)
{
	struct net_device *dev = local->dev;
	char *body = (char *) (rxdesc + 1);
	int len;
	u16 reason_code, *pos;
	struct sta_info *sta = NULL;

	len = __le16_to_cpu(rxdesc->data_len);

	if (len < 2) {
		printk("handle_deauth - too short payload (len=%d)\n", len);
		return;
	}

	pos = (u16 *) body;
	reason_code = __le16_to_cpu(*pos);

	PDEBUG(DEBUG_AP, "%s: deauthentication: " MACSTR " len=%d, "
	       "reason_code=%d\n", dev->name, MAC2STR(rxdesc->addr2), len,
	       reason_code);

	spin_lock_bh(&local->ap->sta_table_lock);
	sta = ap_get_sta(local->ap, rxdesc->addr2);
	if (sta != NULL) {
		if ((sta->flags & WLAN_STA_ASSOC) && !sta->ap)
			hostap_event_expired_sta(local->dev, sta);
		sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC);
		prism2_ap_update_sq(sta, rxdesc);
	}
	spin_unlock_bh(&local->ap->sta_table_lock);
	if (sta == NULL) {
		printk("%s: deauthentication from " MACSTR ", "
	       "reason_code=%d, but STA not authenticated\n", dev->name,
		       MAC2STR(rxdesc->addr2), reason_code);
	}
}


/* Called only as a scheduled task for pending AP frames. */
static void handle_disassoc(local_info_t *local,
			    struct hfa384x_rx_frame *rxdesc)
{
	struct net_device *dev = local->dev;
	char *body = (char *) (rxdesc + 1);
	int len;
	u16 reason_code, *pos;
	struct sta_info *sta = NULL;

	len = __le16_to_cpu(rxdesc->data_len);

	if (len < 2) {
		printk("handle_disassoc - too short payload (len=%d)\n", len);
		return;
	}

	pos = (u16 *) body;
	reason_code = __le16_to_cpu(*pos);

	PDEBUG(DEBUG_AP, "%s: disassociation: " MACSTR " len=%d, "
	       "reason_code=%d\n", dev->name, MAC2STR(rxdesc->addr2), len,
	       reason_code);

	spin_lock_bh(&local->ap->sta_table_lock);
	sta = ap_get_sta(local->ap, rxdesc->addr2);
	if (sta != NULL) {
		if ((sta->flags & WLAN_STA_ASSOC) && !sta->ap)
			hostap_event_expired_sta(local->dev, sta);
		sta->flags &= ~WLAN_STA_ASSOC;
		prism2_ap_update_sq(sta, rxdesc);
	}
	spin_unlock_bh(&local->ap->sta_table_lock);
	if (sta == NULL) {
		printk("%s: disassociation from " MACSTR ", "
		       "reason_code=%d, but STA not authenticated\n",
		       dev->name, MAC2STR(rxdesc->addr2), reason_code);
	}
}


/* Called only as a scheduled task for pending AP frames. */
static void ap_handle_data_nullfunc(local_info_t *local,
				    struct hfa384x_rx_frame *rxdesc)
{
	struct net_device *dev = local->dev;

	/* some STA f/w's seem to require control::ACK frame for
	 * data::nullfunc, but at least Prism2 station f/w version 0.8.0 does
	 * not send this..
	 * send control::ACK for the data::nullfunc */

	printk(KERN_DEBUG "Sending control::ACK for data::nullfunc\n");
	prism2_send_mgmt(dev, WLAN_FC_TYPE_CTRL, WLAN_FC_STYPE_ACK,
			 NULL, 0, 0, rxdesc->addr2, 0);
}


/* Called only as a scheduled task for pending AP frames. */
static void ap_handle_dropped_data(local_info_t *local,
				   struct hfa384x_rx_frame *rxdesc)
{
	struct net_device *dev = local->dev;
	struct sta_info *sta;
	u16 reason;

	spin_lock_bh(&local->ap->sta_table_lock);
	sta = ap_get_sta(local->ap, rxdesc->addr2);
	if (sta)
		atomic_inc(&sta->users);
	spin_unlock_bh(&local->ap->sta_table_lock);

	if (sta != NULL && (sta->flags & WLAN_STA_ASSOC)) {
		PDEBUG(DEBUG_AP, "ap_handle_dropped_data: STA is now okay?\n");
		atomic_dec(&sta->users);
		return;
	}

	reason = __constant_cpu_to_le16(
		WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
	prism2_send_mgmt(dev, WLAN_FC_TYPE_MGMT,
			 ((sta == NULL || !(sta->flags & WLAN_STA_ASSOC)) ?
			  WLAN_FC_STYPE_DEAUTH : WLAN_FC_STYPE_DISASSOC),
			 (char *) &reason, sizeof(reason), 1,
			 rxdesc->addr2, 0);

	if (sta)
		atomic_dec(&sta->users);
}

#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */


/* Called only as a scheduled task for pending AP frames. */
static void pspoll_send_buffered(local_info_t *local, struct sta_info *sta,
				 struct sk_buff *skb)
{
	if (!(sta->flags & WLAN_STA_PS)) {
		/* Station has moved to non-PS mode, so send all buffered
		 * frames using normal device queue. */
		dev_queue_xmit(skb);
		return;
	}

	/* add a flag for hostap_handle_sta_tx() to know that this skb should
	 * be passed through even though STA is using PS */
	memcpy(skb->cb, AP_SKB_CB_MAGIC, AP_SKB_CB_MAGIC_LEN);
	skb->cb[AP_SKB_CB_MAGIC_LEN] = AP_SKB_CB_BUFFERED_FRAME;
	if (!skb_queue_empty(&sta->tx_buf)) {
		/* indicate to STA that more frames follow */
		skb->cb[AP_SKB_CB_MAGIC_LEN] |= AP_SKB_CB_ADD_MOREDATA;
	}
	if (skb->dev->hard_start_xmit(skb, skb->dev)) {
		PDEBUG(DEBUG_AP, "%s: TX failed for buffered frame (PS Poll)"
		       "\n", skb->dev->name);
		dev_kfree_skb(skb);
	}
}


/* Called only as a scheduled task for pending AP frames. */
static void handle_pspoll(local_info_t *local,
			  struct hfa384x_rx_frame *rxdesc)
{
	struct net_device *dev = local->dev;
	struct sta_info *sta;
	u16 aid;
	struct sk_buff *skb;

	PDEBUG(DEBUG_PS2, "handle_pspoll: BSSID=" MACSTR ", TA=" MACSTR
	       " PWRMGT=%d\n",
	       MAC2STR(rxdesc->addr1), MAC2STR(rxdesc->addr2),
	       !!(le16_to_cpu(rxdesc->frame_control) & WLAN_FC_PWRMGT));

	if (memcmp(rxdesc->addr1, dev->dev_addr, 6)) {
		PDEBUG(DEBUG_AP, "handle_pspoll - addr1(BSSID)=" MACSTR
		       " not own MAC\n", MAC2STR(rxdesc->addr1));
		return;
	}

	aid = __le16_to_cpu(rxdesc->duration_id);
	if ((aid & (BIT(15) | BIT(14))) != (BIT(15) | BIT(14))) {
		PDEBUG(DEBUG_PS, "   PSPOLL and AID[15:14] not set\n");
		return;
	}
	aid &= ~BIT(15) & ~BIT(14);
	if (aid == 0 || aid > MAX_AID_TABLE_SIZE) {
		PDEBUG(DEBUG_PS, "   invalid aid=%d\n", aid);
		return;
	}
	PDEBUG(DEBUG_PS2, "   aid=%d\n", aid);

	spin_lock_bh(&local->ap->sta_table_lock);
	sta = ap_get_sta(local->ap, rxdesc->addr2);
	if (sta)
		atomic_inc(&sta->users);
	spin_unlock_bh(&local->ap->sta_table_lock);

	if (sta == NULL) {
		PDEBUG(DEBUG_PS, "   STA not found\n");
		return;
	}
	prism2_ap_update_sq(sta, rxdesc);
	if (sta->aid != aid) {
		PDEBUG(DEBUG_PS, "   received aid=%i does not match with "
		       "assoc.aid=%d\n", aid, sta->aid);
		return;
	}

	/* FIX: todo:
	 * - add timeout for buffering (clear aid in TIM vector if buffer timed
	 *   out (expiry time must be longer than ListenInterval for
	 *   the corresponding STA; "8802-11: 11.2.1.9 AP aging function"
	 * - what to do, if buffered, pspolled, and sent frame is not ACKed by
	 *   sta; store buffer for later use and leave TIM aid bit set? use
	 *   TX event to check whether frame was ACKed?
	 */

	while ((skb = skb_dequeue(&sta->tx_buf)) != NULL) {
		/* send buffered frame .. */
		PDEBUG(DEBUG_PS2, "Sending buffered frame to STA after PS POLL"
		       " (buffer_count=%d)\n", skb_queue_len(&sta->tx_buf));

		pspoll_send_buffered(local, sta, skb);

		if (sta->flags & WLAN_STA_PS) {
			/* send only one buffered packet per PS Poll */
			/* FIX: should ignore further PS Polls until the
			 * buffered packet that was just sent is acknowledged
			 * (Tx or TxExc event) */
			break;
		}
	}

	if (skb_queue_empty(&sta->tx_buf)) {
		/* try to clear aid from TIM */
		if (!(sta->flags & WLAN_STA_TIM))
			PDEBUG(DEBUG_PS2,  "Re-unsetting TIM for aid %d\n",
			       aid);
		hostap_set_tim(local, aid, 0);
		sta->flags &= ~WLAN_STA_TIM;
	}

	atomic_dec(&sta->users);
}


static void prism2_ap_update_sq(struct sta_info *sta,
				struct hfa384x_rx_frame *rxdesc)
{
	sta->last_rx_silence = rxdesc->silence;
	sta->last_rx_signal = rxdesc->signal;
	sta->last_rx_rate = rxdesc->rate;
	sta->last_rx_flow = rxdesc->rxflow;
	sta->last_rx_updated = 7;
	if (rxdesc->rate == 10)
		sta->rx_count[0]++;
	else if (rxdesc->rate == 20)
		sta->rx_count[1]++;
	else if (rxdesc->rate == 55)
		sta->rx_count[2]++;
	else if (rxdesc->rate == 110)
		sta->rx_count[3]++;
}


#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT

static void handle_add_wds_queue(void *data)
{
	local_info_t *local = data;
	struct add_wds_data *entry, *prev;

	spin_lock_bh(&local->lock);
	entry = local->ap->add_wds_entries;
	local->ap->add_wds_entries = NULL;
	spin_unlock_bh(&local->lock);

	while (entry) {
		PDEBUG(DEBUG_AP, "%s: adding automatic WDS connection "
		       "to AP " MACSTR "\n",
		       local->dev->name, MAC2STR(entry->addr));
		prism2_wds_add(local, entry->addr, 0);

		prev = entry;
		entry = entry->next;
		kfree(prev);
	}

#ifndef NEW_MODULE_CODE
	MOD_DEC_USE_COUNT;
#endif
}


/* Called only as a scheduled task for pending AP frames. */
static void handle_beacon(local_info_t *local, struct hfa384x_rx_frame *rxdesc)
{
	char *body = (char *) (rxdesc + 1);
	int len, left;
	u16 *pos, beacon_int, capability;
	char *ssid = NULL;
	unsigned char *supp_rates = NULL;
	int ssid_len = 0, supp_rates_len = 0;
	struct sta_info *sta = NULL;
	int new_sta = 0, channel = -1;

	len = __le16_to_cpu(rxdesc->data_len);

	if (len < 8 + 2 + 2) {
		printk(KERN_DEBUG "handle_beacon - too short payload "
		       "(len=%d)\n", len);
		return;
	}

	pos = (u16 *) body;
	left = len;

	/* Timestamp (8 octets) */
	pos += 4; left -= 8;
	/* Beacon interval (2 octets) */
	beacon_int = __le16_to_cpu(*pos);
	pos++; left -= 2;
	/* Capability information (2 octets) */
	capability = __le16_to_cpu(*pos);
	pos++; left -= 2;

	if (local->ap->ap_policy != AP_OTHER_AP_EVEN_IBSS &&
	    capability & WLAN_CAPABILITY_IBSS)
		return;

	if (left >= 2) {
		unsigned int ileft;
		unsigned char *u = (unsigned char *) pos;

		if (*u == WLAN_EID_SSID) {
			u++; left--;
			ileft = *u;
			u++; left--;

			if (ileft > left || ileft > MAX_SSID_LEN) {
				PDEBUG(DEBUG_AP, "SSID: overflow\n");
				return;
			}

			if (local->ap->ap_policy == AP_OTHER_AP_SAME_SSID &&
			    (ileft != strlen(local->essid) ||
			     memcmp(local->essid, u, ileft) != 0)) {
				/* not our SSID */
				return;
			}

			ssid = u;
			ssid_len = ileft;

			u += ileft;
			left -= ileft;
		}

		if (*u == WLAN_EID_SUPP_RATES) {
			u++; left--;
			ileft = *u;
			u++; left--;
			
			if (ileft > left || ileft == 0 || ileft > 8) {
				PDEBUG(DEBUG_AP, " - SUPP_RATES len error\n");
				return;
			}

			supp_rates = u;
			supp_rates_len = ileft;

			u += ileft;
			left -= ileft;
		}

		if (*u == WLAN_EID_DS_PARAMS) {
			u++; left--;
			ileft = *u;
			u++; left--;
			
			if (ileft > left || ileft != 1) {
				PDEBUG(DEBUG_AP, " - DS_PARAMS len error\n");
				return;
			}

			channel = *u;

			u += ileft;
			left -= ileft;
		}
	}

	spin_lock_bh(&local->ap->sta_table_lock);
	sta = ap_get_sta(local->ap, rxdesc->addr2);
	if (sta != NULL)
		atomic_inc(&sta->users);
	spin_unlock_bh(&local->ap->sta_table_lock);

	if (sta == NULL) {
		/* add new AP */
		new_sta = 1;
		sta = ap_add_sta(local->ap, rxdesc->addr2);
		if (sta == NULL) {
			printk(KERN_INFO "prism2: kmalloc failed for AP "
			       "data structure\n");
			return;
		}
		hostap_event_new_sta(local->dev, sta);

		/* mark APs authentication and associated for pseudo ad-hoc
		 * style communication */
		sta->flags = WLAN_STA_AUTH | WLAN_STA_ASSOC;

		if (local->ap->autom_ap_wds) {
			/* schedule a non-interrupt context process to add the
			 * WDS device since register_netdevice() can sleep */
			struct add_wds_data *entry;
			entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
			if (entry) {
				memcpy(entry->addr, sta->addr, ETH_ALEN);
				spin_lock_bh(&local->lock);
				entry->next = local->ap->add_wds_entries;
				local->ap->add_wds_entries = entry;
				spin_unlock_bh(&local->lock);
				PRISM2_SCHEDULE_TASK(&local->ap->
						     add_wds_queue);
			}
		}
	}

	sta->ap = 1;
	if (ssid) {
		sta->u.ap.ssid_len = ssid_len;
		memcpy(sta->u.ap.ssid, ssid, ssid_len);
		sta->u.ap.ssid[ssid_len] = '\0';
	} else {
		sta->u.ap.ssid_len = 0;
		sta->u.ap.ssid[0] = '\0';
	}
	sta->u.ap.channel = channel;
	sta->rx_packets++;
	sta->rx_bytes += len;
	sta->u.ap.last_beacon = sta->last_rx = jiffies;
	sta->capability = capability;
	sta->listen_interval = beacon_int;
	prism2_ap_update_sq(sta, rxdesc);

	atomic_dec(&sta->users);

	if (new_sta) {
		memset(sta->supported_rates, 0, sizeof(sta->supported_rates));
		memcpy(sta->supported_rates, supp_rates, supp_rates_len);
		prism2_check_tx_rates(sta);
	}
}

#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */


/* Called only as a tasklet. */
static void handle_ap_item(local_info_t *local, struct sk_buff *skb)
{
#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
	struct net_device *dev = local->dev;
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */
	u16 fc, type, stype;
	struct hfa384x_rx_frame *rxdesc;

	/* FIX: should give skb->len to handler functions and check that the
	 * buffer is long enough */
	rxdesc = (struct hfa384x_rx_frame *) skb->data;
	fc = __le16_to_cpu(rxdesc->frame_control);
	type = WLAN_FC_GET_TYPE(fc);
	stype = WLAN_FC_GET_STYPE(fc);

#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
	if (!local->hostapd && type == WLAN_FC_TYPE_DATA) {
		PDEBUG(DEBUG_AP, "handle_ap_item - data frame\n");

		if (!(fc & WLAN_FC_TODS) || (fc & WLAN_FC_FROMDS)) {
			if (stype == WLAN_FC_STYPE_NULLFUNC) {
				/* no ToDS nullfunc seems to be used to check
				 * AP association; so send reject message to
				 * speed up re-association */
				ap_handle_dropped_data(local, rxdesc);
				goto done;
			}
			PDEBUG(DEBUG_AP, "   not ToDS frame (fc=0x%04x)\n",
			       fc);
			goto done;
		}

		if (memcmp(rxdesc->addr1, dev->dev_addr, 6)) {
			PDEBUG(DEBUG_AP, "handle_ap_item - addr1(BSSID)="
			       MACSTR " not own MAC\n",
			       MAC2STR(rxdesc->addr1));
			goto done;
		}

		if (local->ap->nullfunc_ack && stype == WLAN_FC_STYPE_NULLFUNC)
			ap_handle_data_nullfunc(local, rxdesc);
		else
			ap_handle_dropped_data(local, rxdesc);
		goto done;
	}
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */

	if (type == WLAN_FC_TYPE_CTRL &&
	    stype == WLAN_FC_STYPE_PSPOLL) {
		handle_pspoll(local, rxdesc);
		goto done;
	}

	if (local->hostapd) {
		PDEBUG(DEBUG_AP, "Unknown frame in AP queue: type=0x%02x "
		       "subtype=0x%02x\n", type, stype);
		goto done;
	}

#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
	if (type != WLAN_FC_TYPE_MGMT) {
		PDEBUG(DEBUG_AP, "handle_ap_item - not a management frame?\n");
		goto done;
	}

	if (stype == WLAN_FC_STYPE_BEACON) {
		handle_beacon(local, rxdesc);
		goto done;
	}

	if (memcmp(rxdesc->addr1, dev->dev_addr, 6)) {
		PDEBUG(DEBUG_AP, "handle_ap_item - addr1(DA)=" MACSTR
		       " not own MAC\n", MAC2STR(rxdesc->addr1));
		goto done;
	}

	if (memcmp(rxdesc->addr3, dev->dev_addr, 6)) {
		PDEBUG(DEBUG_AP, "handle_ap_item - addr3(BSSID)=" MACSTR
		       " not own MAC\n", MAC2STR(rxdesc->addr3));
		goto done;
	}

	switch (stype) {
	case WLAN_FC_STYPE_ASSOC_REQ:
		handle_assoc(local, rxdesc, 0);
		break;
	case WLAN_FC_STYPE_ASSOC_RESP:
		PDEBUG(DEBUG_AP, "==> ASSOC RESP (ignored)\n");
		break;
	case WLAN_FC_STYPE_REASSOC_REQ:
		handle_assoc(local, rxdesc, 1);
		break;
	case WLAN_FC_STYPE_REASSOC_RESP:
		PDEBUG(DEBUG_AP, "==> REASSOC RESP (ignored)\n");
		break;
	case WLAN_FC_STYPE_ATIM:
		PDEBUG(DEBUG_AP, "==> ATIM (ignored)\n");
		break;
	case WLAN_FC_STYPE_DISASSOC:
		handle_disassoc(local, rxdesc);
		break;
	case WLAN_FC_STYPE_AUTH:
		handle_authen(local, rxdesc);
		break;
	case WLAN_FC_STYPE_DEAUTH:
		handle_deauth(local, rxdesc);
		break;
	default:
		PDEBUG(DEBUG_AP, "Unknown mgmt frame subtype 0x%02x\n", stype);
		break;
	}
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */

 done:
	dev_kfree_skb(skb);
}


/* Called only as a tasklet (software IRQ) */
void hostap_rx(struct net_device *dev, struct sk_buff *skb)
{
	local_info_t *local = (local_info_t *) dev->priv;
	u16 fc;
	struct hfa384x_rx_frame *rxdesc;

	if (skb->len < sizeof(*rxdesc))
		goto drop;

	local->stats.rx_packets++;

	rxdesc = (struct hfa384x_rx_frame *) skb->data;
	fc = le16_to_cpu(rxdesc->frame_control);

	if (local->ap->ap_policy == AP_OTHER_AP_SKIP_ALL &&
	    WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
	    WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_BEACON)
		goto drop;

	handle_ap_item(local, skb);
	return;

 drop:
	dev_kfree_skb(skb);
}


/* Called only as a tasklet (software IRQ) */
static void schedule_packet_send(local_info_t *local, struct sta_info *sta)
{
	struct sk_buff *skb;
	struct hfa384x_rx_frame *rxdesc;

	if (skb_queue_empty(&sta->tx_buf))
		return;

	skb = dev_alloc_skb(sizeof(*rxdesc));
	if (skb == NULL) {
		printk(KERN_DEBUG "%s: schedule_packet_send: skb alloc "
		       "failed\n", local->dev->name);
		return;
	}

	rxdesc = (struct hfa384x_rx_frame *) skb_put(skb, sizeof(*rxdesc));

	/* Generate a fake pspoll frame to start packet delivery */
	memset(rxdesc, 0, sizeof(*rxdesc));
	rxdesc->frame_control = __constant_cpu_to_le16(
		(WLAN_FC_TYPE_CTRL << 2) | (WLAN_FC_STYPE_PSPOLL << 4));
	memcpy(rxdesc->addr1, local->dev->dev_addr, 6);
	memcpy(rxdesc->addr2, sta->addr, 6);
	rxdesc->duration_id = __cpu_to_le16(sta->aid | BIT(15) | BIT(14));

	PDEBUG(DEBUG_PS2, "%s: Scheduling buffered packet delivery for "
	       "STA " MACSTR "\n", local->dev->name, MAC2STR(sta->addr));

	skb->protocol = __constant_htons(ETH_P_HOSTAP);
	skb->dev = local->dev;

	hostap_rx(local->dev, skb);
}


#ifdef WIRELESS_EXT
static int prism2_ap_get_sta_qual(local_info_t *local, struct sockaddr addr[],
				  struct iw_quality qual[], int buf_size,
				  int aplist)
{
	struct ap_data *ap = local->ap;
	struct list_head *ptr;
	int count = 0;

	spin_lock_bh(&ap->sta_table_lock);

	for (ptr = ap->sta_list.next; ptr != NULL && ptr != &ap->sta_list;
	     ptr = ptr->next) {
		struct sta_info *sta = (struct sta_info *) ptr;

		if (aplist && !sta->ap)
			continue;
		addr[count].sa_family = ARPHRD_ETHER;
		memcpy(addr[count].sa_data, sta->addr, ETH_ALEN);
		if (sta->last_rx_silence == 0)
			qual[count].qual = sta->last_rx_signal < 27 ?
				0 : (sta->last_rx_signal - 27) * 92 / 127;
		else
			qual[count].qual = sta->last_rx_signal -
				sta->last_rx_silence - 35;
		qual[count].level = HFA384X_LEVEL_TO_dBm(sta->last_rx_signal);
		qual[count].noise = HFA384X_LEVEL_TO_dBm(sta->last_rx_silence);
		qual[count].updated = sta->last_rx_updated;

		sta->last_rx_updated = 0;

		count++;
		if (count >= buf_size)
			break;
	}
	spin_unlock_bh(&ap->sta_table_lock);

	return count;
}


#if WIRELESS_EXT > 13
/* Translate our list of Access Points & Stations to a card independant
 * format that the Wireless Tools will understand - Jean II */
static int prism2_ap_translate_scan(struct net_device *dev, char *buffer)
{
	local_info_t *local = (local_info_t *) dev->priv;
	struct ap_data *ap = local->ap;
	struct list_head *ptr;
	struct iw_event iwe;
	char *current_ev = buffer;
	char *end_buf = buffer + IW_SCAN_MAX_DATA;
#if !defined(PRISM2_NO_KERNEL_IEEE80211_MGMT) && (WIRELESS_EXT > 14)
	char buf[64];
#endif

	spin_lock_bh(&ap->sta_table_lock);

	for (ptr = ap->sta_list.next; ptr != NULL && ptr != &ap->sta_list;
	     ptr = ptr->next) {
		struct sta_info *sta = (struct sta_info *) ptr;

		/* First entry *MUST* be the AP MAC address */
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWAP;
		iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
		memcpy(iwe.u.ap_addr.sa_data, sta->addr, ETH_ALEN);
		iwe.len = IW_EV_ADDR_LEN;
		current_ev = iwe_stream_add_event(current_ev, end_buf, &iwe,
						  IW_EV_ADDR_LEN);

		/* Use the mode to indicate if it's a station or
		 * an Access Point */
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWMODE;
		if (sta->ap)
			iwe.u.mode = IW_MODE_MASTER;
		else
			iwe.u.mode = IW_MODE_INFRA;
		iwe.len = IW_EV_UINT_LEN;
		current_ev = iwe_stream_add_event(current_ev, end_buf, &iwe,
						  IW_EV_UINT_LEN);

		/* Some quality */
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = IWEVQUAL;
		if (sta->last_rx_silence == 0)
			iwe.u.qual.qual = sta->last_rx_signal < 27 ?
				0 : (sta->last_rx_signal - 27) * 92 / 127;
		else
			iwe.u.qual.qual = sta->last_rx_signal -
				sta->last_rx_silence - 35;
		iwe.u.qual.level = HFA384X_LEVEL_TO_dBm(sta->last_rx_signal);
		iwe.u.qual.noise = HFA384X_LEVEL_TO_dBm(sta->last_rx_silence);
		iwe.u.qual.updated = sta->last_rx_updated;
		iwe.len = IW_EV_QUAL_LEN;
		current_ev = iwe_stream_add_event(current_ev, end_buf, &iwe,
						  IW_EV_QUAL_LEN);

#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
		if (sta->ap) {
			memset(&iwe, 0, sizeof(iwe));
			iwe.cmd = SIOCGIWESSID;
			iwe.u.data.length = sta->u.ap.ssid_len;
			iwe.u.data.flags = 1;
			current_ev = iwe_stream_add_point(current_ev, end_buf,
							  &iwe,
							  sta->u.ap.ssid);

			memset(&iwe, 0, sizeof(iwe));
			iwe.cmd = SIOCGIWENCODE;
			if (sta->capability & WLAN_CAPABILITY_PRIVACY)
				iwe.u.data.flags =
					IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
			else
				iwe.u.data.flags = IW_ENCODE_DISABLED;
			current_ev = iwe_stream_add_point(current_ev, end_buf,
							  &iwe,
							  sta->u.ap.ssid
							  /* 0 byte memcpy */);

			if (sta->u.ap.channel > 0 &&
			    sta->u.ap.channel <= FREQ_COUNT) {
				memset(&iwe, 0, sizeof(iwe));
				iwe.cmd = SIOCGIWFREQ;
				iwe.u.freq.m = freq_list[sta->u.ap.channel - 1]
					* 100000;
				iwe.u.freq.e = 1;
				current_ev = iwe_stream_add_event(
					current_ev, end_buf, &iwe,
					IW_EV_FREQ_LEN);
			}

#if WIRELESS_EXT > 14
			memset(&iwe, 0, sizeof(iwe));
			iwe.cmd = IWEVCUSTOM;
			sprintf(buf, "beacon_interval=%d",
				sta->listen_interval);
			iwe.u.data.length = strlen(buf);
			current_ev = iwe_stream_add_point(current_ev, end_buf,
							  &iwe, buf);
#endif /* WIRELESS_EXT > 14 */
		}
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */

		sta->last_rx_updated = 0;

		/* To be continued, we should make good use of IWEVCUSTOM */
	}

	spin_unlock_bh(&ap->sta_table_lock);

	return current_ev - buffer;
}
#endif /* WIRELESS_EXT > 13 */
#endif /* WIRELESS_EXT */


static int prism2_hostapd_add_sta(struct ap_data *ap,
				  struct prism2_hostapd_param *param)
{
	struct sta_info *sta;

	spin_lock_bh(&ap->sta_table_lock);
	sta = ap_get_sta(ap, param->sta_addr);
	if (sta)
		atomic_inc(&sta->users);
	spin_unlock_bh(&ap->sta_table_lock);

	if (sta == NULL) {
		sta = ap_add_sta(ap, param->sta_addr);
		if (sta == NULL)
			return -1;
	}

	if (!(sta->flags & WLAN_STA_ASSOC) && !sta->ap && sta->local)
		hostap_event_new_sta(sta->local->dev, sta);

	sta->flags |= WLAN_STA_AUTH | WLAN_STA_ASSOC;
	sta->last_rx = jiffies;
	sta->aid = param->u.add_sta.aid;
	sta->capability = param->u.add_sta.capability;
	sta->tx_supp_rates = param->u.add_sta.tx_supp_rates;
	if (sta->tx_supp_rates & WLAN_RATE_1M)
		sta->supported_rates[0] = 2;
	if (sta->tx_supp_rates & WLAN_RATE_2M)
		sta->supported_rates[1] = 4;
 	if (sta->tx_supp_rates & WLAN_RATE_5M5)
		sta->supported_rates[2] = 11;
	if (sta->tx_supp_rates & WLAN_RATE_11M)
		sta->supported_rates[3] = 22;
	prism2_check_tx_rates(sta);
	atomic_dec(&sta->users);
	return 0;
}


static int prism2_hostapd_remove_sta(struct ap_data *ap,
				     struct prism2_hostapd_param *param)
{
	struct sta_info *sta;

	spin_lock_bh(&ap->sta_table_lock);
	sta = ap_get_sta(ap, param->sta_addr);
	if (sta) {
		ap_sta_hash_del(ap, sta);
		list_del(&sta->list);
	}
	spin_unlock_bh(&ap->sta_table_lock);

	if (!sta)
		return -ENOENT;

	if ((sta->flags & WLAN_STA_ASSOC) && !sta->ap && sta->local)
		hostap_event_expired_sta(sta->local->dev, sta);
	ap_free_sta(ap, sta);

	return 0;
}


static int prism2_hostapd_get_info_sta(struct ap_data *ap,
				       struct prism2_hostapd_param *param)
{
	struct sta_info *sta;

	spin_lock_bh(&ap->sta_table_lock);
	sta = ap_get_sta(ap, param->sta_addr);
	if (sta)
		atomic_inc(&sta->users);
	spin_unlock_bh(&ap->sta_table_lock);

	if (!sta)
		return -ENOENT;

	param->u.get_info_sta.inactive_sec = (jiffies - sta->last_rx) / HZ;
	param->u.get_info_sta.txexc = sta->txexc;

	atomic_dec(&sta->users);

	return 1;
}


static int prism2_hostapd_reset_txexc_sta(struct ap_data *ap,
					  struct prism2_hostapd_param *param)
{
	struct sta_info *sta;

	spin_lock_bh(&ap->sta_table_lock);
	sta = ap_get_sta(ap, param->sta_addr);
	if (sta)
		sta->txexc = 0;
	spin_unlock_bh(&ap->sta_table_lock);

	return sta ? 0 : -ENOENT;
}


static int prism2_hostapd_set_flags_sta(struct ap_data *ap,
					struct prism2_hostapd_param *param)
{
	struct sta_info *sta;

	spin_lock_bh(&ap->sta_table_lock);
	sta = ap_get_sta(ap, param->sta_addr);
	if (sta) {
		sta->flags |= param->u.set_flags_sta.flags_or;
		sta->flags &= param->u.set_flags_sta.flags_and;
	}
	spin_unlock_bh(&ap->sta_table_lock);

	if (!sta)
		return -ENOENT;

	return 0;
}


static int prism2_hostapd(struct ap_data *ap,
			  struct prism2_hostapd_param *param)
{
	switch (param->cmd) {
	case PRISM2_HOSTAPD_FLUSH:
		ap_control_kickall(ap);
		return 0;
	case PRISM2_HOSTAPD_ADD_STA:
		return prism2_hostapd_add_sta(ap, param);
	case PRISM2_HOSTAPD_REMOVE_STA:
		return prism2_hostapd_remove_sta(ap, param);
	case PRISM2_HOSTAPD_GET_INFO_STA:
		return prism2_hostapd_get_info_sta(ap, param);
	case PRISM2_HOSTAPD_RESET_TXEXC_STA:
		return prism2_hostapd_reset_txexc_sta(ap, param);
	case PRISM2_HOSTAPD_SET_FLAGS_STA:
		return prism2_hostapd_set_flags_sta(ap, param);
	default:
		printk(KERN_WARNING "prism2_hostapd: unknown cmd=%d\n",
		       param->cmd);
		return -EOPNOTSUPP;
	}
}


/* Update station info for host-based TX rate control and return current
 * TX rate */
static int ap_update_sta_tx_rate(struct sta_info *sta, struct net_device *dev)
{
	int ret = sta->tx_rate;
	local_info_t *local = dev->priv;

	sta->tx_count[sta->tx_rate_idx]++;
	sta->tx_since_last_failure++;
	if (sta->tx_since_last_failure > WLAN_RATE_UPDATE_COUNT &&
	    sta->tx_rate_idx < sta->tx_max_rate) {
		/* use next higher rate */
		int old_rate, new_rate;
		old_rate = new_rate = sta->tx_rate_idx;
		while (new_rate < sta->tx_max_rate) {
			new_rate++;
			if (ap_tx_rate_ok(new_rate, sta, local)) {
				sta->tx_rate_idx = new_rate;
				break;
			}
		}
		if (old_rate != sta->tx_rate_idx) {
			switch (sta->tx_rate_idx) {
			case 0: sta->tx_rate = 10; break;
			case 1: sta->tx_rate = 20; break;
			case 2: sta->tx_rate = 55; break;
			case 3: sta->tx_rate = 110; break;
			default: sta->tx_rate = 0; break;
			}
			PDEBUG(DEBUG_AP, "%s: STA " MACSTR " TX rate raised to"
			       " %d\n", dev->name, MAC2STR(sta->addr),
			       sta->tx_rate);
		}
		sta->tx_since_last_failure = 0;
	}

	return ret;
}


/* Called only from software IRQ. Called for each TX frame prior possible
 * encryption and transmit. */
ap_tx_ret hostap_handle_sta_tx(local_info_t *local, struct sk_buff *skb,
			       struct hfa384x_tx_frame *txdesc, int wds,
			       int host_encrypt,
			       struct prism2_crypt_data **crypt,
			       void **sta_ptr)
{
	struct sta_info *sta = NULL;
	int set_tim, ret;

	ret = AP_TX_CONTINUE;
	if (local->ap == NULL)
		goto out;

	if (txdesc->addr1[0] & 0x01) {
		/* broadcast/multicast frame - no AP related processing */
		goto out;
	}

	/* unicast packet - check whether destination STA is associated */
	spin_lock(&local->ap->sta_table_lock);
	sta = ap_get_sta(local->ap, txdesc->addr1);
	if (sta)
		atomic_inc(&sta->users);
	spin_unlock(&local->ap->sta_table_lock);

	if (local->iw_mode == IW_MODE_MASTER && sta == NULL && !wds) {
		/* remove FromDS flag from (pseudo) ad-hoc style
		 * communication between APs */
		txdesc->frame_control &=
			~(__constant_cpu_to_le16(WLAN_FC_FROMDS));

		printk(KERN_DEBUG "AP: packet to non-associated STA "
		       MACSTR "\n", MAC2STR(txdesc->addr1));
	}

	if (sta == NULL)
		goto out;

	if (!(sta->flags & WLAN_STA_AUTHORIZED))
		ret = AP_TX_CONTINUE_NOT_AUTHORIZED;

	/* Set tx_rate if using host-based TX rate control */
	if (!local->fw_tx_rate_control)
		local->ap->last_tx_rate = txdesc->tx_rate =
			ap_update_sta_tx_rate(sta, local->dev);

	if (local->iw_mode != IW_MODE_MASTER)
		goto out;

	if (!(sta->flags & WLAN_STA_PS))
		goto out;

	if (memcmp(skb->cb, AP_SKB_CB_MAGIC, AP_SKB_CB_MAGIC_LEN) == 0) {
		if (skb->cb[AP_SKB_CB_MAGIC_LEN] & AP_SKB_CB_ADD_MOREDATA) {
			/* indicate to STA that more frames follow */
			txdesc->frame_control |=
				__constant_cpu_to_le16(WLAN_FC_MOREDATA);
		}

		if (skb->cb[AP_SKB_CB_MAGIC_LEN] & AP_SKB_CB_BUFFERED_FRAME) {
			/* packet was already buffered and now send due to
			 * PS poll, so do not rebuffer it */
			goto out;
		}
	}

	if (skb_queue_len(&sta->tx_buf) >= STA_MAX_TX_BUFFER) {
		PDEBUG(DEBUG_PS, "%s: No more space in STA (" MACSTR ")'s PS "
		       "mode buffer\n", local->dev->name, MAC2STR(sta->addr));
		/* Make sure that TIM is set for the station (it might not be
		 * after AP wlan hw reset). */
		hostap_set_tim(local, sta->aid, 1);
		sta->flags |= WLAN_STA_TIM;
		ret = AP_TX_DROP;
		goto out;
	}

	/* STA in PS mode, buffer frame for later delivery */
	set_tim = skb_queue_empty(&sta->tx_buf);
	skb_queue_tail(&sta->tx_buf, skb);
	/* FIX: could save RX time to skb and expire buffered frames after
	 * some time if STA does not poll for them */

	if (set_tim) {
		if (sta->flags & WLAN_STA_TIM)
			PDEBUG(DEBUG_PS2, "Re-setting TIM for aid %d\n",
			       sta->aid);
		hostap_set_tim(local, sta->aid, 1);
		sta->flags |= WLAN_STA_TIM;
	}

	ret = AP_TX_BUFFERED;

 out:
	if (sta != NULL) {
		if (ret == AP_TX_CONTINUE ||
		    ret == AP_TX_CONTINUE_NOT_AUTHORIZED) {
			sta->tx_packets++;
			sta->tx_bytes += le16_to_cpu(txdesc->data_len) + 36;
			sta->last_tx = jiffies;
		}

		if ((ret == AP_TX_CONTINUE ||
		     ret == AP_TX_CONTINUE_NOT_AUTHORIZED) &&
		    sta->crypt && host_encrypt) {
			*crypt = sta->crypt;
			*sta_ptr = sta; /* hostap_handle_sta_release() will be
					 * called to release sta info later */
		} else
			atomic_dec(&sta->users);
	}

	return ret;
}


void hostap_handle_sta_release(void *ptr)
{
	struct sta_info *sta = ptr;
	atomic_dec(&sta->users);
}


/* Called only as a tasklet (software IRQ) */
void hostap_handle_sta_tx_exc(local_info_t *local,
			      struct hfa384x_tx_frame *txdesc)
{
	struct sta_info *sta;

	spin_lock(&local->ap->sta_table_lock);
	/* FIX: is addr1 correct for all frame types? */
	sta = ap_get_sta(local->ap, txdesc->addr1);
	if (!sta) {
		spin_unlock(&local->ap->sta_table_lock);
		PDEBUG(DEBUG_AP, "%s: Could not find STA for this TX error\n",
		       local->dev->name);
		return;
	}

	sta->txexc++;
	sta->tx_since_last_failure = 0;
	if (sta->tx_rate_idx > 0 && txdesc->tx_rate <= sta->tx_rate) {
		/* use next lower rate */
		int old, rate;
		old = rate = sta->tx_rate_idx;
		while (rate > 0) {
			rate--;
			if (ap_tx_rate_ok(rate, sta, local)) {
				sta->tx_rate_idx = rate;
				break;
			}
		}
		if (old != sta->tx_rate_idx) {
			switch (sta->tx_rate_idx) {
			case 0: sta->tx_rate = 10; break;
			case 1: sta->tx_rate = 20; break;
			case 2: sta->tx_rate = 55; break;
			case 3: sta->tx_rate = 110; break;
			default: sta->tx_rate = 0; break;
			}
			PDEBUG(DEBUG_AP, "%s: STA " MACSTR " TX rate lowered "
			       "to %d\n", local->dev->name, MAC2STR(sta->addr),
			       sta->tx_rate);
		}
	}
	spin_unlock(&local->ap->sta_table_lock);
}


static void hostap_update_sta_ps2(local_info_t *local, struct sta_info *sta,
				  int pwrmgt, int type, int stype)
{
	if (pwrmgt && !(sta->flags & WLAN_STA_PS)) {
		sta->flags |= WLAN_STA_PS;
		PDEBUG(DEBUG_PS2, "STA " MACSTR " changed to use PS "
		       "mode (type=0x%02X, stype=0x%02X)\n",
		       MAC2STR(sta->addr), type, stype);
	} else if (!pwrmgt && (sta->flags & WLAN_STA_PS)) {
		sta->flags &= ~WLAN_STA_PS;
		PDEBUG(DEBUG_PS2, "STA " MACSTR " changed to not use "
		       "PS mode (type=0x%02X, stype=0x%02X)\n",
		       MAC2STR(sta->addr), type, stype);
		if (type != WLAN_FC_TYPE_CTRL || stype != WLAN_FC_STYPE_PSPOLL)
			schedule_packet_send(local, sta);
	}
}


/* Called only as a tasklet (software IRQ). Called for each RX frame to update
 * STA power saving state. pwrmgt is a flag from 802.11 frame_control field. */
int hostap_update_sta_ps(local_info_t *local, struct hostap_ieee80211_hdr *hdr)
{
	struct sta_info *sta;
	u16 fc;

	spin_lock(&local->ap->sta_table_lock);
	sta = ap_get_sta(local->ap, hdr->addr2);
	if (sta)
		atomic_inc(&sta->users);
	spin_unlock(&local->ap->sta_table_lock);

	if (!sta)
		return -1;

	fc = le16_to_cpu(hdr->frame_control);
	hostap_update_sta_ps2(local, sta, fc & WLAN_FC_PWRMGT,
			      WLAN_FC_GET_TYPE(fc), WLAN_FC_GET_STYPE(fc));

	atomic_dec(&sta->users);
	return 0;
}


/* Called only as a tasklet (software IRQ). Called for each RX frame after
 * getting RX header and payload from hardware. */
ap_rx_ret hostap_handle_sta_rx(local_info_t *local, struct net_device *dev,
			       struct sk_buff *skb, int wds)
{
	int ret;
	struct sta_info *sta;
	u16 fc, type, stype;
	struct hfa384x_rx_frame *rxdesc;

	if (local->ap == NULL)
		return AP_RX_CONTINUE;

	rxdesc = (struct hfa384x_rx_frame *) skb->data;

	fc = le16_to_cpu(rxdesc->frame_control);
	type = WLAN_FC_GET_TYPE(fc);
	stype = WLAN_FC_GET_STYPE(fc);

	spin_lock(&local->ap->sta_table_lock);
	sta = ap_get_sta(local->ap, rxdesc->addr2);
	if (sta)
		atomic_inc(&sta->users);
	spin_unlock(&local->ap->sta_table_lock);

	if (sta && !(sta->flags & WLAN_STA_AUTHORIZED))
		ret = AP_RX_CONTINUE_NOT_AUTHORIZED;
	else
		ret = AP_RX_CONTINUE;


	if (fc & WLAN_FC_TODS) {
		if (!wds && (sta == NULL || !(sta->flags & WLAN_STA_ASSOC))) {
			if (local->hostapd) {
				local->func->rx_80211(local->apdev, skb,
						      PRISM2_RX_NON_ASSOC,
						      NULL, 0);
#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
			} else {
				printk(KERN_DEBUG "%s: dropped received packet"
				       " from non-associated STA " MACSTR
				       " (type=0x%02x, subtype=0x%02x)\n",
				       dev->name, MAC2STR(rxdesc->addr2), type,
				       stype);
				skb->protocol = __constant_htons(ETH_P_HOSTAP);
				hostap_rx(dev, skb);
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */
			}
			ret = AP_RX_EXIT;
			goto out;
		}
	} else if (fc & WLAN_FC_FROMDS) {
		if (!wds) {
			/* FromDS frame - not for us; probably
			 * broadcast/multicast in another BSS - drop */
			if (memcmp(rxdesc->addr1, dev->dev_addr, 6) == 0) {
				printk(KERN_DEBUG "Odd.. FromDS packet "
				       "received with own BSSID\n");
				hostap_dump_rx_header(dev->name, rxdesc);
			}
			ret = AP_RX_DROP;
			goto out;
		}
	} else if (stype == WLAN_FC_STYPE_NULLFUNC && sta == NULL &&
		   memcmp(rxdesc->addr1, dev->dev_addr, 6) == 0) {

		if (local->hostapd) {
			local->func->rx_80211(local->apdev, skb,
					      PRISM2_RX_NON_ASSOC, NULL, 0);
#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
		} else {
			/* At least Lucent f/w seems to send data::nullfunc
			 * frames with no ToDS flag when the current AP returns
			 * after being unavailable for some time. Speed up
			 * re-association by informing the station about it not
			 * being associated. */
			printk(KERN_DEBUG "%s: rejected received nullfunc "
			       "frame without ToDS from not associated STA "
			       MACSTR "\n",
			       dev->name, MAC2STR(rxdesc->addr2));
			skb->protocol = __constant_htons(ETH_P_HOSTAP);
			hostap_rx(dev, skb);
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */
		}
		ret = AP_RX_EXIT;
		goto out;
	} else if (stype == WLAN_FC_STYPE_NULLFUNC) {
		/* At least Lucent cards seem to send periodic nullfunc
		 * frames with ToDS. Let these through to update SQ
		 * stats and PS state. Nullfunc frames do not contain
		 * any data and they will be dropped below. */
	} else {
		printk(KERN_DEBUG "%s: dropped received packet from "
		       MACSTR " with no ToDS flag (type=0x%02x, "
		       "subtype=0x%02x)\n", dev->name,
		       MAC2STR(rxdesc->addr2), type, stype);
		hostap_dump_rx_header(dev->name, rxdesc);
		ret = AP_RX_DROP;
		goto out;
	}

	if (sta) {
		prism2_ap_update_sq(sta, rxdesc);

		hostap_update_sta_ps2(local, sta, fc & WLAN_FC_PWRMGT,
				      type, stype);

		sta->rx_packets++;
		sta->rx_bytes += le16_to_cpu(rxdesc->data_len);
		sta->last_rx = jiffies;
	}

	if (local->ap->nullfunc_ack && stype == WLAN_FC_STYPE_NULLFUNC &&
	    fc & WLAN_FC_TODS) {
		if (local->hostapd) {
			local->func->rx_80211(local->apdev, skb,
					      PRISM2_RX_NULLFUNC_ACK, NULL, 0);
#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
		} else {
			/* some STA f/w's seem to require control::ACK frame
			 * for data::nullfunc, but Prism2 f/w 0.8.0 (at least
			 * from Compaq) does not send this.. Try to generate
			 * ACK for these frames from the host driver to make
			 * power saving work with, e.g., Lucent WaveLAN f/w */
			skb->protocol = __constant_htons(ETH_P_HOSTAP);
			hostap_rx(dev, skb);
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */
		}
		ret = AP_RX_EXIT;
		goto out;
	}

 out:
	if (sta)
		atomic_dec(&sta->users);

	return ret;
}


/* Called only as a tasklet (software IRQ) */
int hostap_handle_sta_crypto(local_info_t *local,
			     struct hfa384x_rx_frame *rxdesc,
			     struct prism2_crypt_data **crypt, void **sta_ptr)
{
	struct sta_info *sta;

	spin_lock(&local->ap->sta_table_lock);
	sta = ap_get_sta(local->ap, rxdesc->addr2);
	if (sta)
		atomic_inc(&sta->users);
	spin_unlock(&local->ap->sta_table_lock);

	if (!sta)
		return -1;

	if (sta->crypt) {
		*crypt = sta->crypt;
		*sta_ptr = sta;
		/* hostap_handle_sta_release() will be called to release STA
		 * info */
	} else
		atomic_dec(&sta->users);

	return 0;
}


/* Called only as a tasklet (software IRQ) */
int hostap_is_sta_assoc(struct ap_data *ap, u8 *sta_addr)
{
	struct sta_info *sta;
	int ret = 0;

	spin_lock(&ap->sta_table_lock);
	sta = ap_get_sta(ap, sta_addr);
	if (sta != NULL && (sta->flags & WLAN_STA_ASSOC) && !sta->ap)
		ret = 1;
	spin_unlock(&ap->sta_table_lock);

	return ret;
}


/* Called only as a tasklet (software IRQ) */
int hostap_add_sta(struct ap_data *ap, u8 *sta_addr)
{
	struct sta_info *sta;
	int ret = 1;

	if (!ap)
		return -1;

	spin_lock(&ap->sta_table_lock);
	sta = ap_get_sta(ap, sta_addr);
	if (sta)
		ret = 0;
	spin_unlock(&ap->sta_table_lock);

	if (ret == 1) {
		sta = ap_add_sta(ap, sta_addr);
		if (!sta)
			ret = -1;
		sta->flags = WLAN_STA_AUTH | WLAN_STA_ASSOC;
		sta->ap = 1;
		memset(sta->supported_rates, 0, sizeof(sta->supported_rates));
		/* No way of knowing which rates are supported since we did not
		 * get supported rates element from beacon/assoc req. Assume
		 * that remote end supports all 802.11b rates. */
		sta->supported_rates[0] = 0x82;
		sta->supported_rates[1] = 0x84;
		sta->supported_rates[2] = 0x0b;
		sta->supported_rates[3] = 0x16;
		sta->tx_supp_rates = WLAN_RATE_1M | WLAN_RATE_2M |
			WLAN_RATE_5M5 | WLAN_RATE_11M;
		sta->tx_rate = 110;
		sta->tx_max_rate = sta->tx_rate_idx = 3;
	}

	return ret;
}


/* Called only as a tasklet (software IRQ) */
int hostap_update_rx_stats(struct ap_data *ap, struct hfa384x_rx_frame *rxdesc)
{
	struct sta_info *sta;

	if (!ap || !rxdesc)
		return -1;

	spin_lock(&ap->sta_table_lock);
	sta = ap_get_sta(ap, rxdesc->addr2);
	if (sta)
		prism2_ap_update_sq(sta, rxdesc);
	spin_unlock(&ap->sta_table_lock);

	return sta ? 0 : -1;
}


void hostap_update_rates(local_info_t *local)
{
	struct list_head *ptr;
	struct ap_data *ap = local->ap;

	if (!ap)
		return;

	spin_lock_bh(&ap->sta_table_lock);
	for (ptr = ap->sta_list.next; ptr != &ap->sta_list; ptr = ptr->next) {
		struct sta_info *sta = (struct sta_info *) ptr;
		prism2_check_tx_rates(sta);
	}
	spin_unlock_bh(&ap->sta_table_lock);
}


static void * ap_crypt_get_ptrs(struct ap_data *ap, u8 *addr, int permanent,
				struct prism2_crypt_data ***crypt)
{
	struct sta_info *sta;

	spin_lock_bh(&ap->sta_table_lock);
	sta = ap_get_sta(ap, addr);
	if (sta)
		atomic_inc(&sta->users);
	spin_unlock_bh(&ap->sta_table_lock);

	if (!sta && permanent)
		sta = ap_add_sta(ap, addr);

	if (!sta)
		return NULL;

	if (permanent)
		sta->flags |= WLAN_STA_PERM;

	*crypt = &sta->crypt;

	return sta;
}


void hostap_add_wds_links(local_info_t *local)
{
	struct ap_data *ap = local->ap;
	struct list_head *ptr;
	struct add_wds_data *entry;

	spin_lock_bh(&ap->sta_table_lock);
	for (ptr = ap->sta_list.next; ptr != &ap->sta_list; ptr = ptr->next) {
		struct sta_info *sta = (struct sta_info *) ptr;
		if (!sta->ap)
			continue;
		entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
		if (!entry)
			break;
		memcpy(entry->addr, sta->addr, ETH_ALEN);
		spin_lock_bh(&local->lock);
		entry->next = local->ap->add_wds_entries;
		local->ap->add_wds_entries = entry;
		spin_unlock_bh(&local->lock);
	}
	spin_unlock_bh(&ap->sta_table_lock);

	PRISM2_SCHEDULE_TASK(&local->ap->add_wds_queue);
}


void hostap_add_wds_link(local_info_t *local, u8 *addr)
{
	struct add_wds_data *entry;

	entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
	if (!entry)
		return;
	memcpy(entry->addr, addr, ETH_ALEN);
	spin_lock_bh(&local->lock);
	entry->next = local->ap->add_wds_entries;
	local->ap->add_wds_entries = entry;
	spin_unlock_bh(&local->lock);

	PRISM2_SCHEDULE_TASK(&local->ap->add_wds_queue);
}


EXPORT_SYMBOL(hostap_rx);
EXPORT_SYMBOL(hostap_init_data);
EXPORT_SYMBOL(hostap_free_data);
EXPORT_SYMBOL(hostap_check_sta_fw_version);
EXPORT_SYMBOL(hostap_handle_sta_tx);
EXPORT_SYMBOL(hostap_handle_sta_release);
EXPORT_SYMBOL(hostap_handle_sta_tx_exc);
EXPORT_SYMBOL(hostap_update_sta_ps);
EXPORT_SYMBOL(hostap_handle_sta_rx);
EXPORT_SYMBOL(hostap_handle_sta_crypto);
EXPORT_SYMBOL(hostap_is_sta_assoc);
EXPORT_SYMBOL(hostap_add_sta);
EXPORT_SYMBOL(hostap_update_rx_stats);
EXPORT_SYMBOL(hostap_update_rates);
EXPORT_SYMBOL(hostap_add_wds_links);
EXPORT_SYMBOL(hostap_add_wds_link);
#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
EXPORT_SYMBOL(hostap_deauth_all_stas);
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */
