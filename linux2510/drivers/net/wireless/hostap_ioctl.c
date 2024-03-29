/* ioctl() (mostly Linux Wireless Extensions) routines for Host AP driver */

#ifdef in_atomic
/* Get kernel_locked() for in_atomic() */
#include <linux/smp_lock.h>
#endif

typedef struct iw_freq      iwfreq; //ryc++

#ifdef WIRELESS_EXT

/* Conversion to new driver API by Jean II */

#if WIRELESS_EXT <= 12
/* Wireless extensions backward compatibility */

/* Dummy prototype, as we don't really need it */
struct iw_request_info;
#endif /* WIRELESS_EXT <= 12 */


#if WIRELESS_EXT >= 15
/* Wireless ext ver15 allows verification of iwpriv support and sub-ioctls can
 * be included even if not especially configured. */
#ifndef PRISM2_USE_WE_SUB_IOCTLS
#define PRISM2_USE_WE_SUB_IOCTLS
#endif /* PRISM2_USE_WE_SUB_IOCTLS */

/* Assume that hosts using new wireless ext also have new wireless tools
 * (ver >= 25) */
#ifndef PRISM2_USE_WE_TYPE_ADDR
#define PRISM2_USE_WE_TYPE_ADDR
#endif /* PRISM2_USE_WE_TYPE_ADDR */
#endif /* WIRELESS_EXT >= 15 */


#ifdef PRISM2_USE_WE_TYPE_ADDR
/* Added in WIRELESS_EXT 15, but can be used with older versions assuming
 * iwpriv ver >= 25 */
#ifndef IW_PRIV_TYPE_ADDR
#define IW_PRIV_TYPE_ADDR 0x6000
#endif /* IW_PRIV_TYPE_ADDR */
#endif /* PRISM2_USE_WE_TYPE_ADDR */


#if WIRELESS_EXT < 9
struct iw_point {
	caddr_t pointer;
	__u16 length;
	__u16 flags;
};
#endif /* WIRELESS_EXT < 9 */


static struct iw_statistics *hostap_get_wireless_stats(struct net_device *dev)
{
	local_info_t *local = (local_info_t *) dev->priv;

	local->wstats.status = 0;
	local->wstats.discard.code =
		local->comm_tallies.rx_discards_wep_undecryptable;
	local->wstats.discard.misc =
		local->comm_tallies.rx_fcs_errors +
		local->comm_tallies.rx_discards_no_buffer +
		local->comm_tallies.tx_discards_wrong_sa;

#if WIRELESS_EXT > 11
	local->wstats.discard.retries =
		local->comm_tallies.tx_retry_limit_exceeded;
	local->wstats.discard.fragment =
		local->comm_tallies.rx_message_in_bad_msg_fragments;
#endif /* WIRELESS_EXT > 11 */

	if (local->iw_mode != IW_MODE_MASTER &&
	    local->iw_mode != IW_MODE_REPEAT) {
		struct hfa384x_comms_quality sq;
#ifdef in_atomic
		/* FIX: get_rid() will sleep and it must not be called
		 * in interrupt context or while atomic. However, this
		 * function seems to be called while atomic (at least in Linux
		 * 2.5.59). Now, we just avoid illegal call, but in this case
		 * the signal quality values are not shown. Statistics could be
		 * collected before, if this really needs to be called while
		 * atomic. */
		if (in_atomic()) {
			printk(KERN_DEBUG "%s: hostap_get_wireless_stats() "
			       "called while atomic - skipping signal "
			       "quality query\n", dev->name);
		} else
#endif /* in_atomic */
		if (local->func->get_rid(local->dev,
						HFA384X_RID_COMMSQUALITY,
						&sq, sizeof(sq), 1) >= 0) {
			local->wstats.qual.qual = le16_to_cpu(sq.comm_qual);
			local->wstats.qual.level = HFA384X_LEVEL_TO_dBm(
				le16_to_cpu(sq.signal_level));
			local->wstats.qual.noise = HFA384X_LEVEL_TO_dBm(
				le16_to_cpu(sq.noise_level));
			local->wstats.qual.updated = 7;
		}
	}

	return &local->wstats;
}


static int prism2_get_datarates(struct net_device *dev, u8 *rates)
{
	local_info_t *local = (local_info_t *) dev->priv;
	u8 buf[12];
	int len;
	u16 val;

	len = local->func->get_rid(dev, HFA384X_RID_SUPPORTEDDATARATES, buf,
				   sizeof(buf), 0);
	if (len < 2)
		return 0;

	val = le16_to_cpu(*(u16 *) buf); /* string length */

	if (len - 2 < val || val > 10)
		return 0;

	memcpy(rates, buf + 2, val);
	return val;
}


static int prism2_get_name(struct net_device *dev,
			   struct iw_request_info *info,
			   char *name, char *extra)
{
	u8 rates[10];
	int len, i, over2 = 0;

	len = prism2_get_datarates(dev, rates);

	for (i = 0; i < len; i++) {
		if (rates[i] == 0x0b || rates[i] == 0x16) {
			over2 = 1;
			break;
		}
	}

	strcpy(name, over2 ? "IEEE 802.11b" : "IEEE 802.11-DS");

	return 0;
}


static void prism2_crypt_delayed_deinit(local_info_t *local,
					struct prism2_crypt_data **crypt)
{
	struct prism2_crypt_data *tmp;
	unsigned long flags;

	tmp = *crypt;
	*crypt = NULL;

	if (tmp == NULL)
		return;

	/* must not run ops->deinit() while there may be pending encrypt or
	 * decrypt operations. Use a list of delayed deinits to avoid needing
	 * locking. */

	spin_lock_irqsave(&local->lock, flags);
	list_add(&tmp->list, &local->crypt_deinit_list);
	if (!timer_pending(&local->crypt_deinit_timer)) {
		local->crypt_deinit_timer.expires = jiffies + HZ;
		add_timer(&local->crypt_deinit_timer);
	}
	spin_unlock_irqrestore(&local->lock, flags);
}


#if WIRELESS_EXT > 8
static int prism2_ioctl_siwencode(struct net_device *dev,
				  struct iw_request_info *info,
				  struct iw_point *erq, char *keybuf)
{
	local_info_t *local = (local_info_t *) dev->priv;
	int i;
	int first = 0;

	if (erq->flags & IW_ENCODE_DISABLED) {
		prism2_crypt_delayed_deinit(local, &local->crypt);
		goto done;
	}

	if (local->crypt != NULL && local->crypt->ops != NULL &&
	    strcmp(local->crypt->ops->name, "WEP") != 0) {
		/* changing to use WEP; deinit previously used algorithm */
		prism2_crypt_delayed_deinit(local, &local->crypt);
	}

	if (local->crypt == NULL) {
		struct prism2_crypt_data *new_crypt;

		/* take WEP into use */
		new_crypt = (struct prism2_crypt_data *)
			kmalloc(sizeof(struct prism2_crypt_data), GFP_KERNEL);
		if (new_crypt == NULL)
			return -ENOMEM;
		memset(new_crypt, 0, sizeof(struct prism2_crypt_data));
		new_crypt->ops = hostap_get_crypto_ops("WEP");
		if (!new_crypt->ops) {
			request_module("hostap_crypt_wep");
			new_crypt->ops = hostap_get_crypto_ops("WEP");
		}
		if (new_crypt->ops)
			new_crypt->priv = new_crypt->ops->init();
		if (!new_crypt->ops || !new_crypt->priv) {
			kfree(new_crypt);
			new_crypt = NULL;

			printk(KERN_WARNING "%s: could not initialize WEP: "
			       "load module hostap_crypt_wep.o\n",
			       dev->name);
			return -EOPNOTSUPP;
		}
		first = 1;
		local->crypt = new_crypt;
	}

	i = erq->flags & IW_ENCODE_INDEX;
	if (i < 1 || i > 4)
		i = local->crypt->ops->get_key_idx(local->crypt->priv);
	else
		i--;
	if (i < 0 || i >= WEP_KEYS)
		return -EINVAL;

	if (erq->length > 0) {
		int len = erq->length <= 5 ? 5 : 13;
		if (len > erq->length)
			memset(keybuf + erq->length, 0, len - erq->length);
		local->crypt->ops->set_key(i, keybuf, len, local->crypt->priv);
		if (first)
			local->crypt->ops->set_key_idx(i, local->crypt->priv);
	} else {
		if (local->crypt->ops->set_key_idx(i, local->crypt->priv) < 0)
			return -EINVAL; /* keyidx not valid */
	}

 done:
	local->open_wep = erq->flags & IW_ENCODE_OPEN;

	if (hostap_set_encryption(local)) {
		printk(KERN_DEBUG "%s: set_encryption failed\n", dev->name);
		return -EINVAL;
	}

	/* Do not reset port0 if card is in Managed mode since resetting will
	 * generate new IEEE 802.11 authentication which may end up in looping
	 * with IEEE 802.1X. Prism2 documentation seem to require port reset
	 * after WEP configuration. However, keys are apparently changed at
	 * least in Managed mode. */
	if (local->iw_mode != IW_MODE_INFRA && local->func->reset_port(dev)) {
		printk(KERN_DEBUG "%s: reset_port failed\n", dev->name);
		return -EINVAL;
	}

	return 0;
}


static int prism2_ioctl_giwencode(struct net_device *dev,
				  struct iw_request_info *info,
				  struct iw_point *erq, char *key)
{
	local_info_t *local = (local_info_t *) dev->priv;
	int i, len;
	u16 val;

	if (local->crypt == NULL || local->crypt->ops == NULL) {
		erq->length = 0;
		erq->flags = IW_ENCODE_DISABLED;
		return 0;
	}

	if (strcmp(local->crypt->ops->name, "WEP") != 0) {
		/* only WEP is supported with wireless extensions, so just
		 * report that encryption is used */
		erq->length = 0;
		erq->flags = IW_ENCODE_ENABLED;
		return 0;
	}

	i = erq->flags & IW_ENCODE_INDEX;
	if (i < 1 || i > 4)
		i = local->crypt->ops->get_key_idx(local->crypt->priv);
	else
		i--;
	if (i < 0 || i >= WEP_KEYS)
		return -EINVAL;

	erq->flags = i + 1;

	/* Reads from HFA384X_RID_CNFDEFAULTKEY* return bogus values, so show
	 * the keys from driver buffer */
	len = local->crypt->ops->get_key(i, key, WEP_KEY_LEN,
					 local->crypt->priv);
	erq->length = (len >= 0 ? len : 0);

	if (local->func->get_rid(dev, HFA384X_RID_CNFWEPFLAGS, &val, 2, 1) < 0)
	{
		printk("CNFWEPFLAGS reading failed\n");
		return -EOPNOTSUPP;
	}
	le16_to_cpus(&val);
	if (val & HFA384X_WEPFLAGS_PRIVACYINVOKED)
		erq->flags |= IW_ENCODE_ENABLED;
	else
		erq->flags |= IW_ENCODE_DISABLED;
	if (val & HFA384X_WEPFLAGS_EXCLUDEUNENCRYPTED)
		erq->flags |= IW_ENCODE_RESTRICTED;
	else
		erq->flags |= IW_ENCODE_OPEN;

	return 0;
}


#if WIRELESS_EXT <= 15
static int prism2_ioctl_giwspy(struct net_device *dev,
			       struct iw_request_info *info,
			       struct iw_point *srq, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	struct sockaddr addr[IW_MAX_SPY];
	struct iw_quality qual[IW_MAX_SPY];

	if (local->iw_mode != IW_MODE_MASTER) {
		printk("SIOCGIWSPY is currently only supported in Host AP "
		       "mode\n");
		srq->length = 0;
		return -EOPNOTSUPP;
	}

	srq->length = prism2_ap_get_sta_qual(local, addr, qual, IW_MAX_SPY, 0);

	memcpy(extra, &addr, sizeof(addr[0]) * srq->length);
	memcpy(extra + sizeof(addr[0]) * srq->length, &qual,
	       sizeof(qual[0]) * srq->length);

	return 0;
}
#endif /* WIRELESS_EXT <= 15 */


static int hostap_set_rate(struct net_device *dev)
{
	local_info_t *local = (local_info_t *) dev->priv;
	int ret;

	ret = (hostap_set_word(dev, HFA384X_RID_TXRATECONTROL,
			       local->tx_rate_control) ||
	       hostap_set_word(dev, HFA384X_RID_CNFSUPPORTEDRATES,
			       local->tx_rate_control) ||
	       local->func->reset_port(dev));
		
	if (ret) {
		printk(KERN_WARNING "%s: TXRateControl/cnfSupportedRates "
		       "setting to 0x%x failed\n",
		       dev->name, local->tx_rate_control);
	}

	/* Update TX rate configuration for all STAs based on new operational
	 * rate set. */
	hostap_update_rates(local);

	return ret;
}


static int prism2_ioctl_siwrate(struct net_device *dev,
				struct iw_request_info *info,
				struct iw_param *rrq, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;

	if (rrq->fixed) {
		switch (rrq->value) {
		case 11000000:
			local->tx_rate_control = HFA384X_RATES_11MBPS;
			break;
		case 5500000:
			local->tx_rate_control = HFA384X_RATES_5MBPS;
			break;
		case 2000000:
			local->tx_rate_control = HFA384X_RATES_2MBPS;
			break;
		case 1000000:
			local->tx_rate_control = HFA384X_RATES_1MBPS;
			break;
		default:
			local->tx_rate_control = HFA384X_RATES_1MBPS |
				HFA384X_RATES_2MBPS | HFA384X_RATES_5MBPS |
				HFA384X_RATES_11MBPS;
			break;
		}
	} else {
		switch (rrq->value) {
		case 11000000:
			local->tx_rate_control = HFA384X_RATES_1MBPS |
				HFA384X_RATES_2MBPS | HFA384X_RATES_5MBPS |
				HFA384X_RATES_11MBPS;
			break;
		case 5500000:
			local->tx_rate_control = HFA384X_RATES_1MBPS |
				HFA384X_RATES_2MBPS | HFA384X_RATES_5MBPS;
			break;
		case 2000000:
			local->tx_rate_control = HFA384X_RATES_1MBPS |
				HFA384X_RATES_2MBPS;
			break;
		case 1000000:
			local->tx_rate_control = HFA384X_RATES_1MBPS;
			break;
		default:
			local->tx_rate_control = HFA384X_RATES_1MBPS |
				HFA384X_RATES_2MBPS | HFA384X_RATES_5MBPS |
				HFA384X_RATES_11MBPS;
			break;
		}
	}

	return hostap_set_rate(dev);
}


static int prism2_ioctl_giwrate(struct net_device *dev,
				struct iw_request_info *info,
				struct iw_param *rrq, char *extra)
{
	u16 val;
	local_info_t *local = (local_info_t *) dev->priv;
	int ret = 0;

	if (local->func->get_rid(dev, HFA384X_RID_TXRATECONTROL, &val, 2, 1) <
	    0)
		return -EINVAL;

	if ((val & 0x1) && (val > 1))
		rrq->fixed = 0;
	else
		rrq->fixed = 1;

	if (local->iw_mode == IW_MODE_MASTER && local->ap != NULL &&
	    !local->fw_tx_rate_control) {
		/* HFA384X_RID_CURRENTTXRATE seems to always be 2 Mbps in
		 * Host AP mode, so use the recorded TX rate of the last sent
		 * frame */
		rrq->value = local->ap->last_tx_rate > 0 ?
			local->ap->last_tx_rate * 100000 : 11000000;
		return 0;
	}

	if (local->func->get_rid(dev, HFA384X_RID_CURRENTTXRATE, &val, 2, 1) <
	    0)
		return -EINVAL;

	switch (val) {
	case HFA384X_RATES_1MBPS:
		rrq->value = 1000000;
		break;
	case HFA384X_RATES_2MBPS:
		rrq->value = 2000000;
		break;
	case HFA384X_RATES_5MBPS:
		rrq->value = 5500000;
		break;
	case HFA384X_RATES_11MBPS:
		rrq->value = 11000000;
		break;
	default:
		/* should not happen */
		rrq->value = 11000000;
		ret = -EINVAL;
		break;
	}

	return ret;
}


static int prism2_ioctl_siwsens(struct net_device *dev,
				struct iw_request_info *info,
				struct iw_param *sens, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;

	/* Set the desired AP density */
	if (sens->value < 1 || sens->value > 3)
		return -EINVAL;

	if (hostap_set_word(dev, HFA384X_RID_CNFSYSTEMSCALE, sens->value) ||
	    local->func->reset_port(dev))
		return -EINVAL;

	return 0;
}

static int prism2_ioctl_giwsens(struct net_device *dev,
				struct iw_request_info *info,
				struct iw_param *sens, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	u16 val;

	/* Get the current AP density */
	if (local->func->get_rid(dev, HFA384X_RID_CNFSYSTEMSCALE, &val, 2, 1) <
	    0)
		return -EINVAL;

	sens->value = __le16_to_cpu(val);
	sens->fixed = 1;

	return 0;
}


/* Deprecated in new wireless extension API */
static int prism2_ioctl_giwaplist(struct net_device *dev,
				  struct iw_request_info *info,
				  struct iw_point *data, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	struct sockaddr addr[IW_MAX_AP];
	struct iw_quality qual[IW_MAX_AP];

	if (local->iw_mode != IW_MODE_MASTER) {
		printk(KERN_DEBUG "SIOCGIWAPLIST is currently only supported "
		       "in Host AP mode\n");
		data->length = 0;
		return -EOPNOTSUPP;
	}

	data->length = prism2_ap_get_sta_qual(local, addr, qual, IW_MAX_AP, 1);

	memcpy(extra, &addr, sizeof(addr[0]) * data->length);
	data->flags = 1; /* has quality information */
	memcpy(extra + sizeof(addr[0]) * data->length, &qual,
	       sizeof(qual[0]) * data->length);

	return 0;
}


static int prism2_ioctl_siwrts(struct net_device *dev,
			       struct iw_request_info *info,
			       struct iw_param *rts, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	u16 val;

	if (rts->disabled)
		val = __constant_cpu_to_le16(2347);
	else if (rts->value < 0 || rts->value > 2347)
		return -EINVAL;
	else
		val = __cpu_to_le16(rts->value);

	if (local->func->set_rid(dev, HFA384X_RID_RTSTHRESHOLD, &val, 2) ||
	    local->func->reset_port(dev))
		return -EINVAL;

	return 0;
}

static int prism2_ioctl_giwrts(struct net_device *dev,
			       struct iw_request_info *info,
			       struct iw_param *rts, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	u16 val;

	if (local->func->get_rid(dev, HFA384X_RID_RTSTHRESHOLD, &val, 2, 1) <
	    0)
		return -EINVAL;

	rts->value = __le16_to_cpu(val);
	rts->disabled = (rts->value == 2347);
	rts->fixed = 1;

	return 0;
}


static int prism2_ioctl_siwfrag(struct net_device *dev,
				struct iw_request_info *info,
				struct iw_param *rts, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	u16 val;

	if (rts->disabled)
		val = __constant_cpu_to_le16(2346);
	else if (rts->value < 256 || rts->value > 2346)
		return -EINVAL;
	else
		val = __cpu_to_le16(rts->value & ~0x1); /* even numbers only */

	if (local->func->set_rid(dev, HFA384X_RID_FRAGMENTATIONTHRESHOLD, &val,
				 2)
	    || local->func->reset_port(dev))
		return -EINVAL;

	return 0;
}

static int prism2_ioctl_giwfrag(struct net_device *dev,
				struct iw_request_info *info,
				struct iw_param *rts, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	u16 val;

	if (local->func->get_rid(dev, HFA384X_RID_FRAGMENTATIONTHRESHOLD,
				 &val, 2, 1) < 0)
		return -EINVAL;

	rts->value = __le16_to_cpu(val);
	rts->disabled = (rts->value == 2346);
	rts->fixed = 1;

	return 0;
}
#endif /* WIRELESS_EXT > 8 */


static int prism2_ioctl_siwap(struct net_device *dev,
			      struct iw_request_info *info,
			      struct sockaddr *ap_addr, char *extra)
{
#ifdef PRISM2_NO_STATION_MODES
	return -EOPNOTSUPP;
#else /* PRISM2_NO_STATION_MODES */
	local_info_t *local = (local_info_t *) dev->priv;

	memcpy(local->preferred_ap, &ap_addr->sa_data, ETH_ALEN);

	if (local->host_roaming && local->iw_mode == IW_MODE_INFRA) {
		struct hfa384x_scan_request scan_req;
		memset(&scan_req, 0, sizeof(scan_req));
		scan_req.channel_list = __constant_cpu_to_le16(0x3fff);
		scan_req.txrate = __constant_cpu_to_le16(HFA384X_RATES_1MBPS);
		if (local->func->set_rid(dev, HFA384X_RID_SCANREQUEST,
					 &scan_req, sizeof(scan_req))) {
			printk(KERN_DEBUG "%s: ScanResults request failed - "
			       "preferred AP delayed to next unsolicited "
			       "scan\n", dev->name);
		}
	} else {
		printk(KERN_DEBUG "%s: Preferred AP (SIOCSIWAP) is used only "
		       "in Managed mode when host_roaming is enabled\n",
		       dev->name);
	}

	return 0;
#endif /* PRISM2_NO_STATION_MODES */
}

static int prism2_ioctl_giwap(struct net_device *dev,
			      struct iw_request_info *info,
			      struct sockaddr *ap_addr, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;

	if (dev == local->stadev) {
		memcpy(&ap_addr->sa_data, local->assoc_ap_addr, ETH_ALEN);
		ap_addr->sa_family = ARPHRD_ETHER;
		return 0;
	}

	if (local->func->get_rid(dev, HFA384X_RID_CURRENTBSSID,
				 &ap_addr->sa_data, ETH_ALEN, 1) < 0)
		return -EOPNOTSUPP;

	/* local->bssid is also updated in LinkStatus handler when in station
	 * mode */
	memcpy(local->bssid, &ap_addr->sa_data, ETH_ALEN);
	ap_addr->sa_family = ARPHRD_ETHER;

	return 0;
}


#if WIRELESS_EXT > 8
static int prism2_ioctl_siwnickn(struct net_device *dev,
				 struct iw_request_info *info,
				 struct iw_point *data, char *nickname)
{
	local_info_t *local = (local_info_t *) dev->priv;

	memset(local->name, 0, sizeof(local->name));
	memcpy(local->name, nickname, data->length);
	local->name_set = 1;

	if (hostap_set_string(dev, HFA384X_RID_CNFOWNNAME, local->name) ||
	    local->func->reset_port(dev))
		return -EINVAL;

	return 0;
}

static int prism2_ioctl_giwnickn(struct net_device *dev,
				 struct iw_request_info *info,
				 struct iw_point *data, char *nickname)
{
	local_info_t *local = (local_info_t *) dev->priv;
	int len;
	char name[MAX_NAME_LEN + 3];
	u16 val;

	len = local->func->get_rid(dev, HFA384X_RID_CNFOWNNAME,
				   &name, MAX_NAME_LEN + 2, 0);
	val = __le16_to_cpu(*(u16 *) name);
	if (len > MAX_NAME_LEN + 2 || len < 0 || val > MAX_NAME_LEN)
		return -EOPNOTSUPP;

	name[val + 2] = '\0';
	data->length = val + 1;
	memcpy(nickname, name + 2, val + 1);

	return 0;
}
#endif /* WIRELESS_EXT > 8 */


static int prism2_ioctl_siwfreq(struct net_device *dev,
				struct iw_request_info *info,
				struct iw_freq *freq, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;

	/* freq => chan. */
	if (freq->e == 1 &&
	    freq->m / 100000 >= freq_list[0] &&
	    freq->m / 100000 <= freq_list[FREQ_COUNT - 1]) {
		int ch;
		int fr = freq->m / 100000;
		for (ch = 0; ch < FREQ_COUNT; ch++) {
			if (fr == freq_list[ch]) {
				freq->e = 0;
				freq->m = ch + 1;
				break;
			}
		}
	}

	if (freq->e != 0 || freq->m < 1 || freq->m > FREQ_COUNT ||
	    !(local->channel_mask & (1 << (freq->m - 1))))
		return -EINVAL;

	local->channel = freq->m; /* channel is used in prism2_setup_rids() */
	if (hostap_set_word(dev, HFA384X_RID_CNFOWNCHANNEL, local->channel) ||
	    local->func->reset_port(dev))
		return -EINVAL;

	return 0;
}

static int prism2_ioctl_giwfreq(struct net_device *dev,
				struct iw_request_info *info,
				struct iw_freq *freq, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	u16 val;

	if (local->func->get_rid(dev, HFA384X_RID_CURRENTCHANNEL, &val, 2, 1) <
	    0)
		return -EINVAL;

	le16_to_cpus(&val);
	if (val < 1 || val > FREQ_COUNT)
		return -EINVAL;

	freq->m = freq_list[val - 1] * 100000;
	freq->e = 1;

	return 0;
}


static void hostap_monitor_set_type(local_info_t *local)
{
	struct net_device *dev = local->dev;

	if (local->monitor_type == PRISM2_MONITOR_PRISM ||
	    local->monitor_type == PRISM2_MONITOR_CAPHDR) {
		dev->type = ARPHRD_IEEE80211_PRISM;
		dev->hard_header_parse =
			hostap_80211_prism_header_parse;
	} else {
		dev->type = ARPHRD_IEEE80211;
		dev->hard_header_parse = hostap_80211_header_parse;
	}
}


#if WIRELESS_EXT > 8
static int prism2_ioctl_siwessid(struct net_device *dev,
				 struct iw_request_info *info,
				 struct iw_point *data, char *ssid)
{
	local_info_t *local = (local_info_t *) dev->priv;

	if (data->flags == 0)
		ssid[0] = '\0'; /* ANY */

	if (local->iw_mode == IW_MODE_MASTER && ssid[0] == '\0') {
		/* Setting SSID to empty string seems to kill the card in
		 * Host AP mode */
		printk(KERN_DEBUG "%s: Host AP mode does not support "
		       "'Any' essid\n", dev->name);
		return -EINVAL;
	}

	memcpy(local->essid, ssid, IW_ESSID_MAX_SIZE);
	local->essid[MAX_SSID_LEN] = '\0';

	if ((!local->fw_ap &&
	     hostap_set_string(dev, HFA384X_RID_CNFDESIREDSSID, local->essid))
	    || hostap_set_string(dev, HFA384X_RID_CNFOWNSSID, local->essid) ||
	    local->func->reset_port(dev))
		return -EINVAL;

	return 0;
}
static int prism2_ioctl_giwessid(struct net_device *dev,
				 struct iw_request_info *info,
				 struct iw_point *data, char *essid)
{

	local_info_t *local = (local_info_t *) dev->priv;
	u16 val;

	data->flags = 1; // active 
	if (local->iw_mode == IW_MODE_MASTER) {
		data->length = strlen(local->essid);
		memcpy(essid, local->essid, IW_ESSID_MAX_SIZE);
	} else {
		int len;
		char ssid[MAX_SSID_LEN + 2];
		memset(ssid, 0, sizeof(ssid));
		len = local->func->get_rid(dev, HFA384X_RID_CURRENTSSID,
					   &ssid, MAX_SSID_LEN + 2, 0);
		val = __le16_to_cpu(*(u16 *) ssid);
		if (len > MAX_SSID_LEN + 2 || len < 0 || val > MAX_SSID_LEN) {
			return -EOPNOTSUPP;
		}
		data->length = val;
		memcpy(essid, ssid + 2, IW_ESSID_MAX_SIZE);
	}
	return 0;
}


static int prism2_ioctl_giwrange(struct net_device *dev,
				 struct iw_request_info *info,
				 struct iw_point *data, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	struct iw_range *range = (struct iw_range *) extra;
	u8 rates[10];
	u16 val;
	int i, len, over2;

	data->length = sizeof(struct iw_range);
	memset(range, 0, sizeof(struct iw_range));

#if WIRELESS_EXT > 9
	/* TODO: could fill num_txpower and txpower array with
	 * something; however, there are 128 different values.. */

	range->txpower_capa = IW_TXPOW_DBM;

	if (local->iw_mode == IW_MODE_INFRA || local->iw_mode == IW_MODE_ADHOC)
	{
		range->min_pmp = 1 * 1024;
		range->max_pmp = 65535 * 1024;
		range->min_pmt = 1 * 1024;
		range->max_pmt = 1000 * 1024;
		range->pmp_flags = IW_POWER_PERIOD;
		range->pmt_flags = IW_POWER_TIMEOUT;
		range->pm_capa = IW_POWER_PERIOD | IW_POWER_TIMEOUT |
			IW_POWER_UNICAST_R | IW_POWER_ALL_R;
	}
#endif /* WIRELESS_EXT > 9 */

#if WIRELESS_EXT > 10
	range->we_version_compiled = WIRELESS_EXT;
	range->we_version_source = 13;

	range->retry_capa = IW_RETRY_LIMIT;
	range->retry_flags = IW_RETRY_LIMIT;
	range->min_retry = 0;
	range->max_retry = 255;
#endif /* WIRELESS_EXT > 10 */

	range->num_channels = FREQ_COUNT;

	val = 0;
	for (i = 0; i < FREQ_COUNT; i++) {
		if (local->channel_mask & (1 << i)) {
			range->freq[val].i = i + 1;
			range->freq[val].m = freq_list[i] * 100000;
			range->freq[val].e = 1;
			val++;
		}
		if (val == IW_MAX_FREQUENCIES)
			break;
	}
	range->num_frequency = val;

	range->max_qual.qual = 92; /* 0 .. 92 */
	range->max_qual.level = 154; /* 27 .. 154 */
	range->max_qual.noise = 154; /* 27 .. 154 */
	range->sensitivity = 3;

	range->max_encoding_tokens = WEP_KEYS;
	range->num_encoding_sizes = 2;
	range->encoding_size[0] = 5;
	range->encoding_size[1] = 13;

	over2 = 0;
	len = prism2_get_datarates(dev, rates);
	range->num_bitrates = 0;
	for (i = 0; i < len; i++) {
		if (range->num_bitrates < IW_MAX_BITRATES) {
			range->bitrate[range->num_bitrates] =
				rates[i] * 500000;
			range->num_bitrates++;
		}
		if (rates[i] == 0x0b || rates[i] == 0x16)
			over2 = 1;
	}
	/* estimated maximum TCP throughput values (bps) */
	range->throughput = over2 ? 5500000 : 1500000;

	range->min_rts = 0;
	range->max_rts = 2347;
	range->min_frag = 256;
	range->max_frag = 2346;

	return 0;
}


static int hostap_monitor_mode_enable(local_info_t *local)
{
	struct net_device *dev = local->dev;

	printk(KERN_DEBUG "Enabling monitor mode\n");
	hostap_monitor_set_type(local);

	if (hostap_set_word(dev, HFA384X_RID_CNFPORTTYPE,
			    HFA384X_PORTTYPE_PSEUDO_IBSS)) {
		printk(KERN_DEBUG "Port type setting for monitor mode "
		       "failed\n");
		return -EOPNOTSUPP;
	}

	/* Host decrypt is needed to get the IV and ICV fields;
	 * however, monitor mode seems to remove WEP flag from frame
	 * control field */
	if (hostap_set_word(dev, HFA384X_RID_CNFWEPFLAGS,
			    HFA384X_WEPFLAGS_HOSTENCRYPT |
			    HFA384X_WEPFLAGS_HOSTDECRYPT)) {
		printk(KERN_DEBUG "WEP flags setting failed\n");
		return -EOPNOTSUPP;
	}

	if (local->func->reset_port(dev) ||
	    local->func->cmd(dev, HFA384X_CMDCODE_TEST |
			     (HFA384X_TEST_MONITOR << 8),
			     0, NULL, NULL)) {
		printk(KERN_DEBUG "Setting monitor mode failed\n");
		return -EOPNOTSUPP;
	}

	return 0;
}


static int hostap_monitor_mode_disable(local_info_t *local)
{
	struct net_device *dev = local->dev;

	printk(KERN_DEBUG "%s: Disabling monitor mode\n", dev->name);
	dev->type = ARPHRD_ETHER;
	dev->hard_header_parse = local->saved_eth_header_parse;
	if (local->func->cmd(dev, HFA384X_CMDCODE_TEST |
			     (HFA384X_TEST_STOP << 8),
			     0, NULL, NULL))
		return -1;
	return hostap_set_encryption(local);
}


static int prism2_ioctl_siwmode(struct net_device *dev,
				struct iw_request_info *info,
				__u32 *mode, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	int double_reset = 0;

	if (*mode != IW_MODE_ADHOC && *mode != IW_MODE_INFRA &&
	    *mode != IW_MODE_MASTER && *mode != IW_MODE_REPEAT &&
	    *mode != IW_MODE_MONITOR)
		return -EOPNOTSUPP;

#ifdef PRISM2_NO_STATION_MODES
	if (*mode == IW_MODE_ADHOC || *mode == IW_MODE_INFRA)
		return -EOPNOTSUPP;
#endif /* PRISM2_NO_STATION_MODES */

	if (*mode == local->iw_mode)
		return 0;

	if (*mode == IW_MODE_MASTER && local->essid[0] == '\0') {
		printk(KERN_WARNING "%s: empty SSID not allowed in Master "
		       "mode\n", dev->name);
		return -EINVAL;
	}

	if (local->iw_mode == IW_MODE_MONITOR)
		hostap_monitor_mode_disable(local);

	if (local->iw_mode == IW_MODE_ADHOC && *mode == IW_MODE_MASTER) {
		/* There seems to be a firmware bug in at least STA f/w v1.5.6
		 * that leaves beacon frames to use IBSS type when moving from
		 * IBSS to Host AP mode. Doing double Port0 reset seems to be
		 * enough to workaround this. */
		double_reset = 1;
	}

	printk("prism2: %s: operating mode changed "
	       "%d -> %d\n\n", dev->name, local->iw_mode, *mode);
	local->iw_mode = *mode;

	if (local->iw_mode == IW_MODE_MONITOR)
		hostap_monitor_mode_enable(local);
	else if (local->iw_mode == IW_MODE_MASTER && !local->host_encrypt &&
		 !local->fw_encrypt_ok) {
		printk(KERN_DEBUG "%s: defaulting to host-based encryption as "
		       "a workaround for firmware bug in Host AP mode WEP\n",
		       dev->name);
		local->host_encrypt = 1;
	}

	if (hostap_set_word(dev, HFA384X_RID_CNFPORTTYPE,
			    hostap_get_porttype(local)))
		return -EOPNOTSUPP;

	if (local->func->reset_port(dev))
		return -EINVAL;
	if (double_reset && local->func->reset_port(dev))
		return -EINVAL;

	return 0;
}


static int prism2_ioctl_giwmode(struct net_device *dev,
				struct iw_request_info *info,
				__u32 *mode, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;

	if (dev == local->stadev) {
		*mode = IW_MODE_INFRA;
		return 0;
	}

	*mode = local->iw_mode;
	return 0;
}


static int prism2_ioctl_siwpower(struct net_device *dev,
				 struct iw_request_info *info,
				 struct iw_param *wrq, char *extra)
{
#ifdef PRISM2_NO_STATION_MODES
	return -EOPNOTSUPP;
#else /* PRISM2_NO_STATION_MODES */
	int ret = 0;

	if (wrq->disabled)
		return hostap_set_word(dev, HFA384X_RID_CNFPMENABLED, 0);

	switch (wrq->flags & IW_POWER_MODE) {
	case IW_POWER_UNICAST_R:
		ret = hostap_set_word(dev, HFA384X_RID_CNFMULTICASTRECEIVE, 0);
		if (ret)
			return ret;
		ret = hostap_set_word(dev, HFA384X_RID_CNFPMENABLED, 1);
		if (ret)
			return ret;
		break;
	case IW_POWER_ALL_R:
		ret = hostap_set_word(dev, HFA384X_RID_CNFMULTICASTRECEIVE, 1);
		if (ret)
			return ret;
		ret = hostap_set_word(dev, HFA384X_RID_CNFPMENABLED, 1);
		if (ret)
			return ret;
		break;
	case IW_POWER_ON:
		break;
	default:
		return -EINVAL;
	}

	if (wrq->flags & IW_POWER_TIMEOUT) {
		ret = hostap_set_word(dev, HFA384X_RID_CNFPMENABLED, 1);
		if (ret)
			return ret;
		ret = hostap_set_word(dev, HFA384X_RID_CNFPMHOLDOVERDURATION,
				      wrq->value / 1024);
		if (ret)
			return ret;
	}
	if (wrq->flags & IW_POWER_PERIOD) {
		ret = hostap_set_word(dev, HFA384X_RID_CNFPMENABLED, 1);
		if (ret)
			return ret;
		ret = hostap_set_word(dev, HFA384X_RID_CNFMAXSLEEPDURATION,
				      wrq->value / 1024);
		if (ret)
			return ret;
	}

	return ret;
#endif /* PRISM2_NO_STATION_MODES */
}


static int prism2_ioctl_giwpower(struct net_device *dev,
				 struct iw_request_info *info,
				 struct iw_param *rrq, char *extra)
{
#ifdef PRISM2_NO_STATION_MODES
	return -EOPNOTSUPP;
#else /* PRISM2_NO_STATION_MODES */
	local_info_t *local = (local_info_t *) dev->priv;
	u16 enable, mcast;

	if (local->func->get_rid(dev, HFA384X_RID_CNFPMENABLED, &enable, 2, 1)
	    < 0)
		return -EINVAL;

	if (!__le16_to_cpu(enable)) {
		rrq->disabled = 1;
		return 0;
	}

	rrq->disabled = 0;

	if ((rrq->flags & IW_POWER_TYPE) == IW_POWER_TIMEOUT) {
		u16 timeout;
		if (local->func->get_rid(dev,
					 HFA384X_RID_CNFPMHOLDOVERDURATION,
					 &timeout, 2, 1) < 0)
			return -EINVAL;

		rrq->flags = IW_POWER_TIMEOUT;
		rrq->value = __le16_to_cpu(timeout) * 1024;
	} else {
		u16 period;
		if (local->func->get_rid(dev, HFA384X_RID_CNFMAXSLEEPDURATION,
					 &period, 2, 1) < 0)
			return -EINVAL;

		rrq->flags = IW_POWER_PERIOD;
		rrq->value = __le16_to_cpu(period) * 1024;
	}

	if (local->func->get_rid(dev, HFA384X_RID_CNFMULTICASTRECEIVE, &mcast,
				 2, 1) < 0)
		return -EINVAL;

	if (__le16_to_cpu(mcast))
		rrq->flags |= IW_POWER_ALL_R;
	else
		rrq->flags |= IW_POWER_UNICAST_R;

	return 0;
#endif /* PRISM2_NO_STATION_MODES */
}
#endif /* WIRELESS_EXT > 8 */


#if WIRELESS_EXT > 10
static int prism2_ioctl_siwretry(struct net_device *dev,
				 struct iw_request_info *info,
				 struct iw_param *rrq, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;

	if (rrq->disabled)
		return -EINVAL;

	/* setting retry limits is not supported with the current station
	 * firmware code; simulate this with alternative retry count for now */
	if (rrq->flags == IW_RETRY_LIMIT) {
		if (rrq->value < 0) {
			/* disable manual retry count setting and use firmware
			 * defaults */
			local->manual_retry_count = -1;
			local->tx_control &= ~HFA384X_TX_CTRL_ALT_RTRY;
		} else {
			if (hostap_set_word(dev, HFA384X_RID_CNFALTRETRYCOUNT,
					    rrq->value)) {
				printk(KERN_DEBUG "%s: Alternate retry count "
				       "setting to %d failed\n",
				       dev->name, rrq->value);
				return -EOPNOTSUPP;
			}

			local->manual_retry_count = rrq->value;
			local->tx_control |= HFA384X_TX_CTRL_ALT_RTRY;
		}
		return 0;
	}

	return -EOPNOTSUPP;

#if 0
	/* what could be done, if firmware would support this.. */

	if (rrq->flags & IW_RETRY_LIMIT) {
		if (rrq->flags & IW_RETRY_MAX)
			HFA384X_RID_LONGRETRYLIMIT = rrq->value;
		else if (rrq->flags & IW_RETRY_MIN)
			HFA384X_RID_SHORTRETRYLIMIT = rrq->value;
		else {
			HFA384X_RID_LONGRETRYLIMIT = rrq->value;
			HFA384X_RID_SHORTRETRYLIMIT = rrq->value;
		}

	}

	if (rrq->flags & IW_RETRY_LIFETIME) {
		HFA384X_RID_MAXTRANSMITLIFETIME = rrq->value / 1024;
	}

	return 0;
#endif /* 0 */
}

static int prism2_ioctl_giwretry(struct net_device *dev,
				 struct iw_request_info *info,
				 struct iw_param *rrq, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	u16 shortretry, longretry, lifetime;

	if (local->func->get_rid(dev, HFA384X_RID_SHORTRETRYLIMIT, &shortretry,
				 2, 1) < 0 ||
	    local->func->get_rid(dev, HFA384X_RID_LONGRETRYLIMIT, &longretry,
				 2, 1) < 0 ||
	    local->func->get_rid(dev, HFA384X_RID_MAXTRANSMITLIFETIME,
				 &lifetime, 2, 1) < 0)
		return -EINVAL;

	le16_to_cpus(&shortretry);
	le16_to_cpus(&longretry);
	le16_to_cpus(&lifetime);

	rrq->disabled = 0;

	if ((rrq->flags & IW_RETRY_TYPE) == IW_RETRY_LIFETIME) {
		rrq->flags = IW_RETRY_LIFETIME;
		rrq->value = lifetime * 1024;
	} else {
		if (local->manual_retry_count >= 0) {
			rrq->flags = IW_RETRY_LIMIT;
			rrq->value = local->manual_retry_count;
		} else if ((rrq->flags & IW_RETRY_MAX)) {
			rrq->flags = IW_RETRY_LIMIT | IW_RETRY_MAX;
			rrq->value = longretry;
		} else {
			rrq->flags = IW_RETRY_LIMIT;
			rrq->value = shortretry;
			if (shortretry != longretry)
				rrq->flags |= IW_RETRY_MIN;
		}
	}
	return 0;
}
#endif /* WIRELESS_EXT > 10 */


#if WIRELESS_EXT > 9
/* Map HFA386x's CR31 to and from dBm with some sort of ad hoc mapping..
 * This version assumes following mapping:
 * CR31 is 7-bit value with -64 to +63 range.
 * -64 is mapped into +20dBm and +63 into -43dBm.
 * This is certainly not an exact mapping for every card, but at least
 * increasing dBm value should correspond to increasing TX power.
 */

static int prism2_txpower_hfa386x_to_dBm(u16 val)
{
	signed char tmp;

	if (val > 255)
		val = 255;

	tmp = val;
	tmp >>= 2;

	return -12 - tmp;
}

static u16 prism2_txpower_dBm_to_hfa386x(int val)
{
	signed char tmp;

	if (val > 20)
		return 128;
	else if (val < -43)
		return 127;

	tmp = val;
	tmp = -12 - tmp;
	tmp <<= 2;

	return (unsigned char) tmp;
}


static int prism2_ioctl_siwtxpow(struct net_device *dev,
				 struct iw_request_info *info,
				 struct iw_param *rrq, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	char *tmp;
	u16 val;
	int ret = 0;

	if (rrq->disabled) {
		if (local->txpower_type != PRISM2_TXPOWER_OFF) {
			val = 0xff; /* use all standby and sleep modes */
			ret = local->func->cmd(dev, HFA384X_CMDCODE_WRITEMIF,
					       HFA386X_CR_A_D_TEST_MODES2,
					       &val, NULL);
			printk(KERN_DEBUG "%s: Turning radio off: %s\n",
			       dev->name, ret ? "failed" : "OK");
			local->txpower_type = PRISM2_TXPOWER_OFF;
		}
		return (ret ? -EOPNOTSUPP : 0);
	}

	if (local->txpower_type == PRISM2_TXPOWER_OFF) {
		val = 0; /* disable all standby and sleep modes */
		ret = local->func->cmd(dev, HFA384X_CMDCODE_WRITEMIF,
				       HFA386X_CR_A_D_TEST_MODES2, &val, NULL);
		printk(KERN_DEBUG "%s: Turning radio on: %s\n",
		       dev->name, ret ? "failed" : "OK");
		local->txpower_type = PRISM2_TXPOWER_UNKNOWN;
	}

	if (!rrq->fixed && local->txpower_type != PRISM2_TXPOWER_AUTO) {
		printk(KERN_DEBUG "Setting ALC on\n");
		val = HFA384X_TEST_CFG_BIT_ALC;
		local->func->cmd(dev, HFA384X_CMDCODE_TEST |
				 (HFA384X_TEST_CFG_BITS << 8), 1, &val, NULL);
		local->txpower_type = PRISM2_TXPOWER_AUTO;
		return 0;
	}

	if (local->txpower_type != PRISM2_TXPOWER_FIXED) {
		printk(KERN_DEBUG "Setting ALC off\n");
		val = HFA384X_TEST_CFG_BIT_ALC;
		local->func->cmd(dev, HFA384X_CMDCODE_TEST |
				 (HFA384X_TEST_CFG_BITS << 8), 0, &val, NULL);
			local->txpower_type = PRISM2_TXPOWER_FIXED;
	}

	if (rrq->flags == IW_TXPOW_DBM)
		tmp = "dBm";
	else if (rrq->flags == IW_TXPOW_MWATT)
		tmp = "mW";
	else
		tmp = "UNKNOWN";
	printk(KERN_DEBUG "Setting TX power to %d %s\n", rrq->value, tmp);

	if (rrq->flags != IW_TXPOW_DBM) {
		printk("SIOCSIWTXPOW with mW is not supported; use dBm\n");
		return -EOPNOTSUPP;
	}

	local->txpower = rrq->value;
	val = prism2_txpower_dBm_to_hfa386x(local->txpower);
	if (local->func->cmd(dev, HFA384X_CMDCODE_WRITEMIF,
			     HFA386X_CR_MANUAL_TX_POWER, &val, NULL))
		ret = -EOPNOTSUPP;

	return ret;
}

static int prism2_ioctl_giwtxpow(struct net_device *dev,
				 struct iw_request_info *info,
				 struct iw_param *rrq, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	u16 resp0;

	rrq->flags = IW_TXPOW_DBM;
	rrq->disabled = 0;
	rrq->fixed = 0;

	if (local->txpower_type == PRISM2_TXPOWER_AUTO) {
		if (local->func->cmd(dev, HFA384X_CMDCODE_READMIF,
				     HFA386X_CR_MANUAL_TX_POWER,
				     NULL, &resp0) == 0) {
			rrq->value = prism2_txpower_hfa386x_to_dBm(resp0);
		} else {
			/* Could not get real txpower; guess 15 dBm */
			rrq->value = 15;
		}
	} else if (local->txpower_type == PRISM2_TXPOWER_OFF) {
		rrq->value = 0;
		rrq->disabled = 1;
	} else if (local->txpower_type == PRISM2_TXPOWER_FIXED) {
		rrq->value = local->txpower;
		rrq->fixed = 1;
	} else {
		printk("SIOCGIWTXPOW - unknown txpower_type=%d\n",
		       local->txpower_type);
	}
	return 0;
}
#endif /* WIRELESS_EXT > 9 */


#if WIRELESS_EXT > 13
static int prism2_ioctl_siwscan(struct net_device *dev,
				struct iw_request_info *info,
				struct iw_point *data, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
#ifndef PRISM2_NO_STATION_MODES
	struct hfa384x_scan_request scan_req;
	int ret = 0;
#endif /* !PRISM2_NO_STATION_MODES */
// test point
	if (local->iw_mode == IW_MODE_MASTER) {
		/// In master mode, we just return the results of our local
		//  tables, so we don't need to start anything...
		// Jean II 
		data->length = 0;
		return 0;
	}
#ifdef PRISM2_NO_STATION_MODES
	return -EOPNOTSUPP;
#else /* PRISM2_NO_STATION_MODES */

	memset(&scan_req, 0, sizeof(scan_req));
	scan_req.channel_list = __constant_cpu_to_le16(0x3fff);
	scan_req.txrate = __constant_cpu_to_le16(HFA384X_RATES_1MBPS);

	/* FIX:
	 * It seems to be enough to set roaming mode for a short moment to
	 * host-based and then setup scanrequest data and return the mode to
	 * firmware-based.
	 *
	 * Master mode would need to drop to Managed mode for a short while
	 * to make scanning work.. Or sweep through the different channels and
	 * use passive scan based on beacons. */

	if (!local->host_roaming)
		hostap_set_word(dev, HFA384X_RID_CNFROAMINGMODE,
				HFA384X_ROAMING_HOST);

	if (local->func->set_rid(dev, HFA384X_RID_SCANREQUEST, &scan_req,
				 sizeof(scan_req))) {
		printk("SCANREQUEST failed\n");
		ret = -EINVAL;
	}

	if (!local->host_roaming)
		hostap_set_word(dev, HFA384X_RID_CNFROAMINGMODE,
				HFA384X_ROAMING_FIRMWARE);

	local->scan_timestamp = jiffies;

#if 0
	/* NOTE! longer hostscan_request struct! Additional ssid field (empty
	 * means any). HostScan would be better than "old scan" since it keeps
	 * old association status. However, it requires newer station firmware
	 * (1.3.x?). */
	if (local->func->set_rid(dev, HFA384X_RID_HOSTSCAN, &scan_req,
				 sizeof(scan_req))) {
		printk(KERN_DEBUG "HOSTSCAN failed\n");
		return -EINVAL;
	}
#endif

	/* inquire F101, F103 or wait for SIOCGIWSCAN and read RID */

	return ret;
#endif /* PRISM2_NO_STATION_MODES */
}
/* ryc++
 * inserted teh following functions because of auto-channel-setting 
 */
double
roh_iw_freq2float(iwfreq *in)
{
#if 1
      /* Version without libm : slower */
      int       i;
        double    res = (double) in->m;
          for(i = 0; i < in->e; i++)
                  res *= 10;
            return(res);
#else   /* WE_NOLIBM */
              /* Version with libm : faster */
//            return ((double) in->m) * pow(10,in->e);
#endif  /* WE_NOLIBM */
}
#define KILO    1e3
#define MEGA    1e6
#define GIGA    1e9
void
roh_iw_print_freq(double    freq)
{
  if(freq < KILO)
    printk("Channel:%x\n", freq);
  else
    {
      if(freq >= GIGA)
    printk("Frequency:%xGHz\n", freq / GIGA);
      else
    {
      if(freq >= MEGA)
        printk("Frequency:%xMHz\n", freq / MEGA);
      else
        printk("Frequency:%xkHz\n", freq / KILO);
    }
    }
}
int
roh_iw_freq_to_channel(double       freq,
           struct iw_range *    range)
{
  double    ref_freq;
  int       k;

  /* Check if it's a frequency or not already a channel */
  if(freq < KILO){
    printk("fail-1\n");
    return(-1);
  }

  /* We compare the frequencies as double to ignore differences
   * in encoding. Slower, but safer... */
  for(k = 0; k < range->num_frequency; k++)
    {
      //printk("range->freq[%d] = %x\n",k,range->freq[k]); //ryc++
      ref_freq = roh_iw_freq2float(&(range->freq[k]));
      if(freq == ref_freq){
        return(range->freq[k].i);
        }
    }
  /* Not found */
  return(-2);
}

//++ryc------------


#ifndef PRISM2_NO_STATION_MODES
/* Translate scan data returned from the card to a card independant
 * format that the Wireless Tools will understand - Jean II */
static inline int sec_translate_scan(struct net_device *dev, char *buffer,
					char *scan, int scan_len)
{
	struct hfa384x_scan_result *atom;
	int left, i;
	struct iw_event iwe;
	char *current_ev = buffer;
	char *end_buf = buffer + IW_SCAN_MAX_DATA;
	char *current_val;
//ryc++ ---------
    local_info_t *local = (local_info_t *) dev->priv;
    struct iw_range roh_range;
    u16 val;
    int roh_count, roh_channel, loop_num, min_count;
	double roh_freq;
	int	ch_range_count[14]={0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int selected_ch = 1, ret = 0;
	u32 mode;

    memset(&roh_range, 0, sizeof(struct iw_range));
//++ryc ---------

	left = scan_len;

	if (left % sizeof(*atom) != 0) {
		printk(KERN_DEBUG "%s: invalid total scan result length %d "
		       "(entry length %d)\n",
		       dev->name, left, sizeof(*atom));
		return -EINVAL;
	}

	atom = (struct hfa384x_scan_result *) scan;
	while (left > 0) {
		u16 capabilities;

		/* First entry *MUST* be the AP MAC address */
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWAP;
		iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
		memcpy(iwe.u.ap_addr.sa_data, atom->bssid, ETH_ALEN);
		/* FIX:
		 * I do not know how this is possible, but iwe_stream_add_event
		 * seems to re-order memcpy execution so that len is set only
		 * after copying.. Pre-setting len here "fixes" this, but real
		 * problems should be solved (after which these iwe.len
		 * settings could be removed from this function). */
		iwe.len = IW_EV_ADDR_LEN;
		current_ev = iwe_stream_add_event(current_ev, end_buf, &iwe,
						  IW_EV_ADDR_LEN);

		/* Other entries will be displayed in the order we give them */

		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWESSID;
		iwe.u.data.length = le16_to_cpu(atom->ssid_len);
		if (iwe.u.data.length > 32)
			iwe.u.data.length = 32;
		iwe.u.data.flags = 1;
		iwe.len = IW_EV_POINT_LEN + iwe.u.data.length;
		current_ev = iwe_stream_add_point(current_ev, end_buf, &iwe,
						  atom->ssid);

		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWMODE;
		capabilities = le16_to_cpu(atom->capability);
		if (capabilities & (WLAN_CAPABILITY_ESS |
				    WLAN_CAPABILITY_IBSS)) {
			if (capabilities & WLAN_CAPABILITY_ESS)
				iwe.u.mode = IW_MODE_MASTER;
			else
				iwe.u.mode = IW_MODE_ADHOC;
			iwe.len = IW_EV_UINT_LEN;
			current_ev = iwe_stream_add_event(current_ev, end_buf,
							  &iwe,
							  IW_EV_UINT_LEN);
		}

		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWFREQ;
		iwe.u.freq.m = freq_list[le16_to_cpu(atom->chid) - 1] * 100000;
		iwe.u.freq.e = 1;
		iwe.len = IW_EV_FREQ_LEN;

//ryc++---------
/* scan frequency of neighbor access points and then gather the informations*/
        val = 0;
        for (roh_count = 0; roh_count < FREQ_COUNT; roh_count++) {
            if (local->channel_mask & (1 << roh_count)) {
                roh_range.freq[val].i = roh_count + 1;
                roh_range.freq[val].m = freq_list[roh_count] * 100000;
                roh_range.freq[val].e = 1;
                val++;
            }
            if (val == IW_MAX_FREQUENCIES)
                break;
        }
        roh_range.num_frequency = val;

        roh_freq = roh_iw_freq2float(&(iwe.u.freq));
        roh_channel = roh_iw_freq_to_channel(roh_freq, &roh_range);
        printk("Scaned neighbor AP channel = %d\n",roh_channel);

		if(roh_channel  >= 1 && roh_channel < 5){
			ch_range_count[1]++;
		}
		else if(roh_channel  >= 5 && roh_channel < 9){
			ch_range_count[5]++;
		}
		else if(roh_channel  >= 9 && roh_channel < 13){
			ch_range_count[9]++;
		}
		else{
			ch_range_count[13]++;
		}

//++ryc---------

		current_ev = iwe_stream_add_event(current_ev, end_buf, &iwe,
						  IW_EV_FREQ_LEN);

		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = IWEVQUAL;
		iwe.u.qual.level = HFA384X_LEVEL_TO_dBm(le16_to_cpu(atom->sl));
		iwe.u.qual.noise =
			HFA384X_LEVEL_TO_dBm(le16_to_cpu(atom->anl));
		iwe.len = IW_EV_QUAL_LEN;
		current_ev = iwe_stream_add_event(current_ev, end_buf, &iwe,
						  IW_EV_QUAL_LEN);

		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWENCODE;
		if (capabilities & WLAN_CAPABILITY_PRIVACY)
			iwe.u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
		else
			iwe.u.data.flags = IW_ENCODE_DISABLED;
		iwe.u.data.length = 0;
		iwe.len = IW_EV_POINT_LEN + iwe.u.data.length;
		current_ev = iwe_stream_add_point(
			current_ev, end_buf, &iwe,
			atom->ssid /* memcpy 0 bytes */);

		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWRATE;
		current_val = current_ev + IW_EV_LCP_LEN;
		for (i = 0; i < sizeof(atom->sup_rates); i++) {
			if (atom->sup_rates[i] == 0)
				break;
			/* Bit rate given in 500 kb/s units (+ 0x80) */
			iwe.u.bitrate.value =
				((atom->sup_rates[i] & 0x7f) * 500000);
			current_val = iwe_stream_add_value(
				current_ev, current_val, end_buf, &iwe,
				IW_EV_PARAM_LEN);
		}
		/* Check if we added any event */
		if ((current_val - current_ev) > IW_EV_LCP_LEN)
			current_ev = current_val;

		/* Could add beacon_interval and rate (of the received
		 * ProbeResp) to scan results. */

		atom++;
		left -= sizeof(*atom);
	} /* while */

#if 0
	{
		u8 *pos = buffer;
		left = (current_ev - buffer);
		printk(KERN_DEBUG "IW SCAN (len=%d):", left);
		while (left > 0) {
			printk(" %02x", *pos++);
			left--;
		}
		printk("\n");
	}
#endif
	//ryc++-----------------------
		/* After scanning, change mode to master mode */
		mode = IW_MODE_MASTER;
		ret = prism2_ioctl_siwmode(dev, NULL, &mode, NULL);
		
		/* setting Auto-selected channel */
		for(loop_num = 1;loop_num < 13; loop_num += 4){
			if( ch_range_count[selected_ch] <= ch_range_count[loop_num + 4]){
				min_count = ch_range_count[loop_num];
			}else{
				min_count = ch_range_count[loop_num+4];
				selected_ch = loop_num + 4;;
			}
		}
		printk("Auto Selected channel is channel[%d]\n",selected_ch);
		/* set auto selected channel */
		local->channel = selected_ch;
		if (hostap_set_word(dev, HFA384X_RID_CNFOWNCHANNEL, local->channel) ||
		    local->func->reset_port(dev)){
       		printk("%s: Channel setting to %d failed\n",
               dev->name, local->channel);
		}
	//++ryc-----------------------

	return current_ev - buffer;
}
/* Translate scan data returned from the card to a card independant
 * format that the Wireless Tools will understand - Jean II */
static inline int prism2_translate_scan(struct net_device *dev, char *buffer,
					char *scan, int scan_len)
{
	struct hfa384x_scan_result *atom;
	int left, i;
	struct iw_event iwe;
	char *current_ev = buffer;
	char *end_buf = buffer + IW_SCAN_MAX_DATA;
	char *current_val;

	left = scan_len;

	if (left % sizeof(*atom) != 0) {
		printk(KERN_DEBUG "%s: invalid total scan result length %d "
		       "(entry length %d)\n",
		       dev->name, left, sizeof(*atom));
		return -EINVAL;
	}

	atom = (struct hfa384x_scan_result *) scan;
	while (left > 0) {
		u16 capabilities;

		/* First entry *MUST* be the AP MAC address */
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWAP;
		iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
		memcpy(iwe.u.ap_addr.sa_data, atom->bssid, ETH_ALEN);
		/* FIX:
		 * I do not know how this is possible, but iwe_stream_add_event
		 * seems to re-order memcpy execution so that len is set only
		 * after copying.. Pre-setting len here "fixes" this, but real
		 * problems should be solved (after which these iwe.len
		 * settings could be removed from this function). */
		iwe.len = IW_EV_ADDR_LEN;
		current_ev = iwe_stream_add_event(current_ev, end_buf, &iwe,
						  IW_EV_ADDR_LEN);

		/* Other entries will be displayed in the order we give them */

		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWESSID;
		iwe.u.data.length = le16_to_cpu(atom->ssid_len);
		if (iwe.u.data.length > 32)
			iwe.u.data.length = 32;
		iwe.u.data.flags = 1;
		iwe.len = IW_EV_POINT_LEN + iwe.u.data.length;
		current_ev = iwe_stream_add_point(current_ev, end_buf, &iwe,
						  atom->ssid);

		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWMODE;
		capabilities = le16_to_cpu(atom->capability);
		if (capabilities & (WLAN_CAPABILITY_ESS |
				    WLAN_CAPABILITY_IBSS)) {
			if (capabilities & WLAN_CAPABILITY_ESS)
				iwe.u.mode = IW_MODE_MASTER;
			else
				iwe.u.mode = IW_MODE_ADHOC;
			iwe.len = IW_EV_UINT_LEN;
			current_ev = iwe_stream_add_event(current_ev, end_buf,
							  &iwe,
							  IW_EV_UINT_LEN);
		}

		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWFREQ;
		iwe.u.freq.m = freq_list[le16_to_cpu(atom->chid) - 1] * 100000;
		iwe.u.freq.e = 1;
		iwe.len = IW_EV_FREQ_LEN;


		current_ev = iwe_stream_add_event(current_ev, end_buf, &iwe,
						  IW_EV_FREQ_LEN);

		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = IWEVQUAL;
		iwe.u.qual.level = HFA384X_LEVEL_TO_dBm(le16_to_cpu(atom->sl));
		iwe.u.qual.noise =
			HFA384X_LEVEL_TO_dBm(le16_to_cpu(atom->anl));
		iwe.len = IW_EV_QUAL_LEN;
		current_ev = iwe_stream_add_event(current_ev, end_buf, &iwe,
						  IW_EV_QUAL_LEN);

		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWENCODE;
		if (capabilities & WLAN_CAPABILITY_PRIVACY)
			iwe.u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
		else
			iwe.u.data.flags = IW_ENCODE_DISABLED;
		iwe.u.data.length = 0;
		iwe.len = IW_EV_POINT_LEN + iwe.u.data.length;
		current_ev = iwe_stream_add_point(
			current_ev, end_buf, &iwe,
			atom->ssid /* memcpy 0 bytes */);

		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWRATE;
		current_val = current_ev + IW_EV_LCP_LEN;
		for (i = 0; i < sizeof(atom->sup_rates); i++) {
			if (atom->sup_rates[i] == 0)
				break;
			/* Bit rate given in 500 kb/s units (+ 0x80) */
			iwe.u.bitrate.value =
				((atom->sup_rates[i] & 0x7f) * 500000);
			current_val = iwe_stream_add_value(
				current_ev, current_val, end_buf, &iwe,
				IW_EV_PARAM_LEN);
		}
		/* Check if we added any event */
		if ((current_val - current_ev) > IW_EV_LCP_LEN)
			current_ev = current_val;

		/* Could add beacon_interval and rate (of the received
		 * ProbeResp) to scan results. */

		atom++;
		left -= sizeof(*atom);
	} /* while */

#if 0
	{
		u8 *pos = buffer;
		left = (current_ev - buffer);
		printk(KERN_DEBUG "IW SCAN (len=%d):", left);
		while (left > 0) {
			printk(" %02x", *pos++);
			left--;
		}
		printk("\n");
	}
#endif

	return current_ev - buffer;
}
#endif /* PRISM2_NO_STATION_MODES */

/* ryc++ for auto-channel-setting */
static inline int sec_ioctl_giwscan_sta(struct net_device *dev,
					   struct iw_request_info *info,
					   struct iw_point *data, char *extra)
{
#ifdef PRISM2_NO_STATION_MODES
	return -EOPNOTSUPP;
#else /* PRISM2_NO_STATION_MODES */
	local_info_t *local = (local_info_t *) dev->priv;
	int res, maxlen, len, free_buffer = 1, offset;
	char *buffer;

	/* Wait until the scan is finished. We can probably do better
	 * than that - Jean II */
	if (local->scan_timestamp &&
	    time_before(jiffies, local->scan_timestamp + 3 * HZ)) {
		/* Important note : we don't want to block the caller
		 * until results are ready for various reasons.
		 * First, managing wait queues is complex and racy
		 * (there may be multiple simultaneous callers).
		 * Second, we grab some rtnetlink lock before comming
		 * here (in dev_ioctl()).
		 * Third, the caller can wait on the Wireless Event
		 * - Jean II */
		return -EAGAIN;
	}
	local->scan_timestamp = 0;

	/* Maximum number of scan results on Prism2 is 32. This reserves
	 * enough buffer for all results. Other option would be to modify
	 * local->func->get_rid() to be able to allocate just the correct
	 * amount of memory based on RID header. */
	maxlen = sizeof(struct hfa384x_scan_result_hdr) +
		sizeof(struct hfa384x_scan_result) * HFA384X_SCAN_MAX_RESULTS;
	buffer = kmalloc(maxlen, GFP_KERNEL);
	if (buffer == NULL)
		return -ENOMEM;

	len = local->func->get_rid(dev, HFA384X_RID_SCANRESULTSTABLE,
				   buffer, maxlen, 0);
	offset = sizeof(struct hfa384x_scan_result_hdr);
	PDEBUG(DEBUG_EXTRA, "Scan len = %d (entry=%d)\n",
	       len, sizeof(struct hfa384x_scan_result));
	if (len < 0 || len < sizeof(struct hfa384x_scan_result_hdr)) {
		kfree(buffer);
		if (local->last_scan_results) {
			PDEBUG(DEBUG_EXTRA, "Using last received ScanResults "
			       "info frame instead of RID\n");
			offset = 0;
			len = local->last_scan_results_count *
				sizeof(struct hfa384x_scan_result);
			free_buffer = 0;
			buffer = (char *) local->last_scan_results;
		} else
			return -EINVAL;
	}

	/* Translate to WE format */
	res = sec_translate_scan(dev, extra, buffer + offset,
				    len - offset);
	if (free_buffer)
		kfree(buffer);

	if (res >= 0) {
		printk(KERN_DEBUG "Scan result translation succeeded "
		       "(length=%d)\n", res);
		data->length = res;
		return 0;
	} else {
		printk(KERN_DEBUG "Scan result translation failed (res=%d)\n",
		       res);
		data->length = 0;
		return res;
	}
#endif /* PRISM2_NO_STATION_MODES */
}

static inline int prism2_ioctl_giwscan_sta(struct net_device *dev,
					   struct iw_request_info *info,
					   struct iw_point *data, char *extra)
{
#ifdef PRISM2_NO_STATION_MODES
	return -EOPNOTSUPP;
#else /* PRISM2_NO_STATION_MODES */
	local_info_t *local = (local_info_t *) dev->priv;
	int res, maxlen, len, free_buffer = 1, offset;
	char *buffer;

	/* Wait until the scan is finished. We can probably do better
	 * than that - Jean II */
	if (local->scan_timestamp &&
	    time_before(jiffies, local->scan_timestamp + 3 * HZ)) {
		/* Important note : we don't want to block the caller
		 * until results are ready for various reasons.
		 * First, managing wait queues is complex and racy
		 * (there may be multiple simultaneous callers).
		 * Second, we grab some rtnetlink lock before comming
		 * here (in dev_ioctl()).
		 * Third, the caller can wait on the Wireless Event
		 * - Jean II */
		return -EAGAIN;
	}
	local->scan_timestamp = 0;

	/* Maximum number of scan results on Prism2 is 32. This reserves
	 * enough buffer for all results. Other option would be to modify
	 * local->func->get_rid() to be able to allocate just the correct
	 * amount of memory based on RID header. */
	maxlen = sizeof(struct hfa384x_scan_result_hdr) +
		sizeof(struct hfa384x_scan_result) * HFA384X_SCAN_MAX_RESULTS;
	buffer = kmalloc(maxlen, GFP_KERNEL);
	if (buffer == NULL)
		return -ENOMEM;

	len = local->func->get_rid(dev, HFA384X_RID_SCANRESULTSTABLE,
				   buffer, maxlen, 0);
	offset = sizeof(struct hfa384x_scan_result_hdr);
	PDEBUG(DEBUG_EXTRA, "Scan len = %d (entry=%d)\n",
	       len, sizeof(struct hfa384x_scan_result));
	if (len < 0 || len < sizeof(struct hfa384x_scan_result_hdr)) {
		kfree(buffer);
		if (local->last_scan_results) {
			PDEBUG(DEBUG_EXTRA, "Using last received ScanResults "
			       "info frame instead of RID\n");
			offset = 0;
			len = local->last_scan_results_count *
				sizeof(struct hfa384x_scan_result);
			free_buffer = 0;
			buffer = (char *) local->last_scan_results;
		} else
			return -EINVAL;
	}

	/* Translate to WE format */
	res = prism2_translate_scan(dev, extra, buffer + offset,
				    len - offset);
	if (free_buffer)
		kfree(buffer);

	if (res >= 0) {
		printk(KERN_DEBUG "Scan result translation succeeded "
		       "(length=%d)\n", res);
		data->length = res;
		return 0;
	} else {
		printk(KERN_DEBUG "Scan result translation failed (res=%d)\n",
		       res);
		data->length = 0;
		return res;
	}
#endif /* PRISM2_NO_STATION_MODES */
}
/* ryc++ for auto-channel-setting */
static int sec_ioctl_siwautoch(struct net_device *dev,
				struct iw_request_info *info,
				struct iw_point *data, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	int res;

	if (local->iw_mode == IW_MODE_MASTER) {
		/* In MASTER mode, it doesn't make sense to go around
		 * scanning the frequencies and make the stations we serve
		 * wait when what the user is really interested about is the
		 * list of stations and access points we are talking to.
		 * So, just extract results from our cache...
		 * Jean II */

		/* Translate to WE format */
		res = prism2_ap_translate_scan(dev, extra);
		if (res >= 0) {
			printk(KERN_DEBUG "Scan result translation succeeded "
			       "(length=%d)\n", res);
			data->length = res;
			return 0;
		} else {
			printk(KERN_DEBUG
			       "Scan result translation failed (res=%d)\n",
			       res);
			data->length = 0;
			return res;
		}
	} else {
		/* Station mode */
		return sec_ioctl_giwscan_sta(dev, info, data, extra);
	}
}
static int prism2_ioctl_giwscan(struct net_device *dev,
				struct iw_request_info *info,
				struct iw_point *data, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	int res;
// test point
	if (local->iw_mode == IW_MODE_MASTER) {
		/* In MASTER mode, it doesn't make sense to go around
		 * scanning the frequencies and make the stations we serve
		 * wait when what the user is really interested about is the
		 * list of stations and access points we are talking to.
		 * So, just extract results from our cache...
		 * Jean II */

		/* Translate to WE format */
		res = prism2_ap_translate_scan(dev, extra);
		if (res >= 0) {
			printk(KERN_DEBUG "Scan result translation succeeded "
			       "(length=%d)\n", res);
			data->length = res;
			return 0;
		} else {
			printk(KERN_DEBUG
			       "Scan result translation failed (res=%d)\n",
			       res);
			data->length = 0;
			return res;
		}
		//return prism2_ioctl_giwscan_sta(dev, info, data, extra);
	} else {
		/* Station mode */
		return prism2_ioctl_giwscan_sta(dev, info, data, extra);
	}
}
#endif /* WIRELESS_EXT > 13 */


#if WIRELESS_EXT > 8
static const struct iw_priv_args prism2_priv[] = {
	{ PRISM2_IOCTL_MONITOR,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "monitor" },
	{ PRISM2_IOCTL_READMIF,
	  IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1,
	  IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1, "readmif" },
	{ PRISM2_IOCTL_WRITEMIF,
	  IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 2, 0, "writemif" },
	{ PRISM2_IOCTL_RESET,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "reset" },
	{ PRISM2_IOCTL_INQUIRE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "inquire" },
	{ PRISM2_IOCTL_SET_RID_WORD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "set_rid_word" },
	{ PRISM2_IOCTL_MACCMD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "maccmd" },
#ifdef PRISM2_USE_WE_TYPE_ADDR
	{ PRISM2_IOCTL_WDS_ADD,
	  IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0, "wds_add" },
	{ PRISM2_IOCTL_WDS_DEL,
	  IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0, "wds_del" },
	{ PRISM2_IOCTL_ADDMAC,
	  IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0, "addmac" },
	{ PRISM2_IOCTL_DELMAC,
	  IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0, "delmac" },
	{ PRISM2_IOCTL_KICKMAC,
	  IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1, 0, "kickmac" },
#else /* PRISM2_USE_WE_TYPE_ADDR */
	{ PRISM2_IOCTL_WDS_ADD,
	  IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 18, 0, "wds_add" },
	{ PRISM2_IOCTL_WDS_DEL,
	  IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 18, 0, "wds_del" },
	{ PRISM2_IOCTL_ADDMAC,
	  IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 18, 0, "addmac" },
	{ PRISM2_IOCTL_DELMAC,
	  IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 18, 0, "delmac" },
	{ PRISM2_IOCTL_KICKMAC,
	  IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 18, 0, "kickmac" },
#endif /* PRISM2_USE_WE_TYPE_ADDR */
	/* --- raw access to sub-ioctls --- */
	{ PRISM2_IOCTL_PRISM2_PARAM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "prism2_param" },
#if WIRELESS_EXT >= 12
	{ PRISM2_IOCTL_GET_PRISM2_PARAM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getprism2_param" },
#ifdef PRISM2_USE_WE_SUB_IOCTLS
	/* --- sub-ioctls handlers --- */
	{ PRISM2_IOCTL_PRISM2_PARAM,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "" },
	{ PRISM2_IOCTL_GET_PRISM2_PARAM,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "" },
	/* --- sub-ioctls definitions --- */
	{ PRISM2_PARAM_PTYPE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ptype" },
	{ PRISM2_PARAM_PTYPE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getptype" },
	{ PRISM2_PARAM_TXRATECTRL,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "txratectrl" },
	{ PRISM2_PARAM_TXRATECTRL,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gettxratectrl" },
	{ PRISM2_PARAM_BEACON_INT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "beacon_int" },
	{ PRISM2_PARAM_BEACON_INT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getbeacon_int" },
#ifndef PRISM2_NO_STATION_MODES
	{ PRISM2_PARAM_PSEUDO_IBSS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "pseudo_ibss" },
	{ PRISM2_PARAM_PSEUDO_IBSS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getpseudo_ibss" },
#endif /* PRISM2_NO_STATION_MODES */
	{ PRISM2_PARAM_ALC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "alc" },
	{ PRISM2_PARAM_ALC,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getalc" },
	{ PRISM2_PARAM_TXPOWER,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "txpower" },
	{ PRISM2_PARAM_TXPOWER,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getxpower" },
	{ PRISM2_PARAM_DUMP,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dump" },
	{ PRISM2_PARAM_DUMP,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getdump" },
	{ PRISM2_PARAM_OTHER_AP_POLICY,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "other_ap_policy" },
	{ PRISM2_PARAM_OTHER_AP_POLICY,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getother_ap_pol" },
	{ PRISM2_PARAM_AP_MAX_INACTIVITY,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "max_inactivity" },
	{ PRISM2_PARAM_AP_MAX_INACTIVITY,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getmax_inactivi" },
	{ PRISM2_PARAM_AP_BRIDGE_PACKETS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bridge_packets" },
	{ PRISM2_PARAM_AP_BRIDGE_PACKETS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getbridge_packe" },
	{ PRISM2_PARAM_DTIM_PERIOD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dtim_period" },
	{ PRISM2_PARAM_DTIM_PERIOD,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getdtim_period" },
	{ PRISM2_PARAM_AP_NULLFUNC_ACK,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "nullfunc_ack" },
	{ PRISM2_PARAM_AP_NULLFUNC_ACK,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getnullfunc_ack" },
	{ PRISM2_PARAM_MAX_WDS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "max_wds" },
	{ PRISM2_PARAM_MAX_WDS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getmax_wds" },
	{ PRISM2_PARAM_AP_AUTOM_AP_WDS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "autom_ap_wds" },
	{ PRISM2_PARAM_AP_AUTOM_AP_WDS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getautom_ap_wds" },
	{ PRISM2_PARAM_AP_AUTH_ALGS,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ap_auth_algs" },
	{ PRISM2_PARAM_AP_AUTH_ALGS,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getap_auth_algs" },
	{ PRISM2_PARAM_MONITOR_ALLOW_FCSERR,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "allow_fcserr" },
	{ PRISM2_PARAM_MONITOR_ALLOW_FCSERR,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getallow_fcserr" },
	{ PRISM2_PARAM_HOST_ENCRYPT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "host_encrypt" },
	{ PRISM2_PARAM_HOST_ENCRYPT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gethost_encrypt" },
	{ PRISM2_PARAM_HOST_DECRYPT,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "host_decrypt" },
	{ PRISM2_PARAM_HOST_DECRYPT,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gethost_decrypt" },
	{ PRISM2_PARAM_BUS_MASTER_THRESHOLD_RX,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "busmaster_rx" },
	{ PRISM2_PARAM_BUS_MASTER_THRESHOLD_RX,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getbusmaster_rx" },
	{ PRISM2_PARAM_BUS_MASTER_THRESHOLD_TX,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "busmaster_tx" },
	{ PRISM2_PARAM_BUS_MASTER_THRESHOLD_TX,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getbusmaster_tx" },
#ifndef PRISM2_NO_STATION_MODES
	{ PRISM2_PARAM_HOST_ROAMING,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "host_roaming" },
	{ PRISM2_PARAM_HOST_ROAMING,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gethost_roaming" },
#endif /* PRISM2_NO_STATION_MODES */
	{ PRISM2_PARAM_BCRX_STA_KEY,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "bcrx_sta_key" },
	{ PRISM2_PARAM_BCRX_STA_KEY,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getbcrx_sta_key" },
	{ PRISM2_PARAM_IEEE_802_1X,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ieee_802_1x" },
	{ PRISM2_PARAM_IEEE_802_1X,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getieee_802_1x" },
	{ PRISM2_PARAM_ANTSEL_TX,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "antsel_tx" },
	{ PRISM2_PARAM_ANTSEL_TX,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getantsel_tx" },
	{ PRISM2_PARAM_ANTSEL_RX,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "antsel_rx" },
	{ PRISM2_PARAM_ANTSEL_RX,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getantsel_rx" },
	{ PRISM2_PARAM_MONITOR_TYPE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "monitor_type" },
	{ PRISM2_PARAM_MONITOR_TYPE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getmonitor_type" },
	{ PRISM2_PARAM_WDS_TYPE,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "wds_type" },
	{ PRISM2_PARAM_WDS_TYPE,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getwds_type" },
	{ PRISM2_PARAM_HOSTSCAN,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "hostscan" },
	{ PRISM2_PARAM_HOSTSCAN,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gethostscan" },
	{ PRISM2_PARAM_AP_SCAN,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ap_scan" },
	{ PRISM2_PARAM_AP_SCAN,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getap_scan" },
	{ PRISM2_PARAM_ENH_SEC,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "enh_sec" },
	{ PRISM2_PARAM_ENH_SEC,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getenh_sec" },
#ifdef PRISM2_IO_DEBUG
	{ PRISM2_PARAM_IO_DEBUG,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "io_debug" },
	{ PRISM2_PARAM_IO_DEBUG,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getio_debug" },
#endif /* PRISM2_IO_DEBUG */
	{ PRISM2_PARAM_BASIC_RATES,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "basic_rates" },
	{ PRISM2_PARAM_BASIC_RATES,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getbasic_rates" },
	{ PRISM2_PARAM_OPER_RATES,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "oper_rates" },
	{ PRISM2_PARAM_OPER_RATES,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getoper_rates" },
	{ PRISM2_PARAM_HOSTAPD,
	  IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "hostapd" },
	{ PRISM2_PARAM_HOSTAPD,
	  0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gethostapd" },
#endif /* PRISM2_USE_WE_SUB_IOCTLS */
#endif /* WIRELESS_EXT >= 12 */
};


#if WIRELESS_EXT <= 12
static int prism2_ioctl_giwpriv(struct net_device *dev, struct iw_point *data)
{

	if (!data->pointer ||
	    verify_area(VERIFY_WRITE, data->pointer, sizeof(prism2_priv)))
		return -EINVAL;

	data->length = sizeof(prism2_priv) / sizeof(prism2_priv[0]);
	if (copy_to_user(data->pointer, prism2_priv, sizeof(prism2_priv)))
		return -EINVAL;
	return 0;
}
#endif /* WIRELESS_EXT <= 12 */
#endif /* WIRELESS_EXT > 8 */
#endif /* WIRELESS_EXT */


static int prism2_ioctl_priv_inquire(struct net_device *dev, int *i)
{
	local_info_t *local = (local_info_t *) dev->priv;

	if (local->func->cmd(dev, HFA384X_CMDCODE_INQUIRE, *i, NULL, NULL))
		return -EOPNOTSUPP;

	return 0;
}


static int prism2_ioctl_priv_prism2_param(struct net_device *dev,
					  struct iw_request_info *info,
					  void *wrqu, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	int *i = (int *) extra;
	int param = *i;
	int value = *(i + 1);
	int ret = 0;
	u16 val;

	switch (param) {
	case PRISM2_PARAM_PTYPE:
		if (hostap_set_word(dev, HFA384X_RID_CNFPORTTYPE, value)) {
			ret = -EOPNOTSUPP;
			break;
		}

		if (local->func->reset_port(dev))
			ret = -EINVAL;
		break;

	case PRISM2_PARAM_TXRATECTRL:
		local->fw_tx_rate_control = value;
		break;

	case PRISM2_PARAM_BEACON_INT:
		if (hostap_set_word(dev, HFA384X_RID_CNFBEACONINT, value) ||
		    local->func->reset_port(dev))
			ret = -EINVAL;
		else
			local->beacon_int = value;
		break;

#ifndef PRISM2_NO_STATION_MODES
	case PRISM2_PARAM_PSEUDO_IBSS:
		if (value == local->pseudo_adhoc)
			break;

		if (value != 0 && value != 1) {
			ret = -EINVAL;
			break;
		}

		printk(KERN_DEBUG "prism2: %s: pseudo IBSS change %d -> %d\n",
		       dev->name, local->pseudo_adhoc, value);
		local->pseudo_adhoc = value;
		if (local->iw_mode != IW_MODE_ADHOC)
			break;

		if (hostap_set_word(dev, HFA384X_RID_CNFPORTTYPE,
				    hostap_get_porttype(local))) {
			ret = -EOPNOTSUPP;
			break;
		}

		if (local->func->reset_port(dev))
			ret = -EINVAL;
		break;
#endif /* PRISM2_NO_STATION_MODES */

	case PRISM2_PARAM_ALC:
		printk(KERN_DEBUG "%s: %s ALC\n", dev->name,
		       value == 0 ? "Disabling" : "Enabling");
		val = HFA384X_TEST_CFG_BIT_ALC;
		local->func->cmd(dev, HFA384X_CMDCODE_TEST |
				 (HFA384X_TEST_CFG_BITS << 8),
				 value == 0 ? 0 : 1, &val, NULL);
		break;

	case PRISM2_PARAM_TXPOWER:
		val = value;
		if (local->func->cmd(dev, HFA384X_CMDCODE_WRITEMIF,
				     HFA386X_CR_MANUAL_TX_POWER, &val, NULL))
			ret = -EOPNOTSUPP;
		break;

	case PRISM2_PARAM_DUMP:
		local->frame_dump = value;
		break;

	case PRISM2_PARAM_OTHER_AP_POLICY:
		if (value < 0 || value > 3) {
			ret = -EINVAL;
			break;
		}
		if (local->ap != NULL)
			local->ap->ap_policy = value;
		break;

	case PRISM2_PARAM_AP_MAX_INACTIVITY:
		if (value < 0 || value > 7 * 24 * 60 * 60) {
			ret = -EINVAL;
			break;
		}
		if (local->ap != NULL)
			local->ap->max_inactivity = value * HZ;
		break;

	case PRISM2_PARAM_AP_BRIDGE_PACKETS:
		if (local->ap != NULL)
			local->ap->bridge_packets = value;
		break;

	case PRISM2_PARAM_DTIM_PERIOD:
		if (value < 0 || value > 65535) {
			ret = -EINVAL;
			break;
		}
		if (hostap_set_word(dev, HFA384X_RID_CNFOWNDTIMPERIOD, value)
		    || local->func->reset_port(dev))
			ret = -EINVAL;
		else
			local->dtim_period = value;
		break;

	case PRISM2_PARAM_AP_NULLFUNC_ACK:
		if (local->ap != NULL)
			local->ap->nullfunc_ack = value;
		break;

	case PRISM2_PARAM_MAX_WDS:
		local->wds_max_connections = value;
		break;

	case PRISM2_PARAM_AP_AUTOM_AP_WDS:
		if (local->ap != NULL) {
			if (!local->ap->autom_ap_wds && value) {
				/* add WDS link to all APs in STA table */
				hostap_add_wds_links(local);
			}
			local->ap->autom_ap_wds = value;
		}
		break;

	case PRISM2_PARAM_AP_AUTH_ALGS:
		if (local->ap != NULL)
			local->ap->auth_algs = value;
		break;

	case PRISM2_PARAM_MONITOR_ALLOW_FCSERR:
		local->monitor_allow_fcserr = value;
		break;

	case PRISM2_PARAM_HOST_ENCRYPT:
		local->host_encrypt = value;
		if (hostap_set_encryption(local) ||
		    local->func->reset_port(dev))
			ret = -EINVAL;
		break;

	case PRISM2_PARAM_HOST_DECRYPT:
		local->host_decrypt = value;
		if (hostap_set_encryption(local) ||
		    local->func->reset_port(dev))
			ret = -EINVAL;
		break;

	case PRISM2_PARAM_BUS_MASTER_THRESHOLD_RX:
		local->bus_master_threshold_rx = value;
		break;

	case PRISM2_PARAM_BUS_MASTER_THRESHOLD_TX:
		local->bus_master_threshold_tx = value;
		break;

#ifndef PRISM2_NO_STATION_MODES
	case PRISM2_PARAM_HOST_ROAMING:
		local->host_roaming = value;
		if (hostap_set_word(dev, HFA384X_RID_CNFROAMINGMODE,
				    value ? HFA384X_ROAMING_HOST :
				    HFA384X_ROAMING_FIRMWARE) ||
		    local->func->reset_port(dev))
			ret = -EINVAL;
		else
			local->host_roaming = value;
		break;
#endif /* PRISM2_NO_STATION_MODES */

	case PRISM2_PARAM_BCRX_STA_KEY:
		local->bcrx_sta_key = value;
		break;

	case PRISM2_PARAM_IEEE_802_1X:
		local->ieee_802_1x = value;
		break;

	case PRISM2_PARAM_ANTSEL_TX:
		if (value < 0 || value > HOSTAP_ANTSEL_HIGH) {
			ret = -EINVAL;
			break;
		}
		local->antsel_tx = value;
		hostap_set_antsel(local);
		break;

	case PRISM2_PARAM_ANTSEL_RX:
		if (value < 0 || value > HOSTAP_ANTSEL_HIGH) {
			ret = -EINVAL;
			break;
		}
		local->antsel_rx = value;
		hostap_set_antsel(local);
		break;

	case PRISM2_PARAM_MONITOR_TYPE:
		if (value != PRISM2_MONITOR_80211 &&
		    value != PRISM2_MONITOR_CAPHDR &&
		    value != PRISM2_MONITOR_PRISM) {
			ret = -EINVAL;
			break;
		}
		local->monitor_type = value;
		if (local->iw_mode == IW_MODE_MONITOR)
			hostap_monitor_set_type(local);
		break;

	case PRISM2_PARAM_WDS_TYPE:
		local->wds_type = value;
		break;

	case PRISM2_PARAM_HOSTSCAN:
	{
		struct hfa384x_hostscan_request scan_req;
		u16 rate;

		memset(&scan_req, 0, sizeof(scan_req));
		scan_req.channel_list = __constant_cpu_to_le16(0x3fff);
		switch (value) {
		case 1: rate = HFA384X_RATES_1MBPS; break;
		case 2: rate = HFA384X_RATES_2MBPS; break;
		case 3: rate = HFA384X_RATES_5MBPS; break;
		case 4: rate = HFA384X_RATES_11MBPS; break;
		default: rate = HFA384X_RATES_1MBPS; break;
		}
		scan_req.txrate = cpu_to_le16(rate);
		/* leave SSID empty to accept all SSIDs */

		if (local->iw_mode == IW_MODE_MASTER) {
			if (hostap_set_word(dev, HFA384X_RID_CNFPORTTYPE,
					    HFA384X_PORTTYPE_BSS) ||
			    local->func->reset_port(dev))
				printk(KERN_DEBUG "Leaving Host AP mode "
				       "for HostScan failed\n");
		}

		if (local->func->set_rid(dev, HFA384X_RID_HOSTSCAN, &scan_req,
					 sizeof(scan_req))) {
			printk(KERN_DEBUG "HOSTSCAN failed\n");
			ret = -EINVAL;
		}
		if (local->iw_mode == IW_MODE_MASTER) {
			wait_queue_t __wait;
			init_waitqueue_entry(&__wait, current);
			add_wait_queue(&local->hostscan_wq, &__wait);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(HZ);
			if (signal_pending(current))
				ret = -EINTR;
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&local->hostscan_wq, &__wait);

			if (hostap_set_word(dev, HFA384X_RID_CNFPORTTYPE,
					    HFA384X_PORTTYPE_HOSTAP) ||
			    local->func->reset_port(dev))
				printk(KERN_DEBUG "Returning to Host AP mode "
				       "after HostScan failed\n");
		}
		break;
	}

	case PRISM2_PARAM_AP_SCAN:
		local->passive_scan_interval = value;
		if (timer_pending(&local->passive_scan_timer))
			del_timer(&local->passive_scan_timer);
		if (value > 0) {
			local->passive_scan_timer.expires = jiffies +
				local->passive_scan_interval * HZ;
			add_timer(&local->passive_scan_timer);
		}
		break;

	case PRISM2_PARAM_ENH_SEC:
		if (value < 0 || value > 3) {
			ret = -EINVAL;
			break;
		}
		local->enh_sec = value;
		if (hostap_set_word(dev, HFA384X_RID_CNFENHSECURITY,
				    local->enh_sec)) {
			printk(KERN_INFO "%s: cnfEnhSecurity requires STA f/w "
			       "1.6.3 or newer\n", dev->name);
			ret = -EOPNOTSUPP;
		}
		break;

#ifdef PRISM2_IO_DEBUG
	case PRISM2_PARAM_IO_DEBUG:
		local->io_debug_enabled = value;
		break;
#endif /* PRISM2_IO_DEBUG */

	case PRISM2_PARAM_BASIC_RATES:
		local->basic_rates = value;
		if (hostap_set_word(dev, HFA384X_RID_CNFBASICRATES,
				    local->basic_rates) ||
		    local->func->reset_port(dev))
			ret = -EINVAL;
		break;

	case PRISM2_PARAM_OPER_RATES:
		local->tx_rate_control = value;
		if (hostap_set_rate(dev))
			ret = -EINVAL;
		break;

	case PRISM2_PARAM_HOSTAPD:
		ret = hostap_set_hostapd(local, value, 1);
		break;

	default:
		printk(KERN_DEBUG "%s: prism2_param: unknown param %d\n",
		       dev->name, param);
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}


#if WIRELESS_EXT >= 12
static int prism2_ioctl_priv_get_prism2_param(struct net_device *dev,
					      struct iw_request_info *info,
					      void *wrqu, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	int *param = (int *) extra;
	int ret = 0;
	u16 val;

	switch (*param) {
	case PRISM2_PARAM_PTYPE:
		if (local->func->get_rid(dev, HFA384X_RID_CNFPORTTYPE,
					 &val, 2, 1) < 0)
			ret = -EINVAL;
		else
			*param = le16_to_cpu(val);
		break;

	case PRISM2_PARAM_TXRATECTRL:
		*param = local->fw_tx_rate_control;
		break;

	case PRISM2_PARAM_BEACON_INT:
		*param = local->beacon_int;
		break;

	case PRISM2_PARAM_PSEUDO_IBSS:
		*param = local->pseudo_adhoc;
		break;

	case PRISM2_PARAM_ALC:
		ret = -EOPNOTSUPP; /* FIX */
		break;

	case PRISM2_PARAM_TXPOWER:
		if (local->func->cmd(dev, HFA384X_CMDCODE_READMIF,
				HFA386X_CR_MANUAL_TX_POWER, NULL, &val))
			ret = -EOPNOTSUPP;
		*param = val;
		break;

	case PRISM2_PARAM_DUMP:
		*param = local->frame_dump;
		break;

	case PRISM2_PARAM_OTHER_AP_POLICY:
		if (local->ap != NULL)
			*param = local->ap->ap_policy;
		else
			ret = -EOPNOTSUPP;
		break;

	case PRISM2_PARAM_AP_MAX_INACTIVITY:
		if (local->ap != NULL)
			*param = local->ap->max_inactivity / HZ;
		else
			ret = -EOPNOTSUPP;
		break;

	case PRISM2_PARAM_AP_BRIDGE_PACKETS:
		if (local->ap != NULL)
			*param = local->ap->bridge_packets;
		else
			ret = -EOPNOTSUPP;
		break;

	case PRISM2_PARAM_DTIM_PERIOD:
		*param = local->dtim_period;
		break;

	case PRISM2_PARAM_AP_NULLFUNC_ACK:
		if (local->ap != NULL)
			*param = local->ap->nullfunc_ack;
		else
			ret = -EOPNOTSUPP;
		break;

	case PRISM2_PARAM_MAX_WDS:
		*param = local->wds_max_connections;
		break;

	case PRISM2_PARAM_AP_AUTOM_AP_WDS:
		if (local->ap != NULL)
			*param = local->ap->autom_ap_wds;
		else
			ret = -EOPNOTSUPP;
		break;

	case PRISM2_PARAM_AP_AUTH_ALGS:
		if (local->ap != NULL)
			*param = local->ap->auth_algs;
		else
			ret = -EOPNOTSUPP;
		break;

	case PRISM2_PARAM_MONITOR_ALLOW_FCSERR:
		*param = local->monitor_allow_fcserr;
		break;

	case PRISM2_PARAM_HOST_ENCRYPT:
		*param = local->host_encrypt;
		break;

	case PRISM2_PARAM_HOST_DECRYPT:
		*param = local->host_decrypt;
		break;

	case PRISM2_PARAM_BUS_MASTER_THRESHOLD_RX:
		*param = local->bus_master_threshold_rx;
		break;

	case PRISM2_PARAM_BUS_MASTER_THRESHOLD_TX:
		*param = local->bus_master_threshold_tx;
		break;

	case PRISM2_PARAM_HOST_ROAMING:
		*param = local->host_roaming;
		break;

	case PRISM2_PARAM_BCRX_STA_KEY:
		*param = local->bcrx_sta_key;
		break;

	case PRISM2_PARAM_IEEE_802_1X:
		*param = local->ieee_802_1x;
		break;

	case PRISM2_PARAM_ANTSEL_TX:
		*param = local->antsel_tx;
		break;

	case PRISM2_PARAM_ANTSEL_RX:
		*param = local->antsel_rx;
		break;

	case PRISM2_PARAM_MONITOR_TYPE:
		*param = local->monitor_type;
		break;

	case PRISM2_PARAM_WDS_TYPE:
		*param = local->wds_type;
		break;

	case PRISM2_PARAM_HOSTSCAN:
		ret = -EOPNOTSUPP;
		break;

	case PRISM2_PARAM_AP_SCAN:
		*param = local->passive_scan_interval;
		break;

	case PRISM2_PARAM_ENH_SEC:
		*param = local->enh_sec;
		break;

#ifdef PRISM2_IO_DEBUG
	case PRISM2_PARAM_IO_DEBUG:
		*param = local->io_debug_enabled;
		break;
#endif /* PRISM2_IO_DEBUG */

	case PRISM2_PARAM_BASIC_RATES:
		*param = local->basic_rates;
		break;

	case PRISM2_PARAM_OPER_RATES:
		*param = local->tx_rate_control;
		break;

	case PRISM2_PARAM_HOSTAPD:
		*param = local->hostapd;
		break;

	default:
		printk(KERN_DEBUG "%s: get_prism2_param: unknown param %d\n",
		       dev->name, *param);
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}
#endif /* WIRELESS_EXT >= 12 */


static int prism2_ioctl_priv_readmif(struct net_device *dev,
				     struct iw_request_info *info,
				     void *wrqu, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	u16 resp0;

	if (local->func->cmd(dev, HFA384X_CMDCODE_READMIF, *extra, NULL,
			     &resp0))
		return -EOPNOTSUPP;
	else
		*extra = resp0;

	return 0;
}


static int prism2_ioctl_priv_writemif(struct net_device *dev,
				      struct iw_request_info *info,
				      void *wrqu, char *extra)
{
	local_info_t *local = (local_info_t *) dev->priv;
	u16 cr, val;

	cr = *extra;
	val = *(extra + 1);
	if (local->func->cmd(dev, HFA384X_CMDCODE_WRITEMIF, cr, &val, NULL))
		return -EOPNOTSUPP;

	return 0;
}


static int prism2_ioctl_priv_monitor(struct net_device *dev, int *i)
{
#if WIRELESS_EXT > 8
	local_info_t *local = (local_info_t *) dev->priv;
	int ret = 0;
	u32 mode;

	printk(KERN_DEBUG "%s: process %d (%s) used deprecated iwpriv monitor "
	       "- update software to use iwconfig mode monitor\n",
	       dev->name, current->pid, current->comm);

	/* Backward compatibility code - this can be removed at some point */
	if (*i == 0) {
		/* Disable monitor mode - old mode was not saved, so go to
		 * Master mode */
		mode = IW_MODE_MASTER;
		ret = prism2_ioctl_siwmode(dev, NULL, &mode, NULL);
	} else if (*i == 1) {
		/* netlink socket mode is not supported anymore since it did
		 * not separate different devices from each other and was not
		 * best method for delivering large amount of packets to
		 * user space */
		ret = -EOPNOTSUPP;
	} else if (*i == 2 || *i == 3) {
		switch (*i) {
		case 2:
			local->monitor_type = PRISM2_MONITOR_80211;
			break;
		case 3:
			local->monitor_type = PRISM2_MONITOR_PRISM;
			break;
		}
		mode = IW_MODE_MONITOR;
		ret = prism2_ioctl_siwmode(dev, NULL, &mode, NULL);
		hostap_monitor_mode_enable(local);
	} else
		ret = -EINVAL;

	return ret;
#else /* WIRELESS_EXT > 8 */
	return -EOPNOTSUPP;
#endif /* WIRELESS_EXT > 8 */
}


static int prism2_ioctl_priv_reset(struct net_device *dev, int *i)
{
	local_info_t *local = (local_info_t *) dev->priv;

	printk(KERN_DEBUG "%s: manual reset request(%d)\n", dev->name, *i);
	switch (*i) {
	case 0:
		/* Disable and enable card */
		local->func->hw_shutdown(dev, 1);
		local->func->hw_config(dev, 0);
		break;

	case 1:
		/* COR sreset */
		local->func->hw_reset(dev);
		break;

	case 2:
		/* Disable and enable port 0 */
		local->func->reset_port(dev);
		break;

	case 3:
		if (local->func->cmd(dev, HFA384X_CMDCODE_DISABLE, 0, NULL,
				     NULL))
			return -EINVAL;
		break;

	case 4:
		if (local->func->cmd(dev, HFA384X_CMDCODE_ENABLE, 0, NULL,
				     NULL))
			return -EINVAL;
		break;

	default:
		printk(KERN_DEBUG "Unknown reset request %d\n", *i);
		return -EOPNOTSUPP;
	}

	return 0;
}


#ifndef PRISM2_USE_WE_TYPE_ADDR
static inline int hex2int(char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);
	return -1;
}

static int macstr2addr(char *macstr, u8 *addr)
{
	int i, val, val2;
	char *pos = macstr;

	for (i = 0; i < 6; i++) {
		val = hex2int(*pos++);
		if (val < 0)
			return -1;
		val2 = hex2int(*pos++);
		if (val2 < 0)
			return -1;
		addr[i] = (val * 16 + val2) & 0xff;

		if (i < 5 && *pos++ != ':')
			return -1;
	}

	return 0;
}


static int prism2_ioctl_priv_wds(struct net_device *dev, int add, char *macstr)
{
	local_info_t *local = (local_info_t *) dev->priv;
	u8 addr[6];

	if (macstr2addr(macstr, addr)) {
		printk(KERN_DEBUG "Invalid MAC address\n");
		return -EINVAL;
	}

	if (add)
		return prism2_wds_add(local, addr, 1);
	else
		return prism2_wds_del(local, addr, 1, 0);
}
#endif /* PRISM2_USE_WE_TYPE_ADDR */


static int prism2_ioctl_priv_set_rid_word(struct net_device *dev, int *i)
{
	int rid = *i;
	int value = *(i + 1);

	printk(KERN_DEBUG "%s: Set RID[0x%X] = %d\n", dev->name, rid, value);

	if (hostap_set_word(dev, rid, value))
		return -EINVAL;

	return 0;
}


#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
static int ap_mac_cmd_ioctl(local_info_t *local, int *cmd)
{
	int ret = 0;

	switch (*cmd) {
	case AP_MAC_CMD_POLICY_OPEN:
		local->ap->mac_restrictions.policy = MAC_POLICY_OPEN;
		break;
	case AP_MAC_CMD_POLICY_ALLOW:
		local->ap->mac_restrictions.policy = MAC_POLICY_ALLOW;
		break;
	case AP_MAC_CMD_POLICY_DENY:
		local->ap->mac_restrictions.policy = MAC_POLICY_DENY;
		break;
	case AP_MAC_CMD_FLUSH:
		ap_control_flush_macs(&local->ap->mac_restrictions);
		break;
	case AP_MAC_CMD_KICKALL:
		ap_control_kickall(local->ap);
		hostap_deauth_all_stas(local->dev, local->ap, 0);
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}


enum { AP_CTRL_MAC_ADD, AP_CTRL_MAC_DEL, AP_CTRL_MAC_KICK };

#ifndef PRISM2_USE_WE_TYPE_ADDR
static int ap_mac_ioctl(local_info_t *local, char *macstr, int cmd)
{
	u8 addr[6];

	if (macstr2addr(macstr, addr)) {
		printk(KERN_DEBUG "Invalid MAC address '%s'\n", macstr);
		return -EINVAL;
	}

	switch (cmd) {
	case AP_CTRL_MAC_ADD:
		return ap_control_add_mac(&local->ap->mac_restrictions, addr);
	case AP_CTRL_MAC_DEL:
		return ap_control_del_mac(&local->ap->mac_restrictions, addr);
	case AP_CTRL_MAC_KICK:
		return ap_control_kick_mac(local->ap, local->dev, addr);
	default:
		return -EOPNOTSUPP;
	}
}
#endif /* PRISM2_USE_WE_TYPE_ADDR */
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */


#if defined(PRISM2_DOWNLOAD_SUPPORT) && WIRELESS_EXT > 8
static int prism2_ioctl_priv_download(local_info_t *local, struct iw_point *p)
{
	struct prism2_download_param *param;
	int ret = 0;

	if (p->length < sizeof(struct prism2_download_param) ||
	    p->length > 1024 || !p->pointer)
		return -EINVAL;

	param = (struct prism2_download_param *)
		kmalloc(p->length, GFP_KERNEL);
	if (param == NULL)
		return -ENOMEM;

	if (copy_from_user(param, p->pointer, p->length)) {
		ret = -EFAULT;
		goto out;
	}

	if (p->length < sizeof(struct prism2_download_param) +
	    param->num_areas * sizeof(struct prism2_download_area)) {
		ret = -EINVAL;
		goto out;
	}

	ret = local->func->download(local, param);

 out:
	if (param != NULL)
		kfree(param);

	return ret;
}
#endif /* PRISM2_DOWNLOAD_SUPPORT and WIRELESS_EXT > 8 */


static int prism2_ioctl_set_encryption(local_info_t *local,
				       struct prism2_hostapd_param *param,
				       int param_len)
{
	int ret = 0;
	struct hostap_crypto_ops *ops;
	struct prism2_crypt_data **crypt;
	void *sta_ptr;

	param->u.crypt.err = 0;
	param->u.crypt.alg[HOSTAP_CRYPT_ALG_NAME_LEN - 1] = '\0';

	if (param_len !=
	    (int) ((char *) param->u.crypt.key - (char *) param) +
	    param->u.crypt.key_len)
		return -EINVAL;

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff) {
		sta_ptr = NULL;
		crypt = &local->crypt;
	} else {
		sta_ptr = ap_crypt_get_ptrs(
			local->ap, param->sta_addr,
			(param->u.crypt.flags & HOSTAP_CRYPT_FLAG_PERMANENT),
			&crypt);

		if (sta_ptr == NULL) {
			param->u.crypt.err = HOSTAP_CRYPT_ERR_UNKNOWN_ADDR;
			return -EINVAL;
		}
	}

	if (strcmp(param->u.crypt.alg, "none") == 0) {
		prism2_crypt_delayed_deinit(local, crypt);
		goto done;
	}

	ops = hostap_get_crypto_ops(param->u.crypt.alg);
	if (ops == NULL && strcmp(param->u.crypt.alg, "WEP") == 0) {
		request_module("hostap_crypt_wep");
		ops = hostap_get_crypto_ops(param->u.crypt.alg);
	}
	if (ops == NULL) {
		printk(KERN_DEBUG "%s: unknown crypto alg '%s'\n",
		       local->dev->name, param->u.crypt.alg);
		param->u.crypt.err = HOSTAP_CRYPT_ERR_UNKNOWN_ALG;
		ret = -EINVAL;
		goto done;
	}

	/* station based encryption and other than WEP algorithms require
	 * host-based encryption, so force them on automatically */
	local->host_decrypt = local->host_encrypt = 1;

	if (*crypt == NULL || (*crypt)->ops != ops) {
		struct prism2_crypt_data *new_crypt;

		prism2_crypt_delayed_deinit(local, crypt);

		new_crypt = (struct prism2_crypt_data *)
			kmalloc(sizeof(struct prism2_crypt_data), GFP_KERNEL);
		if (new_crypt == NULL) {
			ret = -ENOMEM;
			goto done;
		}
		memset(new_crypt, 0, sizeof(struct prism2_crypt_data));
		new_crypt->ops = ops;
		new_crypt->priv = new_crypt->ops->init();
		if (new_crypt->priv == NULL) {
			kfree(new_crypt);
			param->u.crypt.err =
				HOSTAP_CRYPT_ERR_CRYPT_INIT_FAILED;
			ret = -EINVAL;
			goto done;
		}

		*crypt = new_crypt;
	}

	if ((!(param->u.crypt.flags & HOSTAP_CRYPT_FLAG_SET_TX_KEY) ||
	     param->u.crypt.key_len > 0) && (*crypt)->ops->set_key &&
	    (*crypt)->ops->set_key(param->u.crypt.idx, param->u.crypt.key,
			      param->u.crypt.key_len, (*crypt)->priv) < 0) {
		printk(KERN_DEBUG "%s: key setting failed\n",
		       local->dev->name);
		param->u.crypt.err = HOSTAP_CRYPT_ERR_KEY_SET_FAILED;
		ret = -EINVAL;
		goto done;
	}

	if ((param->u.crypt.flags & HOSTAP_CRYPT_FLAG_SET_TX_KEY) &&
	    (*crypt)->ops->set_key_idx &&
	    (*crypt)->ops->set_key_idx(param->u.crypt.idx, (*crypt)->priv) < 0)
	{
		printk(KERN_DEBUG "%s: TX key idx setting failed\n",
		       local->dev->name);
		param->u.crypt.err = HOSTAP_CRYPT_ERR_TX_KEY_SET_FAILED;
		ret = -EINVAL;
		goto done;
	}

 done:
	if (sta_ptr)
		hostap_handle_sta_release(sta_ptr);

	if (ret == 0 &&
	    (hostap_set_encryption(local) ||
	     local->func->reset_port(local->dev))) {
		param->u.crypt.err = HOSTAP_CRYPT_ERR_CARD_CONF_FAILED;
		return -EINVAL;
	}

	return ret;
}


static int prism2_ioctl_get_encryption(local_info_t *local,
				       struct prism2_hostapd_param *param,
				       int param_len)
{
	struct prism2_crypt_data **crypt;
	void *sta_ptr;
	int max_key_len;

	param->u.crypt.err = 0;

	max_key_len = param_len -
		(int) ((char *) param->u.crypt.key - (char *) param);
	if (max_key_len < 0)
		return -EINVAL;

	if (param->sta_addr[0] == 0xff && param->sta_addr[1] == 0xff &&
	    param->sta_addr[2] == 0xff && param->sta_addr[3] == 0xff &&
	    param->sta_addr[4] == 0xff && param->sta_addr[5] == 0xff) {
		sta_ptr = NULL;
		crypt = &local->crypt;
	} else {
		sta_ptr = ap_crypt_get_ptrs(local->ap, param->sta_addr, 0,
					    &crypt);

		if (sta_ptr == NULL) {
			param->u.crypt.err = HOSTAP_CRYPT_ERR_UNKNOWN_ADDR;
			return -EINVAL;
		}
	}

	if (*crypt == NULL || (*crypt)->ops == NULL) {
		memcpy(param->u.crypt.alg, "none", 5);
		param->u.crypt.key_len = 0;
		param->u.crypt.idx = 0xff;
	} else {
		strncpy(param->u.crypt.alg, (*crypt)->ops->name,
			HOSTAP_CRYPT_ALG_NAME_LEN);
		param->u.crypt.key_len = 0;
		if (param->u.crypt.idx >= WEP_KEYS &&
		    (*crypt)->ops->get_key_idx)
			param->u.crypt.idx =
				(*crypt)->ops->get_key_idx((*crypt)->priv);

		if (param->u.crypt.idx < WEP_KEYS && (*crypt)->ops->get_key)
			param->u.crypt.key_len =
				(*crypt)->ops->get_key(param->u.crypt.idx,
						       param->u.crypt.key,
						       max_key_len,
						       (*crypt)->priv);
	}

	if (sta_ptr)
		hostap_handle_sta_release(sta_ptr);

	return 0;
}


static int prism2_ioctl_get_rid(local_info_t *local,
				struct prism2_hostapd_param *param,
				int param_len)
{
	int max_len, res;

	max_len = param_len - PRISM2_HOSTAPD_RID_HDR_LEN;
	if (max_len < 0)
		return -EINVAL;

	res = local->func->get_rid(local->dev, param->u.rid.rid,
				   param->u.rid.data, param->u.rid.len, 0);
	if (res >= 0) {
		param->u.rid.len = res;
		return 0;
	}

	return res;
}


static int prism2_ioctl_set_rid(local_info_t *local,
				struct prism2_hostapd_param *param,
				int param_len)
{
	int max_len;

	max_len = param_len - PRISM2_HOSTAPD_RID_HDR_LEN;
	if (max_len < 0 || max_len < param->u.rid.len)
		return -EINVAL;

	return local->func->set_rid(local->dev, param->u.rid.rid,
				    param->u.rid.data, param->u.rid.len);
}


static int prism2_ioctl_set_assoc_ap_addr(local_info_t *local,
					  struct prism2_hostapd_param *param,
					  int param_len)
{
	printk(KERN_DEBUG "%ssta: associated as client with AP " MACSTR "\n",
	       local->dev->name, MAC2STR(param->sta_addr));
	memcpy(local->assoc_ap_addr, param->sta_addr, ETH_ALEN);
	return 0;
}


static int prism2_ioctl_priv_hostapd(local_info_t *local, struct iw_point *p)
{
	struct prism2_hostapd_param *param;
	int ret = 0;
	int ap_ioctl = 0;

	if (p->length < sizeof(struct prism2_hostapd_param) ||
	    p->length > PRISM2_HOSTAPD_MAX_BUF_SIZE || !p->pointer)
		return -EINVAL;

	param = (struct prism2_hostapd_param *) kmalloc(p->length, GFP_KERNEL);
	if (param == NULL)
		return -ENOMEM;

	if (copy_from_user(param, p->pointer, p->length)) {
		ret = -EFAULT;
		goto out;
	}

	switch (param->cmd) {
	case PRISM2_SET_ENCRYPTION:
		ret = prism2_ioctl_set_encryption(local, param, p->length);
		break;
	case PRISM2_GET_ENCRYPTION:
		ret = prism2_ioctl_get_encryption(local, param, p->length);
		break;
	case PRISM2_HOSTAPD_GET_RID:
		ret = prism2_ioctl_get_rid(local, param, p->length);
		break;
	case PRISM2_HOSTAPD_SET_RID:
		ret = prism2_ioctl_set_rid(local, param, p->length);
		break;
	case PRISM2_HOSTAPD_SET_ASSOC_AP_ADDR:
		ret = prism2_ioctl_set_assoc_ap_addr(local, param, p->length);
		break;
	default:
		ret = prism2_hostapd(local->ap, param);
		ap_ioctl = 1;
		break;
	}

	if (ret == 1 || !ap_ioctl) {
		if (copy_to_user(p->pointer, param, p->length)) {
			ret = -EFAULT;
			goto out;
		} else if (ap_ioctl)
			ret = 0;
	}

 out:
	if (param != NULL)
		kfree(param);

	return ret;
}


#if WIRELESS_EXT > 12
/* Structures to export the Wireless Handlers */

static const iw_handler prism2_handler[] =
{
	(iw_handler) NULL,				/* SIOCSIWCOMMIT */
	(iw_handler) prism2_get_name,			/* SIOCGIWNAME */
	(iw_handler) NULL,				/* SIOCSIWNWID */
	(iw_handler) NULL,				/* SIOCGIWNWID */
	(iw_handler) prism2_ioctl_siwfreq,		/* SIOCSIWFREQ */
	(iw_handler) prism2_ioctl_giwfreq,		/* SIOCGIWFREQ */
	(iw_handler) prism2_ioctl_siwmode,		/* SIOCSIWMODE */
	(iw_handler) prism2_ioctl_giwmode,		/* SIOCGIWMODE */
	(iw_handler) prism2_ioctl_siwsens,		/* SIOCSIWSENS */
	(iw_handler) prism2_ioctl_giwsens,		/* SIOCGIWSENS */
	(iw_handler) NULL /* not used */,		/* SIOCSIWRANGE */
	(iw_handler) prism2_ioctl_giwrange,		/* SIOCGIWRANGE */
	(iw_handler) NULL /* not used */,		/* SIOCSIWPRIV */
	(iw_handler) NULL /* kernel code */,		/* SIOCGIWPRIV */
	(iw_handler) NULL /* not used */,		/* SIOCSIWSTATS */
	(iw_handler) NULL /* kernel code */,		/* SIOCGIWSTATS */
#if WIRELESS_EXT > 15
	iw_handler_set_spy,				/* SIOCSIWSPY */
	iw_handler_get_spy,				/* SIOCGIWSPY */
	iw_handler_set_thrspy,				/* SIOCSIWTHRSPY */
	iw_handler_get_thrspy,				/* SIOCGIWTHRSPY */
#else /* WIRELESS_EXT > 15 */
	(iw_handler) NULL,				/* SIOCSIWSPY */
	(iw_handler) prism2_ioctl_giwspy,		/* SIOCGIWSPY */
	(iw_handler) NULL,				/* -- hole -- */
	(iw_handler) NULL,				/* -- hole -- */
#endif /* WIRELESS_EXT > 15 */
	(iw_handler) prism2_ioctl_siwap,		/* SIOCSIWAP */
	(iw_handler) prism2_ioctl_giwap,		/* SIOCGIWAP */
	(iw_handler) sec_ioctl_siwautoch,		/* -- hole -- ryc++ --> SIOCSIWAUTOCH */
	(iw_handler) prism2_ioctl_giwaplist,		/* SIOCGIWAPLIST */
#if WIRELESS_EXT > 13
	(iw_handler) prism2_ioctl_siwscan,		/* SIOCSIWSCAN */
	(iw_handler) prism2_ioctl_giwscan,		/* SIOCGIWSCAN */
#else /* WIRELESS_EXT > 13 */
	(iw_handler) NULL,				/* SIOCSIWSCAN */
	(iw_handler) NULL,				/* SIOCGIWSCAN */
#endif /* WIRELESS_EXT > 13 */
	(iw_handler) prism2_ioctl_siwessid,		/* SIOCSIWESSID */
	(iw_handler) prism2_ioctl_giwessid,		/* SIOCGIWESSID */
	(iw_handler) prism2_ioctl_siwnickn,		/* SIOCSIWNICKN */
	(iw_handler) prism2_ioctl_giwnickn,		/* SIOCGIWNICKN */
	(iw_handler) NULL,				/* -- hole -- */
	(iw_handler) NULL,				/* -- hole -- */
	(iw_handler) prism2_ioctl_siwrate,		/* SIOCSIWRATE */
	(iw_handler) prism2_ioctl_giwrate,		/* SIOCGIWRATE */
	(iw_handler) prism2_ioctl_siwrts,		/* SIOCSIWRTS */
	(iw_handler) prism2_ioctl_giwrts,		/* SIOCGIWRTS */
	(iw_handler) prism2_ioctl_siwfrag,		/* SIOCSIWFRAG */
	(iw_handler) prism2_ioctl_giwfrag,		/* SIOCGIWFRAG */
	(iw_handler) prism2_ioctl_siwtxpow,		/* SIOCSIWTXPOW */
	(iw_handler) prism2_ioctl_giwtxpow,		/* SIOCGIWTXPOW */
	(iw_handler) prism2_ioctl_siwretry,		/* SIOCSIWRETRY */
	(iw_handler) prism2_ioctl_giwretry,		/* SIOCGIWRETRY */
	(iw_handler) prism2_ioctl_siwencode,		/* SIOCSIWENCODE */
	(iw_handler) prism2_ioctl_giwencode,		/* SIOCGIWENCODE */
	(iw_handler) prism2_ioctl_siwpower,		/* SIOCSIWPOWER */
	(iw_handler) prism2_ioctl_giwpower,		/* SIOCGIWPOWER */
};

static const iw_handler prism2_private_handler[] =
{							/* SIOCIWFIRSTPRIV + */
	(iw_handler) prism2_ioctl_priv_prism2_param,	/* 0 */
	(iw_handler) prism2_ioctl_priv_get_prism2_param, /* 1 */
	(iw_handler) prism2_ioctl_priv_writemif,	/* 2 */
	(iw_handler) prism2_ioctl_priv_readmif,		/* 3 */
};

static const struct iw_handler_def hostap_iw_handler_def =
{
	.num_standard	= sizeof(prism2_handler) / sizeof(iw_handler),
	.num_private	= sizeof(prism2_private_handler) / sizeof(iw_handler),
	.num_private_args = sizeof(prism2_priv) / sizeof(struct iw_priv_args),
	.standard	= (iw_handler *) prism2_handler,
	.private	= (iw_handler *) prism2_private_handler,
	.private_args	= (struct iw_priv_args *) prism2_priv,
#if WIRELESS_EXT > 15
	.spy_offset	= ((void *) (&((local_info_t *) NULL)->spy_data) -
			   (void *) NULL),
#endif /* WIRELESS_EXT > 15 */
};
#endif	/* WIRELESS_EXT > 12 */


int hostap_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
#ifdef WIRELESS_EXT
	struct iwreq *wrq = (struct iwreq *) ifr;
#endif
	local_info_t *local = (local_info_t *) dev->priv;
	int ret = 0;

	switch (cmd) {

#ifdef WIRELESS_EXT
#if WIRELESS_EXT <= 12
	case SIOCGIWNAME:
		ret = prism2_get_name(dev, NULL, (char *) &wrq->u, NULL);
		break;

	case SIOCSIWFREQ:
		ret = prism2_ioctl_siwfreq(dev, NULL, &wrq->u.freq, NULL);
		break;
	case SIOCGIWFREQ:
		ret = prism2_ioctl_giwfreq(dev, NULL, &wrq->u.freq, NULL);
		break;

	case SIOCSIWAP:
		ret = prism2_ioctl_siwap(dev, NULL, &wrq->u.ap_addr, NULL);
		break;
	case SIOCGIWAP:
		ret = prism2_ioctl_giwap(dev, NULL, &wrq->u.ap_addr, NULL);
		break;

#if WIRELESS_EXT > 8
	case SIOCSIWESSID:
		if (!wrq->u.essid.pointer)
			ret = -EINVAL;
		else if (wrq->u.essid.length > IW_ESSID_MAX_SIZE)
			ret = -E2BIG;
		else {
			char ssid[IW_ESSID_MAX_SIZE];
			if (copy_from_user(ssid, wrq->u.essid.pointer,
					   wrq->u.essid.length)) {
				ret = -EFAULT;
				break;
			}
			ret = prism2_ioctl_siwessid(dev, NULL, &wrq->u.essid,
						    ssid);
		}
		break;
	case SIOCGIWESSID:
		if (wrq->u.essid.length > IW_ESSID_MAX_SIZE)
			ret = -E2BIG;
		else if (wrq->u.essid.pointer) {
			char ssid[IW_ESSID_MAX_SIZE];
			ret = prism2_ioctl_giwessid(dev, NULL, &wrq->u.essid,
						    ssid);
			if (copy_to_user(wrq->u.essid.pointer, ssid,
					 wrq->u.essid.length))
				ret = -EFAULT;
		}
		break;

	case SIOCSIWRATE:
		ret = prism2_ioctl_siwrate(dev, NULL, &wrq->u.bitrate, NULL);
		break;
	case SIOCGIWRATE:
		ret = prism2_ioctl_giwrate(dev, NULL, &wrq->u.bitrate, NULL);
		break;

	case SIOCSIWRTS:
		ret = prism2_ioctl_siwrts(dev, NULL, &wrq->u.rts, NULL);
		break;
	case SIOCGIWRTS:
		ret = prism2_ioctl_giwrts(dev, NULL, &wrq->u.rts, NULL);
		break;

	case SIOCSIWFRAG:
		ret = prism2_ioctl_siwfrag(dev, NULL, &wrq->u.rts, NULL);
		break;
	case SIOCGIWFRAG:
		ret = prism2_ioctl_giwfrag(dev, NULL, &wrq->u.rts, NULL);
		break;

	case SIOCSIWENCODE:
		{
			char keybuf[WEP_KEY_LEN];
			if (wrq->u.encoding.pointer) {
				if (wrq->u.encoding.length > WEP_KEY_LEN) {
					ret = -E2BIG;
					break;
				}
				if (copy_from_user(keybuf,
						   wrq->u.encoding.pointer,
						   wrq->u.encoding.length)) {
					ret = -EFAULT;
					break;
				}
			} else if (wrq->u.encoding.length != 0) {
				ret = -EINVAL;
				break;
			}
			ret = prism2_ioctl_siwencode(dev, NULL,
						     &wrq->u.encoding, keybuf);
		}
		break;
	case SIOCGIWENCODE:
		if (!capable(CAP_NET_ADMIN))
			ret = -EPERM;
		else if (wrq->u.encoding.pointer) {
			char keybuf[WEP_KEY_LEN];
			ret = prism2_ioctl_giwencode(dev, NULL,
						     &wrq->u.encoding, keybuf);
			if (copy_to_user(wrq->u.encoding.pointer, keybuf,
					 wrq->u.encoding.length))
				ret = -EFAULT;
		}
		break;

	case SIOCSIWNICKN:
		if (wrq->u.essid.length > IW_ESSID_MAX_SIZE)
			ret = -E2BIG;
		else if (wrq->u.essid.pointer) {
			char nickbuf[IW_ESSID_MAX_SIZE + 1];
			if (copy_from_user(nickbuf, wrq->u.essid.pointer,
					   wrq->u.essid.length)) {
				ret = -EFAULT;
				break;
			}
			ret = prism2_ioctl_siwnickn(dev, NULL, &wrq->u.essid,
						    nickbuf);
		}
		break;
	case SIOCGIWNICKN:
		if (wrq->u.essid.pointer) {
			char nickbuf[IW_ESSID_MAX_SIZE + 1];
			ret = prism2_ioctl_giwnickn(dev, NULL, &wrq->u.essid,
						    nickbuf);
			if (copy_to_user(wrq->u.essid.pointer, nickbuf,
					 wrq->u.essid.length))
				ret = -EFAULT;
		}
		break;

	case SIOCGIWSPY:
		{
			char buffer[IW_MAX_SPY * (sizeof(struct sockaddr) +
						  sizeof(struct iw_quality))];
			ret = prism2_ioctl_giwspy(dev, NULL, &wrq->u.data,
						  buffer);
			if (ret == 0 && wrq->u.data.pointer &&
			    copy_to_user(wrq->u.data.pointer, buffer,
					 wrq->u.data.length *
					 (sizeof(struct sockaddr) +
					  sizeof(struct iw_quality))))
				ret = -EFAULT;
		}
		break;

	case SIOCGIWRANGE:
		{
			struct iw_range range;
			ret = prism2_ioctl_giwrange(dev, NULL, &wrq->u.data,
						    (char *) &range);
			if (copy_to_user(wrq->u.data.pointer, &range,
					 sizeof(struct iw_range)))
				ret = -EFAULT;
		}
		break;

	case SIOCSIWSENS:
		ret = prism2_ioctl_siwsens(dev, NULL, &wrq->u.sens, NULL);
		break;
	case SIOCGIWSENS:
		ret = prism2_ioctl_giwsens(dev, NULL, &wrq->u.sens, NULL);
		break;

	case SIOCGIWAPLIST:
		if (wrq->u.data.pointer) {
			char buffer[IW_MAX_AP * (sizeof(struct sockaddr) +
						 sizeof(struct iw_quality))];
			ret = prism2_ioctl_giwaplist(dev, NULL, &wrq->u.data,
						     buffer);
			if (copy_to_user(wrq->u.data.pointer, buffer,
					 (wrq->u.data.length *
					  (sizeof(struct sockaddr) +
					   sizeof(struct iw_quality)))))
				ret = -EFAULT;
		}
		break;

	case SIOCSIWMODE:
		ret = prism2_ioctl_siwmode(dev, NULL, &wrq->u.mode, NULL);
		break;
	case SIOCGIWMODE:
		ret = prism2_ioctl_giwmode(dev, NULL, &wrq->u.mode, NULL);
		break;

	case SIOCSIWPOWER:
		ret = prism2_ioctl_siwpower(dev, NULL, &wrq->u.power, NULL);
		break;
	case SIOCGIWPOWER:
		ret = prism2_ioctl_giwpower(dev, NULL, &wrq->u.power, NULL);
		break;

	case SIOCGIWPRIV:
		ret = prism2_ioctl_giwpriv(dev, &wrq->u.data);
		break;
#endif /* WIRELESS_EXT > 8 */

#if WIRELESS_EXT > 9
	case SIOCSIWTXPOW:
		ret = prism2_ioctl_siwtxpow(dev, NULL, &wrq->u.txpower, NULL);
		break;
	case SIOCGIWTXPOW:
		ret = prism2_ioctl_giwtxpow(dev, NULL, &wrq->u.txpower, NULL);
		break;
#endif /* WIRELESS_EXT > 9 */

#if WIRELESS_EXT > 10
	case SIOCSIWRETRY:
		ret = prism2_ioctl_siwretry(dev, NULL, &wrq->u.retry, NULL);
		break;
	case SIOCGIWRETRY:
		ret = prism2_ioctl_giwretry(dev, NULL, &wrq->u.retry, NULL);
		break;
#endif /* WIRELESS_EXT > 10 */

	/* not supported wireless extensions */
	case SIOCSIWNWID:
	case SIOCGIWNWID:
		ret = -EOPNOTSUPP;
		break;

	/* FIX: add support for this: */
	case SIOCSIWSPY:
		printk(KERN_DEBUG "%s unsupported WIRELESS_EXT ioctl(0x%04x)\n"
		       , dev->name, cmd);
		ret = -EOPNOTSUPP;
		break;


		/* Private ioctls (iwpriv); these are in SIOCDEVPRIVATE range
		 * if WIRELESS_EXT < 12, so better check privileges */

	case PRISM2_IOCTL_PRISM2_PARAM:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else ret = prism2_ioctl_priv_prism2_param(dev, NULL, &wrq->u,
							  (char *) &wrq->u);
		break;
#if WIRELESS_EXT >= 12
	case PRISM2_IOCTL_GET_PRISM2_PARAM:
		ret = prism2_ioctl_priv_get_prism2_param(dev, NULL, &wrq->u,
							 (char *) &wrq->u);
		break;
#endif /* WIRELESS_EXT >= 12 */

	case PRISM2_IOCTL_WRITEMIF:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else ret = prism2_ioctl_priv_writemif(dev, NULL, &wrq->u,
						      (char *) &wrq->u);
		break;

	case PRISM2_IOCTL_READMIF:
		ret = prism2_ioctl_priv_readmif(dev, NULL, &wrq->u,
						(char *) &wrq->u);
		break;

#endif /* WIRELESS_EXT <= 12 */


		/* Private ioctls (iwpriv) that have not yet been converted
		 * into new wireless extensions API */

	case PRISM2_IOCTL_INQUIRE:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else ret = prism2_ioctl_priv_inquire(dev, (int *) wrq->u.name);
		break;

	case PRISM2_IOCTL_MONITOR:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else ret = prism2_ioctl_priv_monitor(dev, (int *) wrq->u.name);
		break;

	case PRISM2_IOCTL_RESET:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else ret = prism2_ioctl_priv_reset(dev, (int *) wrq->u.name);
		break;

#ifdef PRISM2_USE_WE_TYPE_ADDR
	case PRISM2_IOCTL_WDS_ADD:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else ret = prism2_wds_add(local, wrq->u.ap_addr.sa_data, 1);
		break;

	case PRISM2_IOCTL_WDS_DEL:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else ret = prism2_wds_del(local, wrq->u.ap_addr.sa_data, 1, 0);
		break;
#else /* PRISM2_USE_WE_TYPE_ADDR */
	case PRISM2_IOCTL_WDS_ADD:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else if (wrq->u.data.pointer) {
			char addrbuf[18];
			if (copy_from_user(addrbuf, wrq->u.data.pointer, 18)) {
				ret = -EFAULT;
				break;
			}
			ret = prism2_ioctl_priv_wds(dev, 1, addrbuf);
		}
		break;

	case PRISM2_IOCTL_WDS_DEL:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else if (wrq->u.data.pointer) {
			char addrbuf[18];
			if (copy_from_user(addrbuf, wrq->u.data.pointer, 18)) {
				ret = -EFAULT;
				break;
			}
			ret = prism2_ioctl_priv_wds(dev, 0, addrbuf);
		}
		break;
#endif /* PRISM2_USE_WE_TYPE_ADDR */

	case PRISM2_IOCTL_SET_RID_WORD:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else ret = prism2_ioctl_priv_set_rid_word(dev,
							  (int *) wrq->u.name);
		break;

#ifndef PRISM2_NO_KERNEL_IEEE80211_MGMT
	case PRISM2_IOCTL_MACCMD:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else ret = ap_mac_cmd_ioctl(local, (int *) wrq->u.name);
		break;

#ifdef PRISM2_USE_WE_TYPE_ADDR
	case PRISM2_IOCTL_ADDMAC:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else ret = ap_control_add_mac(&local->ap->mac_restrictions,
					      wrq->u.ap_addr.sa_data);
		break;
	case PRISM2_IOCTL_DELMAC:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else ret = ap_control_del_mac(&local->ap->mac_restrictions,
					      wrq->u.ap_addr.sa_data);
		break;
	case PRISM2_IOCTL_KICKMAC:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else ret = ap_control_kick_mac(local->ap, local->dev,
					       wrq->u.ap_addr.sa_data);
		break;
#else /* PRISM2_USE_WE_TYPE_ADDR */
	case PRISM2_IOCTL_ADDMAC:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else if (wrq->u.data.pointer) {
			char addrbuf[18];
			if (copy_from_user(addrbuf, wrq->u.data.pointer, 18)) {
				ret = -EFAULT;
				break;
			}
			ret = ap_mac_ioctl(local, addrbuf, AP_CTRL_MAC_ADD);
		}
		break;

	case PRISM2_IOCTL_DELMAC:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else if (wrq->u.data.pointer) {
			char addrbuf[18];
			if (copy_from_user(addrbuf, wrq->u.data.pointer, 18)) {
				ret = -EFAULT;
				break;
			}
			ret = ap_mac_ioctl(local, addrbuf, AP_CTRL_MAC_DEL);
		}
		break;

	case PRISM2_IOCTL_KICKMAC:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else if (wrq->u.data.pointer) {
			char addrbuf[18];
			if (copy_from_user(addrbuf, wrq->u.data.pointer, 18)) {
				ret = -EFAULT;
				break;
			}
			ret = ap_mac_ioctl(local, addrbuf, AP_CTRL_MAC_KICK);
		}
		break;
#endif /* PRISM2_USE_WE_TYPE_ADDR */
#endif /* PRISM2_NO_KERNEL_IEEE80211_MGMT */


		/* Private ioctls that are not used with iwpriv;
		 * in SIOCDEVPRIVATE range */

#if defined(PRISM2_DOWNLOAD_SUPPORT) && WIRELESS_EXT > 8
	case PRISM2_IOCTL_DOWNLOAD:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
		else ret = prism2_ioctl_priv_download(local, &wrq->u.data);
		break;
#endif /* PRISM2_DOWNLOAD_SUPPORT and WIRELESS_EXT > 8 */

	case PRISM2_IOCTL_HOSTAPD:
		if (!capable(CAP_NET_ADMIN)) ret = -EPERM;
#if WIRELESS_EXT > 8
		else ret = prism2_ioctl_priv_hostapd(local, &wrq->u.data);
#else /* WIRELESS_EXT > 8 */
		else ret = prism2_ioctl_priv_hostapd(
			local, (struct iw_point *) &wrq->u.data);
#endif /* WIRELESS_EXT > 8 */
		break;

#endif /* WIRELESS_EXT */

	default:
#if WIRELESS_EXT > 12
		if (cmd >= SIOCSIWCOMMIT && cmd <= SIOCGIWPOWER) {
			/* unsupport wireless extensions get through here - do
			 * not report these to debug log */
			ret = -EOPNOTSUPP;
			break;
		}
#endif /* WIRELESS_EXT > 12 */
		printk(KERN_DEBUG "%s unsupported ioctl(0x%04x)\n",
		       dev->name, cmd);
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}
