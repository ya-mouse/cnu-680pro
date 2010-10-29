/***********************************************************************
 *  s3c2510.c : Samsung S3c2510 ethernet driver for linux

    S3c2510 Communication Processor
	based on isa-skeleton: A network driver outline for linux.
    Written 1993-94 by Donald Becker.

	Copyright 1993 United States Government as represented by the
	Director, National Security Agency.

	This software may be used and distributed according to the terms
	of the GNU Public License, incorporated herein by reference.

*************************************************************************/

#define CONFIG_NOWRITEBACK
//#define ETHER_RXCOPY

#include "s3c2510.h"
#include <asm/uaccess.h>

static const char *version =
"s3c2510.c: Revision: 2.0 Date: 2004.Jan.02\n";
#define DRV_NAME	"eth2510"
#define DRV_VERSION	"2.0"

#if defined(CONFIG_PROC_CHIPINFO)
extern struct proc_dir_entry *proc_root_hardware;
#endif

/************************************************************************
 * Internal Function Declarations - all static
 ************************************************************************/
/* Index to functions, as function prototypes. */
int        __init s3c2510_probe( ND dev);
static int __init s3c2510_probe1( ND dev, UINT32 chan);

static int	s3c2510_open( ND dev);
static int	s3c2510_close( ND dev);
static void	s3c2510_tx_timeout( ND dev);
static int	s3c2510_send_packet( struct sk_buff *skb, ND dev);
static struct	net_device_stats	*s3c2510_get_stats( ND dev);
static void	set_multicast_list( ND dev);
static void	HardwareSetFilter( ND dev);

static void	s3c2510_Tx( int irq, void *dev_id, struct pt_regs * regs);
static void	s3c2510_Rx( int irq, void *dev_id, struct pt_regs * regs);

static int SetUpARC( ND dev, void * addr);
static int SetUpMAC(struct net_device *dev, void *addr);
static int s3c2510_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);

static ULONG SetupTxFDs( ND dev );
static ULONG SetupRxFDs( ND dev);
static void remove_skb_in_HW(ND dev);
static _INLINE_ void tx_done_check( ND dev, TxFD *txFd, unsigned int *txFrCtl);
static _INLINE_ int  rx_error_check( S3C2510_MAC *np, unsigned int status, unsigned int len);
static _INLINE_ int  allocate_buffers( ND dev);
static _INLINE_ int  release_buffers( ND dev);
static _INLINE_ int  tx_error_check(S3C2510_MAC *priv, unsigned int statusLen);

/* 8MSol_choish , aadded for NAPI */
#ifdef CONFIG_S3C2510_NAPI
static int s3c2510_poll (struct net_device *dev, int *budget);
static int rx_done_check(ND dev, S3C2510_MAC *np, unsigned int baseAddr);
static void enable_rx_and_rxnobuf_ints(struct net_device *dev);
static void disable_rx_and_rxnobuf_ints(struct net_device *dev);
#else
static int rx_done_check(ND dev, S3C2510_MAC *np, unsigned int baseAddr);
#endif

// ======================[ PROC FILESYSTEM RELATED ROUTINES ]===============

#if defined(CONFIG_PROC_CHIPINFO)
int proc_write_hardware(struct file *file, const char *buffer,
					unsigned long count, void *data)
{
	S3C2510_MAC *priv = data;

	priv->stats.rx_length_errors = 0;
	priv->stats.rx_crc_errors = 0;
	priv->stats.rx_frame_errors = 0;
	priv->stats.rx_fifo_errors = 0;
	priv->stats.rx_over_errors = 0;
	priv->stats.rx_missed_errors = 0;
	priv->stats_2510.iMRxShort = 0;
	priv->stats_2510.iRxParErr = 0;
	priv->stats_2510.iRxZERO = 0;
	priv->stats_2510.iRxLengthOver = 0;
	priv->stats_2510.iBRxFull = 0;		// not used for now

	priv->stats.tx_heartbeat_errors = 0;
	priv->stats.tx_window_errors = 0;
	priv->stats.tx_carrier_errors = 0;
	priv->stats.tx_aborted_errors = 0;
	priv->stats.collisions = 0;
	priv->stats_2510.iTxUnderflow = 0;
	priv->stats_2510.iBTxEmpty = 0;
	priv->stats_2510.iTxPar = 0;
			
	return count;
}

int proc_read_hardware(char *page, char **start, off_t offset,
					int count, int *eof, void *data)
{
	int len;
	S3C2510_MAC *priv = data;

	len = sprintf(page,      "\n===[ Rx buffer descriptor ]===\n");
	len += sprintf(page + len, "RxFrame size over : %d\n", priv->stats_2510.iRxLengthOver);
	len += sprintf(page + len, "MUFS(>1518 Bytes) : %ld\n",
		priv->stats.rx_length_errors - priv->stats_2510.iRxLengthOver - priv->stats_2510.iRxZERO);
	len += sprintf(page + len, "RxParErr          : %d\n", priv->stats_2510.iRxParErr);
	len += sprintf(page + len, "CRCErr            : %ld\n", priv->stats.rx_crc_errors);
	len += sprintf(page + len, "AlighErr          : %ld\n", priv->stats.rx_frame_errors);
	len += sprintf(page + len, "Overflow          : %ld\n", priv->stats.rx_fifo_errors - priv->stats_2510.iRxParErr);
	len += sprintf(page + len, "RxParErr          : %d\n", priv->stats_2510.iRxParErr);
	len += sprintf(page + len, "ZERO lenggth      : %d\n", priv->stats_2510.iRxZERO);
	len += sprintf(page + len, "===[    MBRXSTAT ERROR    ]===\n");
	len += sprintf(page + len, "BRxNO             : %ld\n", priv->stats.rx_over_errors);
	len += sprintf(page + len, "BRxFull           : %d\n", priv->stats_2510.iBRxFull);	// not used for now
	len += sprintf(page + len, "Missed roll       : %ld\n", priv->stats.rx_missed_errors);
	len += sprintf(page + len, "===[       MACRXSTAT      ]===\n");
	len += sprintf(page + len, "MRxShort          : %d\n", priv->stats_2510.iMRxShort);

	len += sprintf(page + len, "\n===[ Tx buffer descriptor ]===\n");
	len += sprintf(page + len, "SQEErr            : %ld\n", priv->stats.tx_heartbeat_errors);
	len += sprintf(page + len, "LateColl          : %ld\n", priv->stats.tx_window_errors);
	len += sprintf(page + len, "NoCarr            : %ld\n", priv->stats.tx_carrier_errors);
	len += sprintf(page + len, "ParErr            : %d\n", priv->stats_2510.iTxPar);
	len += sprintf(page + len, "DeferErr          : %ld\n", priv->stats.tx_aborted_errors);
	len += sprintf(page + len, "ExColl            : %ld\n", priv->stats.collisions);
	len += sprintf(page + len, "Underflow         : %d\n", priv->stats_2510.iTxUnderflow);
	len += sprintf(page + len, "===[       BMTXSTAT       ]===\n");
	len += sprintf(page + len, "BTxEmpty          : %d\n", priv->stats_2510.iBTxEmpty);

	return len;
}
#endif

int ethernetLinkCheck(S3C2510_MAC *priv)
{
	if(priv->mii == 0)	// If switch, always linked.
		return 1;

	return (mii_link_ok(&(priv->mii_if)));
}

int phy_link_procmem(char *buf, char** start, off_t offset,
		                    int count, int *eof, void *data)
{
	ND dev = (ND) data;
	S3C2510_MAC *priv = (S3C2510_MAC *) dev->priv;
	
	int statusOnOff;
	int len = 0;

	statusOnOff = ethernetLinkCheck(priv);
	if(!statusOnOff) 
		len += sprintf(buf + len, "0\n");
	else
		len += sprintf(buf + len, "1\n");
	
	return len;
} 
    
// ======================[ MII (PHY) RELATED ROUTINES ]===============
#ifndef LINK_MON_INTERV
#define LINK_MON_INTERV 2000
#endif
#define MII_TIMEOUT (jiffies + (2*HZ))

static struct {
	unsigned int 	ioaddr;
	unsigned int 	phy_id;
	unsigned int 	tx_irq;
	unsigned int 	rx_irq;
	char 		*rx_int_name;
	char		*tx_int_name;
} board_info[ETH_MAXUNITS] __devinitdata = {
#if (ETH_MAXUNITS==1)
  {
	ETH0OFF,			// ioaddr
	MPHYHWADDR0,			// phy_id
	ETH0_TX_IRQ,			// tx_irq
	ETH0_RX_IRQ,			// rx_irq
	"eth0-Tx",			// interrupt name for tx
	"eth0-Rx",			// interrupt name for rx
  }
#else
  {
	ETH0OFF,			// ioaddr
	MPHYHWADDR0,			// phy_id
	ETH0_TX_IRQ,			// tx_irq
	ETH0_RX_IRQ,			// rx_irq
	"eth0-Tx",			// interrupt name for tx
	"eth0-Rx",			// interrupt name for rx
  },
  {
	ETH1OFF,			// ioaddr
	MPHYHWADDR1,			// phy_id
	ETH1_TX_IRQ,			// tx_irq
	ETH1_RX_IRQ,			// rx_irq
	"eth1-Tx",			// interrupt name for tx
	"eth1-Rx",			// interrupt name for rx
  }
#endif
};

static int hw_initial_set[][2]={
	{ BMTXINTEN,	0 },
	{ BMRXINTEN,	0 },

	// Assert reset & hang
	{ MACCON,	MFullDup | MReset | MHaltReq | MHaltImm },
#ifdef __BIG_ENDIAN__
	{ BDMATXCON,	BTxRS | SET_BTxNBD | SET_BTxMSL | BTxBSWAP },
	{ BDMARXCON,	BRxRS | SET_BRxNBD | BRxAlignSkip2 | BRBSWAP },		// reset the BDMA RX block
#else
	{ BDMATXCON,	BTxRS | SET_BTxNBD | SET_BTxMSL },
	{ BDMARXCON,	BRxRS | SET_BRxNBD | BRxAlignSkip2 },			// reset the BDMA RX block
#endif
	{ MACTXCON,	MTxHalt },
	{ MACRXCON,	MRxHalt | MStripCRC },
	// Release reset
	{ MACCON,	MFullDup | MHaltReq | MHaltImm },
#ifdef __BIG_ENDIAN__
	{ BDMATXCON,	SET_BTxNBD | SET_BTxMSL | BTxBSWAP },
	{ BDMARXCON,	SET_BRxNBD | BRxAlignSkip2 | BRBSWAP },		// reset the BDMA RX block
#else
	{ BDMATXCON,	SET_BTxNBD | SET_BTxMSL },
	{ BDMARXCON,	SET_BRxNBD | BRxAlignSkip2 },			// reset the BDMA RX block
#endif
	{ CAMEN,	0 },
	{ 0xffffffff,	0 }
};

static int hw_start_set[][2]={
#ifdef __BIG_ENDIAN__
	{ BDMATXCON,	BTxEN | SET_BTxNBD | SET_BTxMSL | BTxBSWAP },
	{ BDMARXCON,	BRxEN | SET_BRxNBD | BRxAlignSkip2 | BRBSWAP },
#else
	{ BDMATXCON,	BTxEN | SET_BTxNBD | SET_BTxMSL },
	{ BDMARXCON,	BRxEN | SET_BRxNBD | BRxAlignSkip2 },
#endif
	{ MACTXCON,	MTxEn },
	{ MACRXCON,	MRxEn | MStripCRC },
	{ MACCON,	MFullDup },
	{ BMTXINTEN,	TxCFcompIE | BTxEmptyIE },
	{ BMRXINTEN,	BRxDoneIE | BRxFullIE | BRxNOIE | MissRollIE },
	{ 0xffffffff,	0 }
};

static _INLINE_ int PHYWait( unsigned int baseAddr)
{
	unsigned int i = 0;

	while (readl(baseAddr + STACON) & MPHYbusy)
	{
		if ( i++ >= PHY_BUSYLOOP_COUNT)
		{
			printk( "S3C2510-PHY: *** BUSY Timeout! ***\n");
			return( S3C2510_TIMEOUT);
		}
	}
	return( S3C2510_SUCCESS);
}

unsigned int switch_phy_emulation[32] = {
	BMCR_FULLDPLX + BMCR_SPEED100,			// BMCR register
	BMSR_LSTATUS  + BMSR_100FULL + BMSR_10FULL,	// BMSR register
	ADVERTISE_FULL,					// ADVERTISEMENT register
	LPA_100FULL,					// Link partner ability register
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static _INLINE_ int mdio_read(struct net_device *dev, int phy_id, int reg_num)
{
	S3C2510_MAC *priv = (S3C2510_MAC *) dev->priv;
	//int baseAddr = dev->base_addr;
	int baseAddr = ETH0OFF;			// 2510 has just 1 STA/

	if (!priv->mii)
		return switch_phy_emulation[reg_num];

	if ( S3C2510_SUCCESS != PHYWait( baseAddr ))
		return S3C2510_TIMEOUT;

	writel(MPHYbusy | ((phy_id & 0x1f) << 5) | (reg_num & 0x1f), baseAddr + STACON);
	if ( S3C2510_SUCCESS != PHYWait( baseAddr ))
		return S3C2510_TIMEOUT;
#if PHY_DEBUG
	{
		unsigned int data;
		data = readl(baseAddr + STADATA);
		printk( "S3C2510-PHY: read 0x%08x from PHY%d register 0x%x\n",
					(int) data, (int) phy_id, (int) reg_num);
		return data;  	/* get data */
	}
#else
	return readl(baseAddr + STADATA);  	/* get data */
#endif
}

static _INLINE_ void mdio_write(struct net_device *dev, int phy_id, int reg_num, int val)
{
	S3C2510_MAC *priv = (S3C2510_MAC *) dev->priv;
	//int baseAddr = dev->base_addr;
	int baseAddr = ETH0OFF;		// 2510 has just 1 STA/
	
	if (!priv->mii)
		return;
		
	if ( S3C2510_SUCCESS != PHYWait( baseAddr ))
		return;

	writel(val, baseAddr + STADATA);
	writel(MPHYwrite | MPHYbusy | ((phy_id & 0x1f) << 5) | (reg_num & 0x1f), baseAddr + STACON);
#if PHY_DEBUG
	printk( "S3C2510-PHY: wrote 0x%08x to PHY%d register 0x%x\n",
		(int) val, (int) phy_id, (int) reg_num);
#endif
}

static void mii_watchdog(struct net_device *dev)
{
	S3C2510_MAC *priv = (S3C2510_MAC *) dev->priv;
	int baseAddr = dev->base_addr;
	
	/* Print the link status if it has changed */
	if(mii_check_media(&(priv->mii_if), 1, 0)){
		if(priv->mii_if.full_duplex)
			writel(MCtlSETUP, baseAddr + MACCON);			// Full duplex
		else
			writel(MCtlSETUP & ~MFullDup, baseAddr + MACCON);	// Half duplex
	}
	mod_timer(&(priv->mii_timer), MII_TIMEOUT);
}

static _INLINE_ void mii_start(struct net_device *dev)
{
	S3C2510_MAC *priv = (S3C2510_MAC *) dev->priv;
	int baseAddr = dev->base_addr;

	if(priv->mii){
		init_timer( &priv->mii_timer );
		priv->mii_timer.data = (unsigned long)dev;
		priv->mii_timer.function = (void *) &mii_watchdog;
		
		if(mii_check_media(&(priv->mii_if), 1, 1)){
			if(priv->mii_if.full_duplex){
				writel(MCtlSETUP, baseAddr + MACCON);			// Full duplex
			} else {
				writel(MCtlSETUP & ~MFullDup, baseAddr + MACCON);	// Half duplex
			}
		}
		mod_timer(&(priv->mii_timer), MII_TIMEOUT);
	}
}

static void __devinit mii_initial_setup(struct net_device *dev, unsigned int phy_id)
{
	S3C2510_MAC *priv = (S3C2510_MAC *) dev->priv;

	if(phy_id <= 0x1f){
		priv->mii = 1;
		printk("%s: Auto-negotiation\n", dev->name);

	} else {	// devices like switch
#if 0  /* coreyliu 20040407, add work around for ICPLUS 175C switch */
{
	int val;
               priv->mii= 1;
               val = mdio_read(dev,29,22);
               val |= 1<<5;
	       val |= 1<<10;
	       mdio_write(dev,29,22,val); 	  
}
#endif

		priv->mii = 0;
		priv->mii_if.force_media = 1;
		priv->mii_if.advertising = ADVERTISE_100FULL;
		printk("%s: 100Mbps, Full-duplex fixed\n", dev->name);
	}
	priv->mii_if.dev         = dev;
	priv->mii_if.full_duplex = 1;
	priv->mii_if.mdio_read   = mdio_read;
	priv->mii_if.mdio_write  = mdio_write;
	priv->mii_if.phy_id      = phy_id;
	priv->mii_if.reg_num_mask= 0x1f;
	priv->mii_if.phy_id_mask = 0x1f;
}

static int netdev_ethtool_ioctl (struct net_device *dev, void *useraddr)
{
	S3C2510_MAC *np = (S3C2510_MAC *) dev->priv;
	u32 ethcmd;
	

	if (copy_from_user (&ethcmd, useraddr, sizeof (ethcmd)))
		return -EFAULT;

	switch (ethcmd) {
	case ETHTOOL_GDRVINFO: {
		struct ethtool_drvinfo info = { ETHTOOL_GDRVINFO };
		strcpy (info.driver, DRV_NAME);
		strcpy (info.version, DRV_VERSION);
		sprintf(info.bus_info, "addr: 0x%x, irq: %d",
				(unsigned int)dev->base_addr, (unsigned int)dev->irq);
		if (copy_to_user (useraddr, &info, sizeof (info)))
			return -EFAULT;
		return 0;
	}

	/* get settings */
	case ETHTOOL_GSET: {
		struct ethtool_cmd ecmd = { ETHTOOL_GSET };
		spin_lock_irq(&np->lock);
		mii_ethtool_gset(&np->mii_if, &ecmd);
		spin_unlock_irq(&np->lock);
		if (copy_to_user(useraddr, &ecmd, sizeof(ecmd)))
			return -EFAULT;
		return 0;
	}
	/* set settings */
	case ETHTOOL_SSET: {
		int r;
		struct ethtool_cmd ecmd;
		if (copy_from_user(&ecmd, useraddr, sizeof(ecmd)))
			return -EFAULT;
		spin_lock_irq(&np->lock);
		r = mii_ethtool_sset(&np->mii_if, &ecmd);
		spin_unlock_irq(&np->lock);
		return r;
	}
	/* restart autonegotiation */
	case ETHTOOL_NWAY_RST: {
		return mii_nway_restart(&np->mii_if);
	}
	/* get link status */
	case ETHTOOL_GLINK: {
		struct ethtool_value edata = {ETHTOOL_GLINK};
		edata.data = mii_link_ok(&np->mii_if);
		if (copy_to_user(useraddr, &edata, sizeof(edata)))
			return -EFAULT;
		return 0;
	}			    
	default:
		break;
	}

	return -EOPNOTSUPP;
}

static int netdev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	S3C2510_MAC *np = (S3C2510_MAC *) dev->priv;
	struct mii_ioctl_data *data = (struct mii_ioctl_data *)&rq->ifr_data;
	unsigned int mii_duplex_prev;
	int rc;

	mii_duplex_prev = np->mii_if.full_duplex;

	/* ethtool commands */
	if (cmd == SIOCETHTOOL){
		rc = netdev_ethtool_ioctl(dev, (void *) rq->ifr_data);
	}
	//storm 2004.02.11 
	else if(cmd == SIOCDEVPRIVATE + 9){
        	unsigned long data=0,status=0;
        	int i;
 	        for ( i = 0; i < 2; i++) 
		    {
	            if ( ( status = ethernetLinkCheck( np ) ) == 1 )
			{
			break;
			}
			mdelay( PHY10MSDELAY);	// 10 mS
		    }
		
		if ( status == 1)
                 	data = 1;
		
	       	copy_to_user (rq->ifr_data, &data, sizeof (unsigned long));
	}
	/* all other ioctls (the SIOC[GS]MIIxxx ioctls) */
	else {
		spin_lock_irq(&np->lock);
		rc = generic_mii_ioctl(&np->mii_if, data, cmd, NULL);
		spin_unlock_irq(&np->lock);
	}
	
	/* check duplex change */
	if(mii_duplex_prev != np->mii_if.full_duplex){
		if(np->mii_if.full_duplex){
			writel(MCtlSETUP, dev->base_addr + MACCON);		// Full duplex
		} else {
			writel(MCtlSETUP & ~MFullDup, dev->base_addr + MACCON);	// Half duplex
		}
	}
	return rc;
}

// ======================[ HW RELATED ROUTINES ]===============
//
// 
static int SetUpMAC(struct net_device *dev, void *addr){	
	unsigned char *p ;        
	p = ((struct sockaddr *)addr)->sa_data;	
	SetUpARC(dev,p);	
	return 0;
} 
/*************************************************************************
 * Function Name: SetUpARC
 *
 * Purpose:	Setup the ARC memory MAC HW address...
 *			- NOTE Only sets up one entry - 3rd entry
 *					- 0 and 1 and 18 are reserved for MAC Control packets
 *
 * Parameters:	basePort    - base address of the MAC register set
 *				addr
 *			    - pointer to array of 6 bytes for the MAC HW 
 *				address
 **************************************************************************/
static int
SetUpARC( ND dev, void * addrin)
{
	unsigned int baseAddr = dev->base_addr;
	unsigned int save, temp;
	unsigned char *addr = (unsigned char *)addrin;
	
	/* NOTE: This routine assumes little endian setup of UINT32 */
	/* don't update while active */
	save = readl(baseAddr + CAMEN);
	writel(0x0, baseAddr + CAMEN);

 	/* Enter Address at third entry - hard code */
	temp = readl(baseAddr + CAM + 4*4);
	temp = (temp & 0x0000ffff) | (addr[4]<<24) | (addr[5]<<16);
	writel((addr[0]<<24) | (addr[1] << 16) | (addr[2] << 8) | addr[3] , baseAddr + CAM + 3*4);
	writel(temp, baseAddr + CAM + 4*4);

	/* Enable Third Address - b2 */
	writel((save | (1<<2)), baseAddr + CAMEN);

	// Make it accept anything within dest
	writel((readl(baseAddr + CAMCON)) | MCompEn, baseAddr + CAMCON);

	memcpy( dev->dev_addr, addr, NUM_HW_ADDR);
	
	return ( 0);
}

static void initial_CAM_Contents(ND dev)
{
	unsigned int i;
	
	for (i = 0; i < 32; i++){
		writel(0xffffffff, dev->base_addr + CAM + i*4 );
	}
}

static void hw_init( ND dev)
{
	unsigned int i;
	
	/* Disable Interrupts in point of HW */
	s3c2510_mask_irq( dev->irq );;
	s3c2510_mask_irq( dev->irq + 1 );

	for (i=0; hw_initial_set[i][0] != 0xffffffff ; i++){
		writel(hw_initial_set[i][1], dev->base_addr + hw_initial_set[i][0] );
 	} 
	initial_CAM_Contents(dev);
}

static void hw_start( ND dev )
{
	unsigned int i;

	hw_init(dev);
	for (i=0; hw_start_set[i][0] != 0xffffffff ; i++){
		writel(hw_start_set[i][1], dev->base_addr + hw_start_set[i][0] );
 	} 
	s3c2510_unmask_irq( dev->irq);;
	s3c2510_unmask_irq( dev->irq + 1);
}
// ======================[ OS RELATED ROUTINES ]===============

/************************************************************************
 * This is the real probe routine. Linux has a history of friendly device
 * probes on the ISA bus. A good device probes avoids doing writes, and
 * verifies that the correct device exists and functions.
 * -- This version is hard coded to just find only the hard coded addresses
 *		It will only be called NUM_HW_ADDR times.
 * NOTE: now gets HW address from external routine. If there is no
 *  bootloader, then this routine must still return a HW address
 ************************************************************************/
static int __init 
s3c2510_probe1( ND dev, UINT32 chan)
{
	int rc;						//return code
        /*storm/2004.02.10/get MAC address from /mtd/0  */
	char *	ptr;
	char	*eth0_mac =(char *)0x80200000-7;  
	char	*eth1_mac =(char *)0x80200000-13;
	  
	if (chan == 0)
		printk( KERN_INFO "%s", version);

	dev->irq       = board_info[chan].tx_irq;	// just put in tx (rx is plus 1)
	dev->base_addr = board_info[chan].ioaddr;

	if ((rc = request_irq(dev->irq, &s3c2510_Tx, 
			SA_INTERRUPT, board_info[chan].rx_int_name, dev)))
	{
		printk( KERN_WARNING "%s: unable to get IRQ %d (irqval=%d).\n",
			dev->name, dev->irq, rc);
		return ( -EAGAIN);
	}
	if ((rc = request_irq(dev->irq + 1, &s3c2510_Rx, 
			SA_INTERRUPT, board_info[chan].tx_int_name, dev)))
	{
		printk( KERN_WARNING "%s: unable to get IRQ %d (irqval=%d).\n",
			dev->name, dev->irq + 1, rc);

		free_irq( dev->irq, dev);	// free the one we got
		return ( -EAGAIN);
	}
	if (!request_mem_region( dev->base_addr, MAC_IO_EXTENT, dev->name))
	{
		printk( KERN_WARNING "%s: unable to get memory/io address region %lx\n",
			dev->name, dev->base_addr);
		free_irq( dev->irq, dev);
		free_irq( dev->irq, dev);
		return (-EAGAIN);
	}

	if((dev->priv = kmalloc( sizeof( S3C2510_MAC), GFP_KERNEL)) == NULL)
	{
		printk( KERN_WARNING "%s: unable to get memory for private data %lx\n",
			dev->name, sizeof(S3C2510_MAC));

		release_mem_region(dev->base_addr, MAC_IO_EXTENT);
		free_irq( dev->irq, dev);
		free_irq( dev->irq, dev);
		return ( -ENOMEM);
	}
	disable_irq( dev->irq );
	disable_irq( dev->irq + 1 );
	hw_init(dev);

	dev->open		= &s3c2510_open;
	dev->stop		= &s3c2510_close;
	dev->hard_start_xmit	= &s3c2510_send_packet;
	dev->get_stats		= &s3c2510_get_stats;
	dev->set_multicast_list = &set_multicast_list;
	dev->do_ioctl		= &netdev_ioctl;

	dev->tx_timeout		= &s3c2510_tx_timeout;
	dev->watchdog_timeo	= MY_TX_TIMEOUT;
        //storm /2004.0210
        dev->set_mac_address	= &SetUpMAC;
	/* 8MSol_choish , added for NAPI */
#ifdef CONFIG_S3C2510_NAPI
	dev->poll 		= s3c2510_poll;
	dev->weight 	= RX_NUM_FD;
#endif
	ether_setup( dev);

        /*storm/2004.02.10/get MAC address from /mtd/0  */
        if (chan==0)  	
   	  ptr = eth0_mac;  
        else  	
	  ptr = eth1_mac;
	
	memset( dev->priv, 0, sizeof( S3C2510_MAC));

        /*storm/2004.02.10/get MAC address from /mtd/0  */
//	memcpy( dev->dev_addr, (char *) get_MAC_address(dev->name), NUM_HW_ADDR);
	memcpy( dev->dev_addr, ptr, NUM_HW_ADDR);
	mii_initial_setup(dev, board_info[chan].phy_id);
	spin_lock_init( &((S3C2510_MAC *) dev->priv)->lock);

	printk( KERN_INFO "%s: found at %08lx\n", dev->name, dev->base_addr);
	return( S3C2510_SUCCESS );
}

/************************************************************************
 * Check for a network adaptor of this type, and return '0' iff one exists.
 *  - we just allocate them the number of times I'm called
 *  NOTE: we ignore the following values for base_addr 
 * If dev->base_addr == 0, probe all likely locations.
 * If dev->base_addr == 1, always return failure.
 * If dev->base_addr == 2, allocate space for the device and return success
 * (detachable devices only).
 ************************************************************************/
int __init 
s3c2510_probe( ND dev)
{
	int i;

	for(i=0; i < ETH_MAXUNITS; i++){
		if(check_mem_region( board_info[i].ioaddr, MAC_IO_EXTENT))
			continue;
		if(s3c2510_probe1(dev, i) == 0)
			return 0;
	}
	return (-ENODEV);
}

static _INLINE_ int
s3c2510_cleanup( ND dev)
{
	free_irq( dev->irq, dev);
	free_irq( dev->irq + 1, dev);
	release_mem_region( dev->base_addr, MAC_IO_EXTENT);
	if(dev->priv)
		kfree( dev->priv);

	unregister_netdev( dev);
	return ( S3C2510_SUCCESS);
}

/************************************************************************
 * Open/initialize the board. This is called (in the current kernel)
 * sometime after booting when the 'ifconfig' program is run.
 *
 * This routine should set everything up anew at each open, even
 * registers that "should" only need to be set once at boot, so that
 * there is non-reboot way to recover if something goes wrong.
 ***********************************************************************/
static int
s3c2510_open( ND dev)
{
	S3C2510_MAC *np = (S3C2510_MAC *) dev->priv;

	allocate_buffers( dev);	// initialize the Mac Private structure

	hw_start(dev);
	enable_irq( dev->irq);
	enable_irq( dev->irq + 1);
	mii_start(dev);

	netif_start_queue( dev);

	/* proc intert for LinkCheck */
	create_proc_read_entry("EthernetLink", 0, NULL, phy_link_procmem, (void *)dev);
#if defined(CONFIG_PROC_CHIPINFO)
	np->proc = create_proc_entry(dev->name, 0644, proc_root_hardware);
	if(np->proc != NULL){
		np->proc->read_proc = proc_read_hardware;
		np->proc->write_proc = proc_write_hardware;
		np->proc->data = dev->priv;
	}
#endif
	MOD_INC_USE_COUNT;
	return S3C2510_SUCCESS;
}

/************************************************************************
 * The inverse routine to net_open().
 * - NOTE: Sections have been '#if 0' out to make the open / close pairs 
 *	 work (ifconfig up / down).
 ************************************************************************/
static int
s3c2510_close( ND dev)
{
	S3C2510_MAC *np = dev->priv;

	netif_stop_queue( dev);
	if(np->mii)
		del_timer_sync(&np->mii_timer);

	hw_init(dev);
	release_buffers(dev);

	remove_proc_entry("EthernetLink", 0);
#if defined(CONFIG_PROC_CHIPINFO)
	remove_proc_entry(dev->name, proc_root_hardware);
#endif
	MOD_DEC_USE_COUNT;
	return 0;
}

/************************************************************************
 This will only be invoked if your driver is _not_ in XOFF state.
 * What this means is that you need not check it, and that this
 * invariant will hold if you make sure that the netif_*_queue()
 * calls are done at the proper times.
 ************************************************************************/
static int s3c2510_send_packet( struct sk_buff *skb, ND dev)
{
	S3C2510_MAC *np = dev->priv;
	TxFD    *txFd = np->txFd;
	unsigned int *txFrCtl = np->txFrCtl;

	unsigned int baseAddr = dev->base_addr;
	unsigned int index;
	unsigned int temp;
	unsigned long flags;
	
	/* If some error occurs while trying to transmit this
	* packet, you should return '1' from this function.
	* In such a case you _may not_ do anything to the
	* SKB, it is still owned by the network queueing
	* layer when an error is returned.  This means you
	* may not modify any SKB fields, you may not free
	* the SKB, etc.
	*/

	/* This is the most common case for modern hardware.
	* The spinlock protects this code from the TX complete
	* hardware interrupt handler.  Queue flow control is
	* thus managed under this lock as well.
	*/

// tshwang_begin 2005/05/26 for ethernet throughput
//    spin_lock_irqsave(&np->lock, flags);
//    tx_done_check(dev, txFd, txFrCtl);
// tshwang_end 2005/05/26 for ethernet throughput

	index = np->tx_ready;

// Radicalis_hans_begin (04.01.14) - to prevent tx memory leak and tx timeout
//	if(!((txFd[index].FDStatLen) & (OWNERSHIP16<<16))){
	if(txFrCtl[index] == NULL){
// Radicalis_hans_begin (04.01.14) - to prevent tx memory leak and tx timeout
#ifdef CONFIG_NOWRITEBACK
		txFd[index].FDDataPtr = skb->data - 2;		// Check the -2
#else
		if(skb->data >= DMA_START){
			txFd[index].FDDataPtr = skb->data - 2;	// Check the -2
		}else{
			txFd[index].FDDataPtr = (np->txBuffer + index);
			memcpy( txFd[index].FDDataPtr, skb->data - 2, skb->len + 2);	
		}
#endif

#if 0
	{	
        int i=0;
        printk("\n%s:S3C2510_SEND: Length %d\n; The head of skb: fist 16 byte=\n",dev->name,skb->len);
	for(i=0;i<skb->len;i++)
        printk("%02X:",skb->data[i]);
        printk("\n");
	}
#endif			
		txFrCtl[index] = (unsigned int)skb;
		txFd[index].FDStatLen = ((TxWidSet16 + OWNERSHIP16)<<16) | skb->len;
		np->tx_ready = (index + 1)%TX_NUM_FD;
		
//        printk("\n%s:S3C2510_SEND:FDStatLen=%d\n",dev->name,txFd[index].FDStatLen);

// tshwang_begin 2005/05/26 for ethernet throughput	
        spin_lock_irqsave(&np->lock, flags);
// tshwang_end 2005/05/26 for ethernet throughput
        temp = readl(baseAddr + BDMATXCON);
        writel(temp | BTxEN, baseAddr + BDMATXCON);
        temp = readl(baseAddr + MACTXCON);
        writel(temp | MTxEn, baseAddr + MACTXCON);
        spin_unlock_irqrestore(&np->lock, flags);
        dev->trans_start = jiffies;
        return 0;
    } else {
		// Owner is BDMA
// tshwang_begin 2005/05/26 for ethernet throughput
        spin_lock_irqsave(&np->lock, flags);
// tshwang_end 2005/05/26 for ethernet throughput
        netif_stop_queue(dev);
        temp = readl(baseAddr + BMTXINTEN);
        writel( temp | TxCompIE, baseAddr + BMTXINTEN);
        spin_unlock_irqrestore(&np->lock, flags);
        return (-1);
	}
}

/************************************************************************
 * Helper routine to collect stats on tx completion and release skbs
 * MUST be called with spin_lock already invoked
 ************************************************************************/
static _INLINE_ void
tx_done_check( ND dev, TxFD *txFd, unsigned int *txFrCtl)
{
	S3C2510_MAC *np = (S3C2510_MAC *)dev->priv;
	unsigned int index = np->tx_done;
	struct sk_buff *skb;


// Radicalis_hans_begin (04.01.14) - to prevent tx memory leak and tx timeout
#if 0
	unsigned int txPtr_HW = readl(baseAddr + BTXBDCNT); 

	while( index != txPtr_HW ){
#else
	do {
		if(!((txFd[index].FDStatLen) & (TxS_Comp << 16)))
			break;
#endif
// Radicalis_hans_end (04.01.14) - to prevent tx memory leak and tx timeout
#if (NET_DEBUG >= NET_DEBUG_ERRORS)
		if((skb = (struct sk_buff *)txFrCtl[index])!= NULL){
			tx_error_check(np, txFd[index].FDStatLen);
			dev_kfree_skb_irq(skb);
		} else {		
			printk("SKB in tx-queue is NULL\n");
		}
#else
		skb = (struct sk_buff *)txFrCtl[index];
		tx_error_check(np, txFd[index].FDStatLen);
		dev_kfree_skb_irq(skb);
#endif
		txFrCtl[index] = (unsigned int)NULL;
		txFd[index].FDDataPtr = (UCHAR *) 0;
		txFd[index].FDStatLen = (TxWidSet16 << 16);
		index = (index + 1)%TX_NUM_FD;
// Radicalis_hans_begin (04.01.14) - to prevent tx memory leak and tx timeout
#if 0
	}
#else
	} while(1);
#endif
// Radicalis_hans_end (04.01.14) - to prevent tx memory leak and tx timeout
	np->tx_done = index;
}

/***********************************************************************
 * log transmit MAC errors - status register is
 * *** NOT *** the same as the
 *		status in the FD block - juse the FD.status (multiple packets)
 * ** Coded for the FD block (txtimeout has to be modified)
 *	returns S3C2510_FAIL if error
 *			S3C2510_SUCCESS if status did not show error

 +---------------+---------------+---------------+---------------+
  Byte 3          Byte 2          Byte 1          Byte 0
 +---------------+---------------+---------------+---------------+
                        TX FRAME DESCRIPTOR
 +---------------------------------------------------------------+
 |                          Buffer Pointer	    		         |0x00
 +---------------------------+---+-------------------------------+
 |            Status         |   |                               |
 |O - - H Q D C F P L N E U X|Wid|	          Tx Length          |0x04
 +-+-+-+-+-+---------------------+-------------------------------+
  3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
  1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
      Status
 	O - Ownership 1->BDMA, 0->CPU
   H - Halted
   Q - SQEErr - Signal Quality Error
   D - Defer - Transmission of the frame was deferred
   C - Coll - Collision occurred in half-duplex - will be retried
   F - Finished - The transmission has Completed
   P - ParErr - Parity error in FIFO
   L - Late Coll - The collision occurred after 64 byte times
   N - NoCarr - No Carrier Sense was detected during MAC Tx
   E - DeferErr - Frame aborted - deferred 24284-bit times
   U - Underflow - FIFO underflow
   X - ExColl - Frame aborted - 16 consecutive collisions
      Widget
   Wid - Transmission alignment - number of bytes
*******************************************************************/
static _INLINE_ int
tx_error_check(S3C2510_MAC *priv, unsigned int statusLen)
{
	unsigned int status = (statusLen>>16) & 0xffff;
	unsigned int len = statusLen & 0xffff;

	if(0 == (status & (TxS_ExColl + TxS_LateColl + TxS_Paused + TxS_MaxDefer
//			+ TxS_Halted + TxS_NCarr + TxS_Par + TxS_Under + TxS_SQErr)))
			+ TxS_Halted + TxS_Par + TxS_Under + TxS_SQErr)))
	{
		priv->stats.tx_bytes += len;
		priv->stats.tx_packets ++;
		return ( S3C2510_SUCCESS);
	}
	priv->stats.tx_errors++;
	
	if ( status & TxS_SQErr)	priv->stats.tx_heartbeat_errors++;
	if ( status & TxS_LateColl)	priv->stats.tx_window_errors++;
	if ( status & TxS_NCarr)	priv->stats.tx_carrier_errors++;
	if ( status & TxS_ExColl)	priv->stats.collisions++;	// at least 16 collisions occurred
	if ( status & TxS_MaxDefer)	priv->stats.tx_aborted_errors++;
	if ( status & (TxS_Par + TxS_Under))
	{
		priv->stats.tx_fifo_errors++;
		if(status & TxS_Par)	priv->stats_2510.iTxPar++;
		else			priv->stats_2510.iTxUnderflow++;
	}

	return ( S3C2510_FAIL);
}

/************************************************************************
 *NOTE this needs updating
 * Rx interrupt - Frame received (others ??)
 * We have a good packet(s), get it/them out of the buffers.
 * - we take both BL and FDA from the MAC.first
 *   - move them to the front.last
 *	 - buffer is stripped off, added to skb head, skb address stored in ??
 *	- continue while MAC.first != MAC.last
 * 	- other things to check / do
 *		- ensure all four entries are set to SYS_OWNS_xx
 ************************************************************************/
static void
s3c2510_Rx( int irq, void *dev_id, struct pt_regs * regs)
{
	ND dev = dev_id;
	S3C2510_MAC *np = (S3C2510_MAC *)dev->priv;
	unsigned int baseAddr = dev->base_addr;
	unsigned int RxStat   = readl(baseAddr + BMRXSTAT);
	unsigned int MacRxStat = readl(baseAddr + MACRXSTAT);
	unsigned int temp;

/* 8MSol_choish , added for NAPI */
#ifdef CONFIG_S3C2510_NAPI
//  int first;  // tshwang_begin 2005/05/26 for ethernet throughput
#endif
/* 8MSol_choish , added for NAPI */

#ifdef CONFIG_S3C2510_NAPI
	writel(RxStat & ~(BRxDone | BRxFull), baseAddr + BMRXSTAT);
#else
	writel(RxStat, baseAddr + BMRXSTAT);	// choish modified for SMDK2510 (Clear interrupt)
#endif
#if (NET_DEBUG >= NET_DEBUG_ALL)
	printk("rx int] RxStat = 0x%x(after reset: 0x%x), MacRxStat = 0x%x\n",
		RxStat, readl(baseAddr + BMRXSTAT), MacRxStat);
#endif
	
	if (RxStat & BRxNO)
	{
#if (NET_DEBUG >= NET_DEBUG_ERRORS_2)
		printk( "%s: Invalid Interrupt! Not owner of RX buffer\n", dev->name);
#endif
		np->stats.rx_over_errors++;
	}
	if (RxStat & MissRoll)
	{
#if (NET_DEBUG >= NET_DEBUG_ERRORS_2)
		printk( "%s: Interrupt! Missed Error Count rolled over\n",  dev->name);
#endif
		np->stats.rx_missed_errors += (readl(baseAddr + MISSCNT) & 0xffff);
	}
	if (MacRxStat & MRxShort )
	{
		np->stats_2510.iMRxShort++;
#if (NET_DEBUG >= NET_DEBUG_ERRORS_2)
		printk( "%s: Short Rx Frame - MACRXSTAT = %x, BMRXSTAT = %x\n",
				dev->name, readl(baseAddr + MACRXSTAT), RxStat);
#endif
	}
	
/* 8MSol_choish , added for NAPI */
#ifndef CONFIG_S3C2510_NAPI
	rx_done_check(dev, np, baseAddr);
#else
//  if (first && (RxStat & (BRxDone|BRxFull))) // tshwang_begin 2005/05/26 for ethernet throughput
    if (RxStat & (BRxDone|BRxFull))
    {
//      first = 0;  // tshwang_begin 2005/05/26 for ethernet throughput
        if (netif_rx_schedule_prep(dev)) {
            disable_rx_and_rxnobuf_ints(dev);
            __netif_rx_schedule(dev);
        }
        else {
            disable_rx_and_rxnobuf_ints(dev);
        }
    }
#endif
#ifndef CONFIG_S3C2510_NAPI
	temp = readl(baseAddr + BDMARXCON);
	writel(temp | BRxEN, baseAddr + BDMARXCON);
	temp = readl(baseAddr + MACRXCON);
	writel(temp | MRxEn, baseAddr + MACRXCON);
#endif
	return;
}

/* 8MSol_choish, added for NAPI */
// #ifndef CONFIG_S3C2510_NAPI
static int rx_done_check(ND dev, S3C2510_MAC *np, unsigned int baseAddr)
{
	RxFD *rxFd = np->rxFd;
	unsigned int *rxFrCtl = np->rxFrCtl;
	struct sk_buff *skb;

	unsigned int index = np->rx_done;
	int status, len;
	int received = 0;
  
#ifdef REFILL_RXSKB_GROUP
// Radicalis_hans_begin (04.01.14) - to prevent RX IRQ locking
#if 0
	unsigned int rxPtr_HW = readl(baseAddr + BRXBDCNT); 

	while( index != rxPtr_HW ){
		status = (rxFd[index].FDStatLen >> 16) & 0xffff;
		len = rxFd[index].FDStatLen & 0xffff;

		if(status & RxS_Done){
			skb = (struct sk_buff *)rxFrCtl[index];
#else
	for(;;){
		status=(rxFd[index].FDStatLen >> 16) & 0x7fff;	// exclude OWNERSHIP
//      if(status == NULL)  // tshwang_begin 2005/05/26 for ethernet throughput
        if((status == NULL) || (np->curr_work_limit <= 0))
            break;
		
		len = rxFd[index].FDStatLen & 0xffff;
		if(status & RxS_Done){
			skb = (struct sk_buff *)rxFrCtl[index];
#endif
// Radicalis_hans_end (04.01.14) - to prevent RX IRQ locking
#else
	for(;;){
		status = (rxFd[index].FDStatLen >> 16) & 0xffff;
		if (status & OWNERSHIP16)
			break;

		// To check __dev_alloc_skb() fail to get skbs for rx
		if((skb = (struct sk_buff *)rxFrCtl[index]) == NULL)
			goto refill_skb;
		len = rxFd[index].FDStatLen & 0xffff;
		if(status & RxS_Done){
#endif
			np->stats.rx_bytes += len;
			np->stats.rx_packets++;
#ifdef ETHER_RXCOPY
#define RXCOPY_LIMIT 2000
			if( len < RXCOPY_LIMIT){
				skb = dev_alloc_skb( PKT_BUF_SZ );
				if (skb == NULL)
				{
					printk( KERN_NOTICE "%s: Unable to allocate buffers.\n",
									dev->name);
					return ( S3C2510_FAIL);
				}
				skb->dev = dev;
				memcpy(skb->data, rxCtl[ first ].skb->data - 2, len + 2);
				skb_reserve( skb, 2);                   /* 16 byte alignment - for TCP */
				dev_kfree_skb_irq( rxCtl[ first ].skb );
			}
#endif
			skb->tail += len;
			skb->len += len;
			skb->protocol = eth_type_trans( skb, dev);
#ifndef CONFIG_S3C2510_NAPI
			netif_rx( skb);                   /* give the SKB to the kernel */
#else
            netif_receive_skb(skb);
            received++;
            np->curr_work_limit--;  // tshwang_begin 2005/05/26 for ethernet throughput
#endif
#ifndef REFILL_RXSKB_GROUP
refill_skb:
			skb = __dev_alloc_skb( PKT_BUF_SZ, GFP_ATOMIC | GFP_DMA );
			if(skb == NULL)
			{
				printk(KERN_NOTICE "%s: Memory squeeze.\n", dev->name);
				rxFrCtl[index] = (unsigned int)NULL;
				// ownership is still CPU, so 
				break;
			}
			skb->dev = dev;
			skb_reserve(skb, 2);	// 16-bit align
			rxFrCtl[index] = (unsigned int)skb;
			rxFd[index].FDDataPtr = skb->data;
#else
			rxFrCtl[index] = (unsigned int)NULL;
#endif
#if (NET_DEBUG >= NET_DEBUG_ALL) 
			{
				int i,j;

				printk( KERN_ERR "S3C2510_Rx: Length %d\n", skb->len);
				for ( j = 0; j < (skb->len & ~ 15); j += 16)
				{
					for ( i = j; i < j + 16; i += 2)
					{
						printk("%04x ", (UINT16) (skb->data[ i + 1 ] + (skb->data[ i ]<<8)));
					}
					printk("\n");
				}
				for ( i = (skb->len & ~ 15); i < skb->len; i += 2)
				{
					printk("%04x ", (UINT16) (skb->data[ i + 1 ] + (skb->data[ i ]<<8)));
				}
				printk("\n");
			}
#endif
		}
		else
		{
			rx_error_check( np, status, len);
#if (NET_DEBUG >= NET_DEBUG_INFO)
			/* On error we'll stay with the original skb 
			 * we had an error in the packet, we're keeping the skb
			 * FDDataPtr is not NULL, that is FDDataPtr = skb->data */
			{
				int i;
				skb = rxCtl[ first ].skb;
				printk( KERN_ERR "S3C2510_Rx:PKT ERR: Skb length %d\n - data=", skb->len);
				for ( i = 0; i < skb->len; i++)
				{
					printk("%02x ", skb->data[ i]);
				}
				printk("\n");
			}
#endif
	    }

// Radicalis_hans_begin (04.01.14) - to prevent memory leak and tx timeout
//#ifndef REFILL_RXSKB_GROUP
#ifdef REFILL_RXSKB_GROUP
		rxFd[index].FDStatLen = 0;
#else
// Radicalis_hans_end (04.01.14) - to prevent memory leak and tx timeout
		rxFd[index].FDStatLen = ((OWNERSHIP16 <<16) + PKT_BUF_SZ);
#endif
		index = (index + 1)%RX_NUM_FD;
    }
    np->rx_done = index;
	
#ifdef REFILL_RXSKB_GROUP
    if((len = (index - np->rx_skb_refill)) < 0)
        len += RX_NUM_FD;

#if 0	// Radicalis_hans_begin (04.01.14) - to prevent memory leak and tx timeout
	if(len > (RX_NUM_FD/4))
#else
    if((len > (RX_NUM_FD/4)) ||	// get the cache effect of locality
        (len == 0))	// done == rx_skb_refill
#endif	// Radicalis_hans_end (04.01.14) - to prevent memory leak and tx timeout
    rx_refill_skb(dev, np, rxFd, rxFrCtl);
#endif
	return received;
}
// #endif // CONFIG_S3C2510_NAPI

/***********************************************************************
 * log receive BDMA errors
 *	returns S3C2510_FAIL if error
 *			S3C2510_SUCCESS if status did not show error
 * - status passed should be from the F Descriptor

 +---------------+---------------+---------------+---------------+
  Byte 3          Byte 2          Byte 1          Byte 0
 +---------------+---------------+---------------+---------------+
                        RX FRAME DESCRIPTOR
 +---------------------------------------------------------------+
 |                          Buffer Pointer	    		         |0x00
 +---------+---------------------+-------------------------------+
 |  Flags  |      Status         |                               |
 |O B S E D|M H 1 D P U O C A - -|	          Frame Length       |0x04
 +-+-+-+-+-+---------------------+-------------------------------+
  3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
  1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
      FLAGS
 	O - Ownership 1->BDMA, 0->CPU
   B - Skip BD 
   S - SOF - First BD for a frame
   S - EOF - Last BD for a frame
   D - Done (set on first BD when frame finished)
      Status
   M - MSO - bigger than max size
   H - Halted
   1 - Rx'd over 10 Mbps interface
   D - BRxDone
   U - Parity Error
   O - Overflow in FIFO
   C - CRC error
   A - Alignment error
*******************************************************************/
static _INLINE_ int 
rx_error_check( S3C2510_MAC *priv, unsigned int status, unsigned int len)
{
	priv->stats.rx_errors++;

	if (len == 0)
	{ /* This should not happen */
#if (NET_DEBUG >= NET_DEBUG_ERRORS_2)
		printk( "%s-rx: ZERO length packet Status=0x%x\n", dev->name, status);
#endif
		priv->stats.rx_length_errors++;
		priv->stats_2510.iRxZERO++;
	}
	if ( status & ( RxS_OvMax + RxS_MUFS ))		// MSO, MUFS, ZERO Length
	{
		priv->stats.rx_length_errors++;
		if(status & RxS_OvMax)	priv->stats_2510.iRxLengthOver++;
	}
	if ( status & ( RxS_CrcErr ))
		priv->stats.rx_crc_errors++;
	if ( status & ( RxS_AlignErr ))
		priv->stats.rx_frame_errors++;
	if ( status & ( RxS_OFlow + RxS_RxPar ))
	{
		priv->stats.rx_fifo_errors++;
		if(status & RxS_OFlow)	priv->stats_2510.iRxOverflow++;
		else priv->stats_2510.iRxParErr++;
	}
	return ( S3C2510_FAIL);
}

#ifdef REFILL_RXSKB_GROUP
static _INLINE_ void rx_refill_skb(ND dev, S3C2510_MAC *np, RxFD *rxFd, unsigned int *rxFrCtl)
{
    unsigned int index = np->rx_skb_refill;
    // unsigned int goal  = np->rx_done;	// Radicalis_hans (04.01.14) - to prevent memory leak and tx timeout
    struct sk_buff *skb;

// Radicalis_hans_begin (04.01.14) - to prevent memory leak and tx timeout
//	while( index != goal )
    while( rxFd[index].FDStatLen == 0)
// Radicalis_hans_end (04.01.14) - to prevent memory leak and tx timeout
    {
        if(rxFrCtl[index] == NULL)
        {
            skb = dev_alloc_skb( PKT_BUF_SZ );
//          skb = __dev_alloc_skb( PKT_BUF_SZ, GFP_ATOMIC | GFP_DMA );  // tshwang
            if(skb == NULL)
            {
                printk(KERN_NOTICE "%s: Memory squeeze.\n", dev->name);
                // rxFrCtl[index] is already NULL;
                // ownership is still CPU, so 
                break;
            }
            skb->dev = dev;
            skb_reserve(skb, 2);	// 16-bit align
            rxFrCtl[index] = (unsigned int)skb;
            rxFd[index].FDDataPtr = skb->data;
        }
        rxFd[index].FDStatLen = ((OWNERSHIP16 <<16) + PKT_BUF_SZ);
        index = (index + 1)%RX_NUM_FD;
    }
    np->rx_skb_refill = index;
}
#endif

/***********************************************************************
 * Transmit timeout
 *	Kernel calls this routine when a transmit has timed out
 ***********************************************************************/
static void 
s3c2510_tx_timeout( ND dev)
{
	S3C2510_MAC *priv = dev->priv;
	unsigned int baseAddr = dev->base_addr;

	printk(KERN_ERR "%s: transmit timed out\n", dev->name);
	printk(KERN_ERR "BDMATXCON=%x, MACCON=%x, MACTXCON=%x, BMTXSTAT=%x,MACTXSTAT=%x\n",
		readl(baseAddr + BDMATXCON), readl(baseAddr + MACCON), readl(baseAddr + MACTXCON),
		readl(baseAddr + BMTXSTAT), readl(baseAddr + MACTXSTAT));
	
	hw_init(dev);
	
	remove_skb_in_HW(dev);
	SetupTxFDs(dev);
	SetupRxFDs(dev);

	hw_start(dev);
	netif_wake_queue(dev);

	priv->stats.tx_errors++;
	dev->trans_start = jiffies;	// reset the start time
	return;
}

/************************************************************************
 * Get the current statistics.
 * This may be called with the card or closed.
 ************************************************************************/
static struct net_device_stats *s3c2510_get_stats( ND dev)
{
	S3C2510_MAC *priv = (S3C2510_MAC *) dev->priv;
	return (&priv->stats);
}

/************************************************************************
 * Set or clear the multicast filter for this adaptor.
 * num_addrs == -1	Promiscuous mode, receive all packets
 * num_addrs == 0	Normal mode, clear multicast list
 * num_addrs > 0	Multicast mode, receive normal and MC packets,
 *			and do best-effort filtering.
 ************************************************************************/
static void
set_multicast_list( ND dev)
{
	int baseAddr = dev->base_addr;

	writel(0, baseAddr + CAMEN);	// turn off ARC usage for now

	if (dev->flags & IFF_PROMISC){
		writel(MBroad | MGroup | MStation, baseAddr + CAMCON);
		return;
	}
	
	if ((dev->flags & IFF_ALLMULTI) || dev->mc_count > (HW_MAX_ADDRS-1))
	{
		writel(MCompEn | MBroad | MGroup, baseAddr + CAMCON);
	}
	else if (dev->mc_count)
	{
		/* Walk the address list, and load the filter */
		HardwareSetFilter( dev);
		writel((MCompEn | MBroad), baseAddr + CAMCON);
	}
	else // setup for normal
	{
		SetUpARC( dev, dev->dev_addr);
		writel((MCompEn | MBroad), baseAddr + CAMCON);
	}

	writel((readl(baseAddr + CAMEN) | (1<<2)), baseAddr + CAMEN);	    // Enable all the entries
}

/************************************************************************
 * HardwareSetFilter
 ************************************************************************/
static void HardwareSetFilter( ND dev )
{
	unsigned int baseAddr = dev->base_addr;
	struct dev_mc_list *ptr = dev->mc_list;
	
	unsigned int offset, data;
	unsigned int i = 1, enableMask = 0;

	/*
	 * NOTE: this section has never been tested
	 */
	while (ptr && (i < (HW_MAX_ADDRS - 1)))
	{
		offset = CAM + (3 + ((i*3)/2))*4;	// start from 3'rd CAM entry
		
		if ( i & 1)
		{	// odd entry starting at the third location
			data = readl(baseAddr + offset);
			writel(((0xffff0000 & data) 
				| (ptr->dmi_addr[ 0 ] << 8 )
				| (ptr->dmi_addr[ 1 ])),
				baseAddr + offset);
			writel(   (ptr->dmi_addr[ 2 ] << 24) 
				| (ptr->dmi_addr[ 3 ] << 16)
				| (ptr->dmi_addr[ 4 ] << 8) 
				| (ptr->dmi_addr[ 5 ]),
				baseAddr + offset + 4);
		} else {
			data = readl(baseAddr + offset + 4);
			writel((  (ptr->dmi_addr[ 0 ] << 24)
				| (ptr->dmi_addr[ 1 ] << 16)
				| (ptr->dmi_addr[ 2 ] << 8)
				| (ptr->dmi_addr[ 3 ])),
				baseAddr + offset);
			writel((0x0000ffff & data)
				| (ptr->dmi_addr[ 4 ] << 24 )
				| (ptr->dmi_addr[ 5 ] << 16),
				baseAddr + offset + 4);
		}
		enableMask |= (1 << ( i + 2 ));
		ptr = ptr->next;
		i++;
	}
	writel(enableMask, baseAddr + CAMEN);
	if (i != (dev->mc_count + 1))
	{
		printk("S3C2510: %s - MultiCast Count (%d) != pointers (%d)\n",
				dev->name, dev->mc_count, i);
	}
	return;
}

/* 8MSol_choish */
#ifdef CONFIG_S3C2510_NAPI
static void enable_rx_and_rxnobuf_ints(ND dev)
{
	unsigned int baseAddr = dev->base_addr;
	unsigned int RxIntEn = readl(baseAddr + BMRXINTEN);
	
	writel(RxIntEn | ( BRxDoneIE | BRxFullIE), baseAddr + BMRXINTEN );
//	CSR         *    baseAddr    = (CSR *) (dev->base_addr);
//	WriteCSR( RxIntEn, ReadCSR( RxIntEn) | ( BRxDoneIE | BRxFullIE));
}

static void disable_rx_and_rxnobuf_ints(ND dev)
{
    unsigned int baseAddr = dev->base_addr;
    unsigned int RxInten = readl(baseAddr + BMRXINTEN);
	
    writel(RxInten & ~(BRxDoneIE | BRxFullIE), baseAddr + BMRXINTEN );
//	CSR         *    baseAddr    = (CSR *) (dev->base_addr);
//	WriteCSR( RxIntEn, ReadCSR( RxIntEn)  & ~(BRxDoneIE | BRxFullIE));
}

static int s3c2510_poll (ND dev, int *budget)
{
    S3C2510_MAC *np = (S3C2510_MAC *)dev->priv;
    unsigned int baseAddr = dev->base_addr;
    unsigned int RxStat   = readl(baseAddr + BMRXSTAT);
    unsigned int temp;
    int received, done = 1;

    spin_lock_irq(&np->lock);

    // tshwang 2005/06/01 don't change this code position
    writel( RxStat, baseAddr + BMRXSTAT);
    
    tx_done_check(dev, np->txFd, np->txFrCtl);
	
    np->curr_work_limit = *budget;
    if (np->curr_work_limit > dev->quota)
        np->curr_work_limit = dev->quota;
	
    received = rx_done_check(dev, np, baseAddr);
			
    dev->quota -= received;
    *budget -= received;

    if (received >= np->curr_work_limit)
        done = 0;

    if (done) {   
        netif_rx_complete(dev);
        enable_rx_and_rxnobuf_ints(dev);

        temp = readl(baseAddr + BDMARXCON);
        writel(temp | BRxEN, baseAddr + BDMARXCON);
        temp = readl(baseAddr + MACRXCON);
        writel(temp | MRxEn, baseAddr + MACRXCON);
    }

    spin_unlock_irq(&np->lock);

    return (done ? 0 : 1);	
}
#endif /* CONFIG_S3C2510_NAPI */

/************************************************************************
 * This handles TX complete events posted by the device interrupt.
 * - includes the following
 *	- transmit of a frame completed (*** Do we want this enabled??? **)
 *	- Sent all frames (stopped by owner bit)
 *		- it would be nice to use this interrupt rather than the MAC "Send"
 *		per packet, but error status and busy cause probs 
 *		- the interrupt should be disabled serves no purpose
 *		- two parts to this, we disable the interrupt (see EnableInt )
 *		but enable the stop (no skip frame)
 *	- Interrupt on control packet after MAC sends
 *		- we don't currently send control packets, so this int is never
 *		envoked.
 *		- we will leave it enabled though
 *	- Tx Buffer Empty
 *		- leaving this off too
 ************************************************************************/
void s3c2510_Tx( int irq, void *dev_id, struct pt_regs * regs)
{
	ND dev = dev_id;
	S3C2510_MAC *np = (S3C2510_MAC *)dev->priv;
	unsigned int baseAddr = dev->base_addr;
	unsigned int status = readl(baseAddr + BMTXSTAT);

	writel(status, baseAddr + BMTXSTAT);	// Radicalis_hans++ (040114) - to prevent tx timeout
	// checks to see if there was an additional (MAC/BDMA internal) error as well
	if ( status & BTxEmpty )
	{
#if (NET_DEBUG >= NET_DEBUG_ERRORS_2)
		printk( "%s: Invalid interrupt! Transmit Buffer Empty \n", dev->name);
#endif
		np->stats_2510.iBTxEmpty++;
	} else if (status & TxCFcomp )
	{	/* we currently aren't supporting control packets */
#if (NET_DEBUG >= NET_DEBUG_ERRORS)
		printk( "%s: Invalid interrupt! Control Packet Sent \n", dev->name);
#endif
	}
	if((status & TxComp) && netif_queue_stopped(dev))
	{
// Radicalis_hans_begin (04.01.14) - to prevent tx memory leak and tx timeout
#if 0
		unsigned int txPtr_HW = readl(baseAddr + BTXBDCNT);
		unsigned int temp;
		
		if(np->tx_done != txPtr_HW){
			netif_wake_queue(dev);
			temp = readl(baseAddr + BMTXINTEN);
			writel( temp &(~TxCompIE), baseAddr + BMTXINTEN);
		}
#else
		TxFD    *txFd = np->txFd;
		unsigned int *txFrCtl = np->txFrCtl;
		unsigned int temp;

		tx_done_check(dev, txFd, txFrCtl);
		temp = readl(baseAddr + BMTXINTEN);
		writel( temp &(~TxCompIE), baseAddr + BMTXINTEN);
		netif_wake_queue(dev);
#endif
// Radicalis_hans_end (04.01.14) - to prevent tx memory leak and tx timeout
	}

	// Now we clear the interrupts
	// writel(status, baseAddr + BMTXSTAT); // Radicalis_hans-- (040114) - to prevent tx timeout
}
/*********************************************************************
 *	Function Name:  SetupTxFDs
 *
 * Purpose: For transmit frame and buffer descriptors
 *          - set up initial values for these.
 *	    	- setup queue pointers
 *			- FDS setup as a ring 
 *
 *	** Assumes called just after dev set up, and priv defined
 *
 * Inputs:  Pointer to dev structure
 *
 * Return Value:
 *	S3C2510_SUCCESS
 *	S3C2510_FAIL
 *********************************************************************/
static ULONG 
SetupTxFDs( ND dev )
{
	unsigned int i;
	S3C2510_MAC *np = dev->priv;

	/* for all the tx frames */
	for(i = 0; i < TX_NUM_FD; i++) 
	{  
		np->txFrCtl[ i ] = (unsigned int)NULL;
		np->txFd[ i ].FDDataPtr = (UCHAR *) 0;
		np->txFd[ i ].FDStatLen = (TxWidSet16<<16);
	}
	np->tx_done = 0;
	np->tx_ready =0;
	
	return ( S3C2510_SUCCESS);
}
/***********************************************************************
 * Function Name:	SetupRxFDs                              
 *                                                                          
 * Purpose: Prepare memory for receive buffers and frame descriptors
 *
 *	** Assumes called just after dev set up, and priv defined
 *
 * Inputs:  Pointer to dev structure
 *
 * Return Value:
 *	S3C2510_SUCCESS
 *	S3C2510_FAIL
 ***********************************************************************/
static ULONG SetupRxFDs( ND dev)
{
    unsigned int i;
    S3C2510_MAC *np = (S3C2510_MAC *) dev->priv;
    struct sk_buff *skb;

    for ( i = 0; i < RX_NUM_FD; i++)
    {
//tshwang  if((skb = __dev_alloc_skb( PKT_BUF_SZ, GFP_ATOMIC | GFP_DMA))==NULL){
        if((skb = dev_alloc_skb( PKT_BUF_SZ ))==NULL){
            printk( KERN_NOTICE "%s: Unable to allocate buffers.\n", dev->name);
            goto no_mem_rxFd;
        }
        skb->dev = dev;
        skb_reserve(skb, 2);	// 16-bit align
        np->rxFrCtl[i] = (unsigned int) skb;
        np->rxFd[i].FDDataPtr = skb->data;
        np->rxFd[i].FDStatLen = ((OWNERSHIP16 <<16) + PKT_BUF_SZ);
#if (NET_DEBUG >= NET_DEBUG_ALL)
        printk("np->rxFd[%d].FDDataPtr = 0x%x, skb=0x%x\n", i, np->rxFd[i].FDDataPtr, skb);
#endif
    }
    np->rx_done = 0;
#ifdef REFILL_RXSKB_GROUP
    np->rx_skb_refill = 0;
#endif
    return (S3C2510_SUCCESS);
	
no_mem_rxFd:
    for(; --i >= 0;){
        dev_kfree_skb((struct sk_buff *)(np->rxFrCtl[i]));
        np->rxFrCtl[i] = (unsigned int)NULL;
    }
    return (S3C2510_FAIL);
}

static void remove_skb_in_HW(ND dev)
{
	S3C2510_MAC *np = dev->priv;
	unsigned int i;
	
	// Tx SKBs in H/W
	if(np->txFrCtl != NULL){
		for(i = 0; i < TX_NUM_FD; i++){
			if(np->txFrCtl[i]){
				dev_kfree_skb((struct sk_buff *)(np->txFrCtl[i]));
				np->txFrCtl[i] = (unsigned int)NULL;
			}
		}
	}

	// Rx SKBs in H/W
	for(i = 0; i < RX_NUM_FD; i++){
		if(np->rxFrCtl[i]){
			dev_kfree_skb((struct sk_buff *)(np->rxFrCtl[i]));
			np->rxFrCtl[i] = (unsigned int)NULL;
		}
	}
}

/************************************************************************
 * initialize all the data in private area
 ************************************************************************/
static _INLINE_ int 
allocate_buffers( ND dev )	// initialize the Mac Private structure
{
	S3C2510_MAC *np = dev->priv;
	unsigned int baseAddr = dev->base_addr;


#ifdef CONFIG_NOWRITEBACK
#else
	if (np->txBuffer ==NULL){
		np->txBuffer = (TXPACKET*)kmalloc (sizeof(TXPACKET) * TX_NUM_FD, GFP_KERNEL | GFP_DMA);
		if ( np->txBuffer == NULL)
		{
			return ( -ENOMEM);
		}
		if((unsigned int)np->txBuffer < DMA_START)
			printk("\n\n DMA alloc error) FrameDesc =0x%x\n\n", (unsigned int)FrameDescArea);i
	}
	printk("***** txBuffer Area =0x%x\n", (unsigned int)np->txBuffer);
#endif
	// Rx Frame descriptor
	if((np->rxFd = kmalloc(sizeof(RxFD) * RX_NUM_FD, GFP_KERNEL | GFP_DMA)) == NULL)
		goto no_mem;
	// Allocate Rx Frame buffers and Setup Rx Frame descriptor
	if(SetupRxFDs(dev))
		goto no_mem;

	// Tx Frame descriptor
	if((np->txFd = kmalloc(sizeof(TxFD) * TX_NUM_FD, GFP_KERNEL | GFP_DMA)) == NULL)
		goto no_mem;
	// Setup Tx Frame descriptor
	if(SetupTxFDs(dev) )
		goto no_mem;

#if (NET_DEBUG >= NET_DEBUG_ERRORS)
	printk("%s: txCtl = 0x%x, tx descriptor = 0x%x, rx descriptor = 0x%x\n",
				dev->name, (int)np->txFrCtl, (int)np->txFd, (int)np->rxFd);
#endif
	writel( (unsigned int)np->txFd,  baseAddr + BDMATXDPTR );
	writel( 0, baseAddr + BTXBDCNT );

	writel( (unsigned int)np->rxFd,  baseAddr + BDMARXDPTR );
	writel( 0, baseAddr + BRXBDCNT );
	writel( MAX_PKT_SIZE | ((PKT_BUF_SZ) << 16), dev->base_addr + BDMARXLEN );
	return 0;

no_mem:
	release_buffers(dev);
	return (-ENOMEM);
}

static _INLINE_ int release_buffers( ND dev )
{
	S3C2510_MAC *np = dev->priv;

	remove_skb_in_HW(dev);
#ifdef CONFIG_NOWRITEBACK
#else
	if (np->txBuffer)
		kfree(np->txBuffer);
	np->txBuffer = NULL;
#endif
	if(np->rxFd)
		kfree(np->rxFd);
	if(np->txFd)
		kfree(np->txFd);

	np->txFd = NULL;
	np->rxFd = NULL;
	return 0;
}
/*
 * Local variables:
 *  compile-command:
 *	gcc -D__KERNEL__ -Wall -Wstrict-prototypes -Wwrite-strings
 *	-Wredundant-decls -O2 -m486 -c s3c2510.c.c
 *  version-control: t
 *  kept-new-versions: 5
 *  tab-width: 4
 *  c-indent-level: 2
 * End:
 */
