/***********************************************************************/
/*                                                                     */
/* Samsung s3c2510x Ethernet Driver Header                             */
/* Copyright 2002 Arcturus Networks, Inc.                              */
/*                                                                     */
/***********************************************************************/
/* s3c2510.h */
/*
 * modification history
 * --------------------
 *
 *			- Initial port to s3c2510x using s3c4530 as base
 *			- * Nomemclature change frame descriptors are now buffer
 *				descriptors - there are no frame descriptors, make the
 *				change in your mind for now
 *			- ** NOTE: Section Numbers refer to an obsoleted version
 *			  **       of the S3c2510X manual :-(
 *			- Initial version for linux - Samsung
 *			- please set TABS == 4
 *
 */
/* choish modified for 2510 */
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/if.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#ifdef CONFIG_BOARD_SMDK2510
#include <asm/arch-samsung/SMDK2510/irq.h>
#endif
#ifdef CONFIG_BOARD_2510REF
#include <asm/arch-samsung/2510REF/irq.h>
#endif
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

/****************
 * DEBUG MACROS
 ***************/
/* use 0 for production, 1 for verification, > 2 for debug */
#define NET_DEBUG_RELEASE	0
#define NET_DEBUG_ERRORS	1	// Errors which will not display when release.
#define NET_DEBUG_ERRORS_2	2	// Errors which cannot avoid
#define NET_DEBUG_WARNING	3
#define NET_DEBUG_INFO		4
#define NET_DEBUG_ALL		5

#ifndef NET_DEBUG
#define NET_DEBUG		NET_DEBUG_RELEASE
#endif

#define PHY_DEBUG		0

/***************************************
 * CONFIGURATION
 ***************************************/
#define ETH_MAXUNITS		2		// number of ethernet channels

#define TX_FD_NUM_PWR2		6		// 2^6 Frame descriptor for TX
#define RX_FD_NUM_PWR2		6		// 2^6 Frame descriptor for RX

//  If PHY setting is fixed to 100M/Full, set MPHYHWADDRx to 0xff
// as in case of switch
#ifdef PHY_ICPLUS
#define MPHYHWADDR0		1		// PHY address for eth0
#define MPHYHWADDR1		0xff		// PHY address for eth1
#else
#define MPHYHWADDR0		1		// PHY address for eth0
#define MPHYHWADDR1		2		// PHY address for eth1
#endif

// Rx SKB can be allocated whether for each rx frame
//  or for some received frames in one interrupt routine
//  The performance can be different
//  When REFILL_RXSKB_GROUP is defined:     4.1 Mbps/64bytes packet
//  When REFILL_RXSKB_GROUP is not defined: 3.87Mbps/64bytes packet 
// Radicalis_hans_begin (20040113) - To improve ethernet performance
#define REFILL_RXSKB_GROUP
// Radicalis_hans_end (20040113)

// If this is defined, the TxPktCompletion (free's tx'd packets) will be 
//  called at the end of the send_pkt routine instead of the Tx completion
//  interrupt.
#define DEFER_TX_COMP		1

// Have to make sure that destructor is always called and
//  no one ever frees the skb data before I trust this
//  would have a very hard bug to trace or replicate 
// **** This is disabled because in 2.4.17, some SKBs do NOT call the
// **** destructor routine => memory leak. Might want to turn it back
// **** on if the problem(s) are fixed -- should be MORE efficient than
// **** repeatedly deleting and creating SKB's
#undef ZERO_COPY_BUFFER



// storm 20040210
#define PHY10MSDELAY	10		// should be 10 for 10 MS delay

/***********************************
 *
 * S3c2510 Ethernet MAC Configuration
 *
 ***********************************/
#define NUM_HW_ADDR	6			// number of bytes in ethernet HW address 

#define HW_MAX_CAM	((32 * 4) / NUM_HW_ADDR) // number of entries in CAM for address
#define HW_MAX_ADDRS	(HW_MAX_CAM - 5)	// maximum usable ARC entries
						//  0, 1 and 18 reserved, we ignore 19 and 20
#define PHY_BUSYLOOP_COUNT	200

#define TX_NUM_FD	(1<<TX_FD_NUM_PWR2)
#define RX_NUM_FD	(1<<RX_FD_NUM_PWR2)
   
/* The number of low I/O ports used by the ethercard.---????? */
#define MY_TX_TIMEOUT  ((400*HZ)/1000)

/*********************
 * Error Return Codes
 *********************/
#define	S3C2510_SUCCESS		0		// function call was successful
#define S3C2510_FAIL		-1		// general error code
#define S3C2510_TIMEOUT		-1000		// timeout error

/**********************
 * S3C2510 Buffer Sizes
 **********************/
#define MAX_PKT_SIZE		1536		// bigger than need be ? (but 3x512)
#define MAX_FRAME_SIZE		MAX_PKT_SIZE - 4 // controller is four less
//#define MAX_FRAME_SIZE	1522

// Realize that this is bigger than needs to be - does this cause a problem?
//  -- the upper layers break down tx stuff, rx?? 
#define PKT_BUF_SZ  	MAX_PKT_SIZE - 16	// Size of each Rx buffer

/* Reserve all the stuff - nothing between used (each adapter has this 
 *  size from the base address of the  */
#define MAC_IO_EXTENT	(0xB0100 - 0xA0000)

/********************
 * Macro Definitions
 ********************/
#ifndef ___align16
#define ___align16 __attribute__((__aligned__(16)))
#endif 

#if NET_DEBUG > 0
// NOTE: NO panic used when in a driver - I might be in the kernel
#define ASSERT(expr) \
  if(!(expr)) \
  { \
    printk( "\n" __FILE__ ":%d: Assertion " #expr " failed!\n",__LINE__); \
  }
#define TDEBUG(x...) printk(KERN_DEBUG ## x)
#else
#define ASSERT(expr)
#define TDEBUG(x...)
#endif

// so we can source debug by nulling this	
#define _INLINE_ inline

// just some more shorthand
#define ND struct net_device *

/*********************************************************************
 * typedefs
 * - wish these were predefined somewhere else 
 *********************************************************************/
typedef unsigned int	UINT;
typedef	unsigned long	ULONG;
typedef unsigned long	UINT32;
typedef unsigned short	UINT16;
typedef unsigned char	UINT8;
typedef	unsigned char	UCHAR;

/***************************************************
 *  Ethernet MAC Base address and interrupt vectors
 ***************************************************/
// Each Mac has Two interrupts
#define ETH0_TX_IRQ		SRC_IRQ_ETH_TX0	/* TX IRQ for ETHERNET 0			*/
#define ETH0_RX_IRQ		SRC_IRQ_ETH_RX0	/* RX IRQ for ETHERNET 0			*/
#define ETH1_TX_IRQ		SRC_IRQ_ETH_TX1	/* TX IRQ for ETHERNET 1			*/
#define ETH1_RX_IRQ		SRC_IRQ_ETH_RX1	/* RX IRQ for ETHERNET 1			*/

/***********************************************
 * Ethernet MAC Register Offsets
 ***********************************************/
#define ETH0OFF		0xF00A0000	/* Offsetset for register - Ethernet 0		*/
#define ETH1OFF		0xF00C0000	/* Offsetset for register - Ethernet 1		*/

/***********************************************
 * Ethernet BDMA Register Address
 ***********************************************/
#define BDMATXCON	0x0000		/* Buffered DMA Transmit Control			*/
#define BDMARXCON	0x0004		/* Buffered DMA Receive Control				*/
#define BDMATXDPTR	0x0008		/* BDMA Transmit Frame Descriptor Start
								   Address									*/
#define BDMARXDPTR	0x000C		/* BDMA Receive Frame Descriptor Start
								   Address									*/
#define BTXBDCNT	0x0010		/* BDMA Tx buffer descriptor counter		*/
#define BRXBDCNT	0x0014		/* BDMA Rx buffer descriptor counter		*/
#define BMTXINTEN	0x0018		/* BDMA/MAC Tx Interrupt enable
								   register						  			*/
#define BMRXINTEN	0x001C		/* BDMA/MAC Rx Interrupt enable
								   register									*/
#define BMTXSTAT	0x0020		/* BDMA/MAC Tx Status register				*/
#define BMRXSTAT	0x0024		/* BDMA/MAC Rx Status register				*/
#define BDMARXLEN  	0x0028		/* BDMA Receive frame Size					*/
#define CFTXSTAT	0x0030		/* Transmit control frame status			*/
/***********************************************
 * Ethernet MAC Register Address
 ***********************************************/
#define MACCON		0x10000		/* MAC control								*/
#define CAMCON		0x10004		/* CAM control								*/
#define MACTXCON	0x10008		/* Transmit control							*/
#define MACTXSTAT	0x1000C		/* Transmit status							*/
#define MACRXCON	0x10010		/* Receive control							*/
#define MACRXSTAT	0x10014		/* Receive status							*/
#define STADATA		0x10018		/* Station management data					*/
#define STACON		0x1001C		/* Station management control and 
								   address									*/
#define CAMEN		0x10028		/* CAM enable								*/
#define MISSCNT		0x1003C		/* Missed error count						*/
#define PZCNT		0x10040		/* Pause count								*/
#define RMPZCNT		0x10044		/* Remote pause count						*/
#define CAM     	0x10080		/* CAM content 32 words 0080-00fc			*/

/*********************************************************
 * Buffered DMA Transmit Control Register Masks (7.5.1.1)
 * BDMATXCON - offset 0x0000
 *********************************************************/
#define BTxNBD		0x000f		/* BDMA Tx # of Buffer Descriptors (Power of 2)										*/
#define BTxBSWAP	0x0080		/* BDMA Tx Byte Swapping */
#define BTxMSL		0x0070		/* BDMA Tx Start Level */
#define BTxEN		0x0400		/* BDMA Tx enable NOTE: BDMATXDPTR set 1st */
#define BTxRS		0x0800		/* BDMA Tx reset - set to '1' to reset Tx */
#define SET_BTxNBD	TX_FD_NUM_PWR2	/* - 64 TX Frames Descriptors */
#define SET_BTxMSL	0x0040		/* 4/8 fill BDMA Tx Start Level */

#ifdef __BIG_ENDIAN__
#define BTxSetup	(SET_BTxNBD | SET_BTxMSL | BTxBSWAP)
#else
#define BTxSetup	(SET_BTxNBD | SET_BTxMSL)
#endif

/*********************************************************
 * Buffered DMA Receive Control Register (7-.5.1.2)
 * BDMARXCON - offset 0x0004
 *********************************************************/
#define BRxNBD		0x000f		/* BDMA Rx # of Buffer Descriptors (Power of 2) */
#define BRxBSWAP	0x0080		/* BDMA Rx Byte Swapping */
#define BRxWA		0x0030		/* BDMA Rx word alignment (0->3 bytes) */
#define BRxEN		0x0400		/* BDMA Rx Enable, BDMARXDPTR must be set */
#define BRxRS		0x0800		/* BDMA Rx reset - set to '1' to reset Tx */

#define SET_BRxNBD	RX_FD_NUM_PWR2	/* - 256 TX Frames Descriptors */
#define BRxAlignSkip0	(0x00<<4)	/* Do not skip any bytes on rx */
#define BRxAlignSkip1	(0x01<<4)	/* Skip 1st byte on rx */
#define BRxAlignSkip2	(0x02<<4)	/* Skip 2nd byte on rx */
#define BRxAlignSkip3	(0x03<<4)	/* Skip 3rd byte on rx */

#ifdef __BIG_ENDIAN__
#define	BRxSetup_RESET	(SET_BRxNBD | BRxAlignSkip2 | BRxBSWAP)
#else
#define	BRxSetup_RESET	(SET_BRxNBD | BRxAlignSkip2 )
#endif

/*********************************************************
 * BDMA Transmit Buffer Descriptor Start Address (7.5.1.3)
 * BDMATXDPTR - offset 0x0008
 *********************************************************/

/*********************************************************
 * BDMA Receive Buffer Descriptor Start Address (7.5.1.4)
 * BDMARXDPTR - offset 0x000C
 *********************************************************/

/*********************************************************
 * BDMA Transmit Buffer Counter (7.5.1.5)
 * BTXBDCNT - offset 0x0010
 *********************************************************/
#define BDMATXCNT	0x0fff	/* BDMA Tx buffer descriptor Counter,
							   Current address = BDMATXDPTR 
							   + (BTXBDCNT << 3)							*/
/*********************************************************
 * BDMA Receive Buffer Counter (7.5.1.6)
 * BRXBDCNT - offset 0x0014
 *********************************************************/
#define BDMARXCNT	0x0fff	/* BDMA Rx buffer descriptor Counter,
							   Current address = BDMARXDPTR 
							   + (BRXBDCNT << 3)							*/
/*********************************************************
 * BDMA/MAC Transmit Interrupt Enable (7.5.1.7)
 * BMTXINTEN - offset 0x0018
 *********************************************************/
#define ExCollIE	0x0001	/* Enable MAC Tx excessive collision-ExColl		*/
#define UnderflowIE	0x0002	/* Enable MAC Tx underflow-Underflow 		 	*/
#define DeferErrIE	0x0004	/* Enable MAC Tx deferral-DeferErr			 	*/
#define NoCarrIE	0x0008	/* Enable MAC Tx no carrier-NoCarr				*/
#define LateCollIE	0x0010	/* Enable MAC Tx late collision-LateColl		*/
#define TxParErrIE	0x0020	/* Enable MAC Tx transmit parity-TxParErr		*/
#define TxCompIE	0x0040	/* Enable MAC Tx completion-TxComp				*/
#define TxCFcompIE	0x10000	/* Tx complete to send control frame 
							   interrupt enable-TxCFcomp					*/
#define BTxNOIE		0x20000	/* BDMA Tx not owner interrupt enable-BTxNO		*/
#define BTxEmptyIE	0x40000	/* BDMA Tx Buffer empty interrupt enable
							   -BTxEmpty									*/
#if NO_TX_COMP_INT
#define TxINTMask	(ExCollIE | UnderflowIE | DeferErrIE | NoCarrIE \
                     | LateCollIE | TxParErrIE | TxCFcompIE \
                     | BTxNOIE | BTxEmptyIE) 
#else
#define TxINTMask	(ExCollIE | UnderflowIE | DeferErrIE | NoCarrIE \
                     | LateCollIE | TxParErrIE | TxCompIE | TxCFcompIE \
                     | BTxNOIE | BTxEmptyIE) 
#endif
/*********************************************************
 * BDMA/MAC Transmit Interrupt Status (7.5.1.8)
 * BDTXSTAT - offset 0x0020
 *********************************************************/
#define ExColl		0x0001	/* MAC Tx excessive collision-ExColl			*/
#define Underflow	0x0002	/* MAC Tx underflow-Underflow 		 			*/
#define DeferErr	0x0004	/* MAC Tx deferral-DeferErr			 			*/
#define NoCarr		0x0008	/* MAC Tx no carrier-NoCarr						*/
#define LateColl	0x0010	/* MAC Tx late collision-LateColl				*/
#define TxParErr	0x0020	/* MAC Tx transmit parity-TxParErr				*/
#define TxComp		0x0040	/* MAC Tx completion-TxComp						*/
#define TxCFcomp	0x10000	/* Tx complete to send control frame-TxCFcomp	*/
#define BTxNO		0x20000	/* BDMA Tx not owner-BTxNO						*/
#define BTxEmpty	0x40000	/* BDMA Tx Buffer empty-BTxEmpty				*/

/*********************************************************
 * BDMA/MAC Receive Interrupt Enable (7.5.1.9)
 * BDRXINTIE - offset 0x001C
 *********************************************************/
#define MissRollIE	0x0001	/* MAC Rx missed roll-MissRoll 			 		*/
#define AlignErrIE	0x0002	/* MAC Rx alignment-AlignErr					*/
#define CRCErrIE	0x0004	/* MAC Rx CRC error-CRCErr						*/
#define OverflowIE	0x0008	/* MAC Rx overflow-Overflow						*/
#define LongErrIE	0x0010	/* MAC Rx long error-LongErr					*/
#define RxParErrIE	0x0020	/* MAC Rx receive parity-RxParErr				*/
#define BRxDoneIE	0x10000	/* BDMA Rx done for every received 
							   frames-BRxDone								*/
#define BRxNOIE		0x20000	/* BDMA Rx not owner interrupt-BRxNO			*/
#define BRxMSOIE	0x40000	/* BDMA Rx maximum size over interrupt
							   -BRxMSO										*/
#define BRxFullIE	0x80000	/* BDMA Rx buffer(BRxBUFF) Overflow 
							   Interrupt-BRxFull							*/
#define BRxEarlyIE	0x100000	/* BDMA Rx early notification 
								   interrupt-BRxEarly						*/
#if 0	// don't want to handle all these 
	    // - most will done by the frame having the status bit set in the
	    // status section of the RX Frame descriptor
#define RxINTMask	(MissRollIE | AlignErrIE | CRCErrIE | OverflowIE \
                    | LongErrIE | RxParErrIE | BRxNOIE | BRxDoneIE \
                    | BRxMSOIE | BRxFullIE | BRxEarlyIE) 
#else
#define RxINTMask  ( BRxDoneIE | BRxFullIE)
#endif
/*********************************************************
 * BDMA/MAC Receive Interrupt Status (7.5.1.10)
 * BDRXSTAT - offset 0x0024
 *********************************************************/
#define MissRoll	0x0001	/* MAC Rx missed roll-MissRoll 			 		*/
#define AlignErr	0x0002	/* MAC Rx alignment-AlignErr					*/
#define CRCErr		0x0004	/* MAC Rx CRC error-CRCErr						*/
#define Overflow	0x0008	/* MAC Rx overflow-Overflow						*/
#define LongErr		0x0010	/* MAC Rx long error-LongErr					*/
#define RxParErr	0x0020	/* MAC Rx receive parity-RxParErr				*/
#define BRxDone		0x10000	/* BDMA Rx done for every received 
							   frames-BRxDone								*/
#define BRxNO		0x20000	/* BDMA Rx not owner interrupt-BRxNO			*/
#define BRxMSO		0x40000	/* BDMA Rx maximum size over interrupt
							   -BRxMSO										*/
#define BRxFull		0x80000	/* BDMA Rx buffer(BRxBUFF) Overflow 
							   Interrupt-BRxFull							*/
#define BRxEarly	0x100000	/* BDMA Rx early notification 
								   interrupt-BRxEarly						*/
#define BRxFRF		0x200000	/* An additional data frame is received
								   in the BDMA receive buffer				*/
#define BRxBUFF		0x7c00000	/* Number of frames in BRxBUFF				*/

/*********************************************************
 * BDMA Receive Frame Size (7.5.1.11)
 * BDRXSTAT - offset 0x0028
 *********************************************************/
#define BRxBS	0x00000FFF	/* This register value specifies the buffer
							   size allocated to each buffer descriptor.
							   Thus, for an incoming frame larger than
							   the BRxBS, multiple buffer descriptors
							   are used for the frame reception.
							   Note: BRxBS value has to keep multiples
							   of 16 in byte unit. For long packet 
							   reception larger than 1518 bytes, the 
							   BRxBS should be at least 4 bytes larger 
							   than the BRxMFS or less than 1518 bytes.		*/
#define BRxMFS	0x0FFF0000	/* BDMA Maximum Rx Frame Size (BRxMFS) This 
							   register value controls how many bytes 
							   per frame can be saved to memory. If 
							   the received frame size exceeds these 
							   values, an error condition is reported.
							   Note: BRxMFS value has to keep multiples 
							   of 16 in byte unit							*/

/*********************************************************
 * MAC Transmit Control Frame Status (7.5.2.1)
 * The transmit control frame status register, CFTXSTAT provides
 * the status of a MAC control frame as it is sent to a remote station.
 * This operation is controlled by the MSdPause bit in the transmit 
 * control register, MACTXCON. It is the responsibility of the BDMA 
 * engine to read this register and to generate an interrupt to notify
 * the system that the transmission of a MAC control packet has been
 * completed.
 * CFTXSTAT - offset 0x0030
 *********************************************************/
#define MACCTLTXSTAT	0xFFFF	/* A 16-bit value indicating the status of
								   a MAC control packet as it is sent to a 
								   remote station. Read by the BDMA engine.	*/

/*********************************************************
 * MAC Control Register  (7.5.2.2)
 * The MAC control register provides global control and status 
 * information for the MAC. The missed roll/link10 bit is a status 
 * bit. All other bits are MAC control bits. MAC control register 
 * settings affect both transmission and reception. After a reset 
 * is complete, the MAC controller clears the reset bit. Not all
 * PHYs support full-duplex operation. (Setting the MAC loopback
 * bit overrides the full-duplex bit.) Also, some 10-Mb/s PHYs may
 * interpret the loop10 bit to control different functions, and
 * manipulate the link10 bit to indicate a different status condition.
 * MACCON - offset 0x10000
 *********************************************************/
#define MHaltReq	0x0001	/* Halt request (MHaltReq) Set this bit to
							   stop data frame transmission and
							   reception as soon as Tx/Rx of any current
							   frames has been completed					*/
#define MHaltImm	0x0002	/* Halt immediate (MHaltImm) Set this bit
							   to immediately stop all transmission
							   and reception 								*/
#define MReset		0x0004	/*  Software reset (MReset) Set this bit
								to reset all MAC control and status
								register and MAC state machines. This
								bit is automatically cleared				*/
#define MFullDup	0x0008	/*  Full-duplex Set this bit to start
								transmission while reception is in
								progress									*/
#define MLoopBack	0x0010	/*  MAC loopback (MLoopBack) Set this bit
								to cause transmission signals to be
								presented as input to the receive
								circuit without leaving the controller		*/
#define MMIIOFF		0x0040	/*  MII-OFF Use this bit to select the
								connection mode. If this bit is set to
								one, 10 M bits/s interface will select
								the 10 M bits/s endec. Otherwise, the
								MII will be selected						*/
#define MLOOP10		0x0080	/*  Loop 10 Mb/s (MLOOP10) If this bit is
								set, the Loop_10 external signal is
								asserted to the 10-Mb/s endec				*/
#define MMDCOFF		0x1000	/* MDC-OFF Clear this bit to enable the MDC
							   clock generation for power management. If
							   it is set to one, the MDC clock
							   generation is disabled						*/
#define MLINK10		0x8000	/*  Link status 10 Mb/s (MLINK10),
								read-only This bit value is read as a
								buffered signal on the link 10 pin.			*/

#define MCtlReset_BUSY_LOOP	100 /* Number of loops to wait for MReset bit	*/
#define MCtlSETUP	(MFullDup)
#define MCtlFullDupShift	3
/*********************************************************
 * CAM Control Register (7.5.2.3
 *	CAMCON - offset 0x0x10004
 *********************************************************/
#define MStation	0x0001	/* Station accept (MStation) Set this bit
							   to accept unicast (i.e. station) frames  	*/
#define MGroup		0x0002	/* Group accept (MGroup) Set this bit to 
							   accept multicast (i.e. group) frames. 		*/
#define MBroad		0x0004	/* Broadcast accept (MBroad) Set this bit to
							   accept broadcast frames. 					*/
#define MNegCAM		0x0008	/* Negative CAM (MNegCAM) Set this bit to
							   enable the Negative CAM comparison mode. 	*/
#define MCompEn		0x0010	/* Compare enable (MCompEn) Set this bit to
							   enable the CAM comparison mode.				*/
#define CAMCtlSETUP     (MStation | MGroup | MBroad | MCompEn)

/*********************************************************
 * MAC Transmit Control Register (7.5.2.4)
 * MACTXCON - offset 0x10008
 *********************************************************/
#define MTxEn		0x0001	/* Transmit enable (MTxEn) Set this bit to 
							   enable transmission. To stop transmission 
							   immediately, clear the transmit enable
							   bit to  0									*/
#define MTxHalt		0x0002	/* Transmit halt request (MTxHalt) Set this
							   bit to halt the transmission after
							   completing the transmission of any
							   current frame								*/
#define MNoPad		0x0004	/* Suppress padding (MNoPad) Set this bit
							   not to generate pad bytes for frames of 
							   less than 64 bytes							*/
#define MNoCRC		0x0008	/* Suppress CRC (MNoCRC) Set this bit to 
							   suppress addition of a CRC at the end of 
							   a frame.										*/
#define MFBack		0x0010	/* Fast back-off (MFBack) Set this bit to 
							   use faster back-off times for testing 		*/
#define MNoDef		0x0020	/* No defer (MNoDef) Set this bit to disable
							   the defer counter. (The defer counter 
							   keeps counting until the carrier sense 
							   (CrS) bit is turned off.						*/
#define MSdPause	0x0040	/* Send Pause (MSdPause) Set this bit to 
							   send a pause command or other MAC control 
							   frame. The send pause bit is 
							   automatically cleared when a complete MAC 
							   control frame has been transmitted. 
							   Writing a  0  to this register bit has 
							   no effect									*/
#define MSQEn		0x0080	/* MII 10-Mb/s SQE test mode enable (MSQEn)
							   Set this bit to enable MII 10-Mb/s SQE
							   test mode									*/

#define MTxCtlSetup (0)		/* None of these on								*/

/*********************************************************
 * MAC Transmit Status Register (7.5.2.5)
 * A transmission status flag is set in the transmit status
 * register, MACTXSTAT, whenever the corresponding event
 * occurs. In addition, an interrupt is generated if the
 * corresponding enable bit in the transmit control register
 * is set. A MAC TxFIFO parity error sets TxParErr, and also
 * clears MTxEn, if the interrupt is enabled.
 * MACTXSTAT - offset 0x1000C
 *********************************************************/
#define MACTXERR	0x00ff	/*  These bits are equivalent to the
								BMTXSTAT.7~0								*/
#define MCollCnt	0x0f00	/* Transmission collision count (MCollCnt)
							   This 4-bit value is the count of 
							   collisions that occurred while 
							   successfully transmitting the frame			*/
#define MTxDefer	0x1000	/* Transmission deferred (MTxDefer) This
							   bit is set if transmission of a frame 
							   was deferred because of a delay during
							   transmission									*/
#define SQEErr		0x2000	/* Signal quality error (SQEErr) According
							   to the IEEE802.3 specification, the SQE
							   signal reports the status of the PMA (MAU
							   or transceiver) operation to the MAC
							   layer. After transmission is complete and
							   1.6 ms has elapsed, a collision detection
							   signal is issued for 1.5 ms to the MAC
							   layer. This signal is called the SQE test
							   signal. The MAC sets this bit if this
							   signal is not reported within the IFG
							   time of 6.4ms 								*/
#define MTxHalted	0x4000	/* Transmission halted (MTxHalted) This bit
							   is set if the MTxEn bit is cleared or the
							   MHaltImm bit is set							*/
#define MPaused		0x8000	/* Paused (MPaused) This bit is set if
							   transmission of frame was delayed due to
							   a Pause being received						*/
#define MCollShift	0x08	/* Amount to shift collisions */
/*********************************************************
 * MAC Receive Control Register (7.5.2.6)
 * MACRXCON - offset 0x10010
 *********************************************************/
#define MRxEn		0x0001	/* Receive enable (MRxEn) Set this bit to
							   1  to enable MAC receive operation. If
							   0 , stop reception immediately				*/
#define MRxHalt		0x0002	/* Receive halt request (MRxHalt) Set
							   this bit to halt reception after
							   completing the reception of any
							   current frame								*/
#define MLongEn		0x0004	/* Long enable (MLongEn) Set this bit to
							   receive frames with lengths greater
							   than 1518 bytes								*/
#define MShortEn	0x0008	/* Short enable (MShortEn) Set this bit
							   to receive frames with lengths less
							   than 64 bytes								*/
#define MStripCRC	0x0010	/* Strip CRC value (MStripCRC) Set this
							   bit to check the CRC, and then strip
							   it from the message							*/
#define MPassCtl	0x0020	/* Pass control frame (MPassCtl) Set this
							   bit to enable the passing of control
							   frames to a MAC client						*/
#define MIgnoreCRC	0x0040	/* Ignore CRC value (MIgnoreCRC) Set this
							   bit to disable CRC value checking			*/

// From usage- we enable rx interrupts later
#define MRxCtlSETUP             (MRxEn | MStripCRC)

/*********************************************************
 * MAC Receive Status Register (7.5.2.7)
 * MACRXSTAT - offset 0x10014
 *********************************************************/
#define MACRXERR	0x00ff	/* These bits are equal to the BMRXSTAT.7~		*/
#define MRxShort	0x0100	/* Short Frame Error (MRxShort) This bit is
							   set if the frame was received with short
							   frame										*/
#define MRx10Stat	0x0200	/* Receive 10-Mb/s status (MRx10Stat) This
							   bit is set to  1  if the frame was
							   received over the 7-wire interface or to
							   0  if the frame was received over the MII	*/
#define MRxHalted	0x0400	/* Reception halted (MRxHalted) This bit is
							   set if the MRxEn bit is cleared or the
							   MHaltImm bit is set							*/
#define MCtlRecd	0x0800	/* Control frame received (MCtlRecd) This
							   bit is set if the frame received is a
							   MAC control frame (type = 0x8808), if the
							   CAM recognizes the frame address, and if
							   the frame length is 64 bytes					*/

/*********************************************************
 * MAC Station Management Data Register (7.5.2.8)
 * STADATA - offset 0x10018
 *********************************************************/
#define STAMANDATA	0xffff	/* This register contains a 16-bit data
							   value for the station management
							   function.									*/

/*********************************************************
 * MAC Station Management Data Control and Address Register 
 *		(7.5.2.9)
 * The MAC controller provides support for reading and
 * writing3 station management data to the PHY. Setting
 * options in station management registers does not
 * affect the controller. Some PHYs may not support the
 * option to suppress preambles after the first operation.
 * STACON - offset 0x1001c
 *********************************************************/
#define MPHYRegAddr	0x001F	/* PHY register address (MPHYRegAddr) A
							   5-bit address, contained in the PHY,
							   of the register to be read or written		*/
#ifdef PHY_INTEL	// NOTE: FIXME should base on board type not PHY type 
#define MPHYHWADDR0	0x00 	/* PHY H/W Address 0x1							*/
#define MPHYHWADDR1	0x00 	/* PHY H/W Address 0x2							*/
#endif
#ifdef PHY_LSI		// NOTE: FIXME should base on board type not PHY type 
#define MPHYHWADDR0	0x20 	/* PHY H/W Address 0x1							*/
#define MPHYHWADDR1	0x40 	/* PHY H/W Address 0x2							*/
#endif
#define MPHYaddr	0x03E0	/* PHY address (MPHYaddr) The 5-bit
							   address of the PHY device to be read
							   or written									*/
#define MPHYwrite	0x0400	/* Write (MPHYwrite) To initiate a write
							   operation, set this bit to  1 . For a
							   read operation, clear it to  0 				*/
#define MPHYbusy	0x0800	/* Busy bit (MPHYbusy) To start a read or
							   write operation, set this bit to 1.
							   The MAC controller clears the Busy bit
							   automatically when the operation is
							   completed								   	*/
#define MPreSup		0x1000	/* Preamble suppress (MPreSup) If you set
							   this bit, the preamble is not sent to
							   the PHY. If it is clear, the preamble
							   is sent										*/
#define MMDCrate	0xE000	/* MDC clock rate (MMDCrate) Controls the
							   MDC period. The default value is `011.
							   MDC period = MMDCrate ´ 4 + 32 Example)
							   MMDCrate = 011,
							   MDC period = 44 x (1/system clock			*/

/*********************************************************
 * CAM Enable Register (7.5.2.10)
 * CAMEN - offset 0x10028
 *********************************************************/
#define CAMENBIT	0x1fffff	/* Set the bits in this 21-bit value to
								   selectively enable CAM locations 20
								   through 0. To disable a CAM location,
								   clear the appropriate bit.	   			*/

/*********************************************************
 * MAC Missed Error Count Register (7.5.2.11)
 * The value in the missed error count register, MISSCNT,
 * indicates the number of frames that were discarded due
 * to various type of errors. Together with status
 * information on frames transmitted and received, the
 * missed error count register and the two pause count
 * registers provide the information required for station
 * management. Reading the missed error counter register
 * clears the register. It is then the responsibility of
 * software to maintain a global count with more bits of
 * precision. The counter rolls over from 0x7FFF to 0x8000
 * and sets the corresponding bit in the MAC control
 * register. It also generates an interrupt if the
 * corresponding interrupt enable bit is set. If station
 * management software wants more frequent interrupts,
 * you can set the missed error count register to a value
 * closer to the rollover value of 0x7FFF. For example,
 * setting a register to 0x7F00 would generate an
 * interrupt when the count value reaches 256 occurrences.
 * MISSCNTT - offset 0x1003C
 *********************************************************/
#define MissErrCnt	0xFFFF	/* (MissErrCnt) The number of valid
							   packets rejected by the MAC unit
							   because of MAC RxFIFO overflows,
							   parity errors, or because the MRxEn
							   bit was cleared. This count does not
							   include the number of packets
							   rejected by the CAM.							*/

/*********************************************************
 * MAC Received Pause Count Register (7.5.2.12)
 * The received pause count register, PZCNT, stores the
 * current value of the 16-bit received pause counter.
 * MISSCNT - offset 0x10040
 *********************************************************/
#define MACPCNT		0xFFFF	/* The count value indicates the number of
							   time slots the transmitter was paused
							   due to the receipt of control pause
							   operation frames from the MAC.				*/

/*********************************************************
 * MAC Remote Pause Count Register (7.5.2.13)
 *	RMPZCNT  - offset 0x10044
 *********************************************************/
#define MACRMPZCNT	0xFFFF	/* The count value indicates the number
							   of time slots that a remote MAC was
							   paused as a result of its sending
							   control pause operation frames.				*/

/*********************************************************
 * Content Addressable Memory (CAM) Register (7.5.2.14)
 * CAM - offset 0x10080 ~ 0x100FC
 *********************************************************/
#define CAMVAL	0xFFFFFFFF	/* The CPU uses the CAM content register as
							   data for destination address. To activate
							   the CAM function, you must set the
							   appropriate enable bits in CAM enable
							   register.									*/

/**********************************************************
 * Frame Descriptor, Buffer Descriptor related parameters.
 **********************************************************/

#define	OWNERSHIP	0x80000000	/* bit set for BDMA owns FD					*/
#define OWNERSHIP16	0x8000		/* bit set for BDMA owns FD - 16 bit access */
#define RELOAD		0x10000		/* bit set tells addRxSKBs to allocate new
								   skb - only set when an error occurred
								   NOTE that we get to reuse the stat only
								   because we KNOW that bottom two bits of
								   16 bit are always 2 (FOR US ONLY) 		*/
#define RELOAD16	0x0001		/* 16 bit access							*/
/*****************************
 * Rx Descriptor Control Bits
 *****************************/
#define SKIPBD	0x40000000	/* Set to skip this Buffer Descriptor when 
							   ownership is cleared							*/
#define SOFR	0x20000000	/* Set by BDMA to indicate Start of Frame 		*/
#define EOFR	0x10000000	/* Set by BDMA to indicate End of Frame			*/
#define DONEFR	0x08000000	/* Set by BDMA on the first BD when the
							   reception of a frame finished and it
							   used multiple BD's. 							*/
/*****************************
 * Rx Descriptor Status Bits
 *****************************/
             /* SET IF ....									*/
#define RxS_OvMax	0x0400		/* Over Maximum Size (BRxMFS)				*/
#define RxS_Halted	0x0200		/* The reception of next frame is
								   halted when MACCON.1 (MHaltImm)
								   is set, or when MACRXCON.0
								   (MRxEn) is clear.						*/
#define RxS_Rx10	0x0100		/* Packet rx'd on 7 wire interface
								   -reset -> rx'd on MII					*/
#define RxS_Done	0x0080		/* The reception process by the BDMA
								   is done.									*/
#define RxS_RxPar	0x0040		/* MAC rx FIFO detected parity error		*/
#define RxS_MUFS	0x0020		/* Set when the size of the Rx frame is
							       larger than the Maximum Untagged Frame
							       Size(1518bytes) if the long packet is
							       not enabled in the MAC Rx control
							       register.								*/
#define RxS_OFlow	0x0010		/* MAC rx FIFO was full when byte rx'd		*/
#define RxS_CrcErr	0x0008		/* CRC error OR PHY asserted Rx_er			*/
#define RxS_AlignErr	0x0004	/* Frame length != mutliple of 8 bits
								   and the CRC was invalid					*/


#define lRxS_OvMax	0x04000000	/* Over Maximum Size (BRxMFS)				*/
#define lRxS_Halted	0x02000000	/* The reception of next frame is
								   halted when MACCON.1 (MHaltImm)
								   is set, or when MACRXCON.0
								   (MRxEn) is clear.						*/
#define lRxS_Rx10	0x01000000	/* Packet rx'd on 7 wire interface
								   -reset -> rx'd on MII					*/
#define lRxS_Done	0x00800000	/* The reception process by the BDMA
								   is done.									*/
#define lRxS_RxPar	0x00400000	/* MAC rx FIFO detected parity error		*/
#define lRxS_MUFS	0x00200000	/* Set when the size of the Rx frame is
								   larger than the Maximum Untagged Frame
								   Size(1518bytes) if the long packet is
								   not enabled in the MAC Rx control
								   register.								*/
#define lRxS_OFlow	0x00100000	/* MAC rx FIFO was full when byte rx'd		*/
#define lRxS_CrcErr	0x00080000	/* CRC error OR PHY asserted Rx_er			*/
#define lRxS_AlignErr	0x00040000	/* Frame length != mutliple of 8 bits
									   and the CRC was invalid				*/

/*****************************
 * Tx Descriptor Status Bits
 * - read only status bits
 *****************************/
#define TxS_Paused	0x2000		/* transmission paused due to the
								   reception of a Pause control frame		*/
#define TxS_Halted	0x1000		/* The transmission of the next frame
								   is halted when MACCON.1 (MHaltImm)
								   is set, or when MACTXCON.0 (MTxEn)
								   is clear.								*/
#define TxS_SQErr	0x0800		/* SQE error								*/
#define TxS_Defer	0x0400		/* transmission deferred					*/
#define TxS_Coll	0x0200		/* The collision is occured in
								   half-duplex. The current frame will 
								   be retried								*/
#define TxS_Comp	0x0100		/* tx complete or discarded 				*/
#define TxS_Par		0x0080		/* tx FIFO parity error 					*/
#define TxS_LateColl	0x0040	/* tx coll after 512 bit times 				*/
#define TxS_NCarr	0x0020		/* carrier sense not detected 				*/
#define TxS_MaxDefer	0x0010	/* tx defered for max_defer (24284-bit
								  times). Frame aborted						*/
#define TxS_Under	0x0008		/* tx (FIFO) underrun 						*/
#define TxS_ExColl	0x0004		/* 16 collisions occured, aborted			*/


#define lTxS_Paused	0x20000000	/* transmission paused due to the
								   reception of a Pause control frame		*/
#define lTxS_Halted	0x10000000	/* The transmission of the next frame
								   is halted when MACCON.1 (MHaltImm)
								   is set, or when MACTXCON.0 (MTxEn)
								   is clear.								*/
#define lTxS_SQErr	0x08000000	/* SQE error								*/
#define lTxS_Defer	0x04000000	/* transmission deferred					*/
#define lTxS_Coll	0x02000000	/* The collision is occured in
								   half-duplex. The current frame will 
								   be retried								*/
#define lTxS_Comp	0x01000000	/* tx complete or discarded 				*/
#define lTxS_Par	0x00800000	/* tx FIFO parity error 					*/
#define lTxS_LateColl	0x00400000	/* tx coll after 512 bit times 			*/
#define lTxS_NCarr	0x00200000	/* carrier sense not detected 				*/
#define lTxS_MaxDefer	0x00100000	/* tx defered for max_defer (24284-bit
								   times). Frame aborted					*/
#define lTxS_Under	0x00080000	/* tx (FIFO) underrun 						*/
#define lTxS_ExColl	0x00040000	/* 16 collisions occured, aborted			*/

/*****************************
 * Tx Widget Bits
 *****************************/
#define TxWidALIGN	0x00030000	/* Alignment of the buffer					*/
#define TxWidSetup	0x00020000	/* two byte offset							*/
#define TxWidSet16	0x0002		/* two byte offset							*/

/***********************************************
 * RX Frame Descriptors for Ethernet MAC of S3c2510
 ***********************************************/
typedef struct _RxFD
{
	volatile UCHAR	*FDDataPtr;
	volatile UINT32	FDStatLen;
} RxFD;

/***********************************************
 * TX Frame Descriptors for Ethernet MAC of S3c2510
 ***********************************************/
typedef struct _TxFD
{
	volatile UCHAR	*FDDataPtr;
	volatile UINT32	FDStatLen;
} TxFD;

/**********************
 * Control structures
 **********************/

#ifdef CONFIG_NOWRITEBACK
#else
typedef struct _TXPACKET {
	    unsigned char packet[MAX_PKT_SIZE];
} TXPACKET;
#endif


/**********************************************************
 * PRIV structure
 * Information that need to be kept for each MAC / board
 *  Use net_device.priv to keep pointer to this struct
 **********************************************************/
typedef struct
{
	TxFD		*txFd;		// transmit frame descriptors
	unsigned int	tx_ready;
	unsigned int	tx_done;
#ifdef CONFIG_NOWRITEBACK
#else
	TXPACKET *txBuffer;
#endif
	unsigned int ___align16 txFrCtl[TX_NUM_FD];	// keep SKBs that is being txmitted
	unsigned int ___align16 rxFrCtl[RX_NUM_FD];
	
	RxFD		*rxFd;		// receive frame descriptors
	unsigned int	rx_done;
#ifdef REFILL_RXSKB_GROUP
	unsigned int	rx_skb_refill;
#endif
  
	/*
	 * Statistics 
	 */
	struct net_device_stats	stats;		// standard statistics

	struct STATS_2510
	{
	// Tx related stats which can distinguish tx_fifo_errors in net_device_stats structure
	// which can represent following errors
		unsigned int	iTxPar;		// defined in Buffer descriptor
		unsigned int	iTxUnderflow;	// defined in Buffer descriptor
		unsigned int	iBTxEmpty;	// defined in BMTXSTAT

	// Rx related stats which can distinguish tx_length_errors in net_device_stats structure
	// which can represent following errors
		unsigned int	iRxLengthOver;	// defined in Buffer descriptor (MSO)
		unsigned int	iRxOver1518;	// defined in Buffer descriptor (MUFS)
		unsigned int	iRxZERO;	// defined in Buffer descriptor (Length)
		unsigned int	iMRxShort;	// defined in MACRXSTAT

	// Rx related stats which can distinguish tx_fifo_errors in net_device_stats structure
	// which can represent following errors
		unsigned int	iRxOverflow;	// defined in Buffer descriptor (Overflow)
		unsigned int	iRxParErr;	// defined in Buffer descriptor (RxParErr)
		unsigned int	iBRxFull;	// defined in BMRXSTAT (BRxFull)
	} stats_2510;

	struct timer_list	mii_timer;
	struct mii_if_info	mii_if;
	unsigned int		mii;		// 1 for mii operation.

	/* 8MSol_choish, added NAPI */
	/* used to pass rx_work_limit into speedo_rx,i don't want to
	 * change its prototype */
#ifdef CONFIG_S3C2510_NAPI
	int curr_work_limit;
#endif

#if defined(CONFIG_PROC_CHIPINFO)
	struct proc_dir_entry *proc;
#endif
	/* Tx control lock.  This protects the transmit buffer ring
	 * state along with the "tx full" state of the driver.  This
	 * means all netif_queue flow control actions are protected
	 * by this lock as well.
	 */
	spinlock_t	lock;

} S3C2510_MAC;

/********************************************************************
 * Prototypes for external routines
 ********************************************************************/
extern void enable_samsung_irq( int int_num);
extern void disable_samsung_irq( int int_num);
extern unsigned char * get_MAC_address( char * name);
