//
//
//    Samsung 8950 DMT Chipset Register Map
//
//
#ifndef __ASM_ARCH_ADSL_H
#define __ASM_ARCH_ADSL_H


/*********************************************/
/*        KTH for Spectial Register Access	 */
/*********************************************/

#define VPint	*(volatile unsigned int *)
#define VPshort	*(volatile unsigned short *)
#define VPchar	*(volatile unsigned char *)

#ifndef CSR_WRITE
#   define CSR_WRITE(addr,data)	(VPint(addr) = (data))
#endif

#ifndef CSR_READ
#   define CSR_READ(addr)	(VPint(addr))
#endif

/*
#ifndef CAM_Reg
#   define CAM_Reg(x)		(VPint(CAMBASE+(x*0x4)))
#endif
*/


extern unsigned int ADSL_Linked ;
extern int start_ADSL(struct atm_dev *dev);

/* 
 * define ADSL mananement command 
 */
#define	ADSL_IOCTL_NUM	0xAD
/* SAR H/W handle command */
#define ADSL_SAR_RESET		_IO(ADSL_IOCTL_NUM, 0x00)
#define ADSL_SAR_RESTART	_IO(ADSL_IOCTL_NUM, 0x01)
#define ADSL_SAR_STAT		_IO(ADSL_IOCTL_NUM, 0x02)
#define ADSL_SAR_CONTBL		_IO(ADSL_IOCTL_NUM, 0x03)
#define ADSL_SAR_DEBUG_ON	_IO(ADSL_IOCTL_NUM, 0x04)
#define ADSL_SAR_DEBUG_OFF	_IO(ADSL_IOCTL_NUM, 0x05)
#define ADSL_SAR_LOOP_ON	_IO(ADSL_IOCTL_NUM, 0x06)
#define ADSL_SAR_LOOP_OFF	_IO(ADSL_IOCTL_NUM, 0x07)
#define	ADSL_SAR_TX_DBG		_IO(ADSL_IOCTL_NUM, 0x08)
#define ADSL_SAR_RX_DBG		_IO(ADSL_IOCTL_NUM, 0x09)
#define ADSL_SAR_NO_DBG		_IO(ADSL_IOCTL_NUM, 0x0A)
#define ADSL_SAR_MEMDUMP    _IO(ADSL_IOCTL_NUM, 0x0B)
#define ADSL_ATM_QOS        _IO(ADSL_IOCTL_NUM, 0x0C)

/* TC handle command */
#define ADSLDMT_TCRESET	_IO(ADSL_IOCTL_NUM, 0x10)
#define ADSLDMT_TCINIT		_IO(ADSL_IOCTL_NUM, 0x11)
#define ADSLDMT_TCSTATE		_IO(ADSL_IOCTL_NUM, 0x12)

/* EOC handle command */
#define ADSLDMT_EOC_CLEAR	_IO(ADSL_IOCTL_NUM, 0x20)
#define ADSLDMT_EOC_CMD		_IO(ADSL_IOCTL_NUM, 0x21)

/* DMT handle command */
#define ADSLDMT_FIRMWARE	_IO(ADSL_IOCTL_NUM, 0x30)
#define ADSLDMT_LINKRATE	_IO(ADSL_IOCTL_NUM, 0x31)
#define ADSLDMT_CFG			_IO(ADSL_IOCTL_NUM, 0x32)
#define ADSLDMT_DSP_IDLE	_IO(ADSL_IOCTL_NUM, 0x33)
#define ADSLDMT_OPMODE_SET	_IO(ADSL_IOCTL_NUM, 0x34)
#define ADSLDMT_OPMODE		_IO(ADSL_IOCTL_NUM, 0x35)
#define ADSLDMT_TCM_ON		_IO(ADSL_IOCTL_NUM, 0x37)
#define ADSLDMT_TCM_OFF		_IO(ADSL_IOCTL_NUM, 0x38)
#define ADSLDMT_FEC_ON		_IO(ADSL_IOCTL_NUM, 0x39)
#define ADSLDMT_FEC_OFF		_IO(ADSL_IOCTL_NUM, 0x3A)
#define ADSLDMT_DSP_ABORT	_IO(ADSL_IOCTL_NUM, 0x3B)
#define ADSLDMT_DSP_DOWNLOAD	_IO(ADSL_IOCTL_NUM, 0x3C)
#define ADSLDMT_DSP_DISCONN	_IO(ADSL_IOCTL_NUM, 0x3D)
#define ADSLDMT_DEBUG_ON	_IO(ADSL_IOCTL_NUM, 0x3F)
#define ADSLDMT_DEBUG_OFF	_IO(ADSL_IOCTL_NUM, 0x40)
#define ADSLDMT_HOSTMEM		_IO(ADSL_IOCTL_NUM, 0x41)
#define ADSLDMT_DI_LOOP		_IO(ADSL_IOCTL_NUM, 0x42)
#define ADSLDMT_TC_LOOP		_IO(ADSL_IOCTL_NUM, 0x43)
#define ADSLDMT_MEDLEY		_IO(ADSL_IOCTL_NUM, 0x46)
#define ADSLDMT_STP_VERB	_IO(ADSL_IOCTL_NUM, 0x47)
#define ADSLDMT_LINESTATE	_IO(ADSL_IOCTL_NUM, 0x48)
#define ADSLDMT_LINESTATE_CLEAR _IO(ADSL_IOCTL_NUM, 0x49)
#define ADSLDMT_STP_VERB_ON	_IO(ADSL_IOCTL_NUM, 0x4A)
#define ADSLDMT_STP_VERB_OFF	_IO(ADSL_IOCTL_NUM, 0x4B)
#define ADSLDMT_DSLCFG	_IO(ADSL_IOCTL_NUM, 0x4C)
#define ADSLDMT_AFE_CHECK   _IO(ADSL_IOCTL_NUM, 0x4D)   /* KTH++ 031021, AFE device check : 8961 or 8963 */
														   

#define ADSLDMT_DBG_MODE	_IO(ADSL_IOCTL_NUM, 0x4D)

#define ADSLDMT_SIG_QUIET	_IO(ADSL_IOCTL_NUM, 0x50)
#define ADSLDMT_SIG_PILOT	_IO(ADSL_IOCTL_NUM, 0x51)
#define ADSLDMT_SIG_REVERB	_IO(ADSL_IOCTL_NUM, 0x52)
#define ADSLDMT_MEM_ADDR	_IO(ADSL_IOCTL_NUM, 0x53)
#define ADSLDMT_MEM_READ	_IO(ADSL_IOCTL_NUM, 0x54)
#define ADSLDMT_MEM_DUMP	_IO(ADSL_IOCTL_NUM, 0x55)
#define ADSLDMT_FS_SET		_IO(ADSL_IOCTL_NUM, 0x56)
#define ADSLDMT_FS_RESET	_IO(ADSL_IOCTL_NUM, 0x57)
#define ADSLDMT_CFG_MODE	_IO(ADSL_IOCTL_NUM, 0x58)
#define ADSLDMT_CFG_SNR	_IO(ADSL_IOCTL_NUM, 0x59)
#define ADSLDMT_CFG_FEC	_IO(ADSL_IOCTL_NUM, 0x5A)
#define ADSLDMT_CFG_R_2	_IO(ADSL_IOCTL_NUM, 0x5B)
#define ADSLDMT_CFG_TXPWR	_IO(ADSL_IOCTL_NUM, 0x5C)
#define ADSLDMT_RELINK		_IO(ADSL_IOCTL_NUM, 0x5D)
#define ADSLDMT_VCXO		_IO(ADSL_IOCTL_NUM, 0x5E)
#define ADSLDMT_AUTOTEST	_IO(ADSL_IOCTL_NUM, 0x5F)
#define ADSLDMT_DEBUG_DUMP_ON	_IO(ADSL_IOCTL_NUM, 0x70)
#define ADSLDMT_PILOT_RECFG	_IO(ADSL_IOCTL_NUM, 0x71)
#define ADSLDMT_MEM_WRITE	_IO(ADSL_IOCTL_NUM, 0x72)
#define ADSLDMT_BIT_NOISE       _IO(ADSL_IOCTL_NUM, 0x73)

/* MAC & PHY Control */
#define ADSLDMT_MACHPY_AN	_IO(ADSL_IOCTL_NUM, 0x60)
#define ADSLDMT_MACHPY_100FULL	_IO(ADSL_IOCTL_NUM, 0x61)
#define ADSLDMT_MACHPY_100HALF	_IO(ADSL_IOCTL_NUM, 0x62)
#define ADSLDMT_MACHPY_10FULL	_IO(ADSL_IOCTL_NUM, 0x63)
#define ADSLDMT_MACHPY_10HALF	_IO(ADSL_IOCTL_NUM, 0x64)
#define ADSLDMT_PHY_STATE		_IO(ADSL_IOCTL_NUM, 0x65)

/* 
 * define management data structure 
 */

/* SAR handle command interface */
typedef	struct sar_stat_struct {
	unsigned long	itf;		/* interface number */
	unsigned long	vpi;
	unsigned long	vci;
	unsigned char	sar_clock[16];	/* SAR clock selection */
	unsigned char	utopia_clock[16];/* UTOPIA clock selection */
	unsigned long	clock_ratio;	/* clock ratio */
	unsigned long	pcr;		/* SAR peak cell rate */
	unsigned long	mbr;		/* SAR maximum bit rate */
	unsigned long	channels;	/* channel number */

	/* packet information */
	unsigned long	tx;		/* SAR transmit count */
	unsigned long	rx;		/* SAR receive count */
	unsigned long	tx_bytes;	/* SAR Transmit Bytes */
	unsigned long	rx_bytes;	/* SAR Receive Bytes */
	unsigned long	tx_clp0;	/* SAR CPL0 Tx Cell count */
	unsigned long	rx_clp0;	/* SAR CPL0 Rx Cell count */
	unsigned long	tx_clp1;	/* SAR CPL1 Tx Cell count */
	unsigned long	rx_clp1;	/* SAR CPL1 Rx Cell count */
	unsigned long	oam_tx;		/* SAR OAM transmit count */
	unsigned long	oam_rx;		/* SAR OAM receive count */
	unsigned long	tx_cell;	/* Total Tx Cell count */
	unsigned long	rx_cell;	/* Total Rx Cell count */
	unsigned long	crc_error;	/* SAR CRC error */
	unsigned long	aal5_error;	/* SAR AAL5 error count */
	unsigned long	to_error;	/* SAR time out error count */
	unsigned long	aal5_error_tx;	/* SAR AAL5 Tx Error count */
	unsigned long	rx_hec_error;	/* Rx HEC error */

	/* error interrupt information */
	unsigned long	rx_pools_warn;	/* Rx pools warning */	
	unsigned long	rx_buff_err;	/* Rx buffer error */
	unsigned long	rx_que_err;	/* Rx Queue error */
	unsigned long	rx_inactive;	/* Rx inactive VC error */
	unsigned long	tx_que_err;	/* Tx Queue error */
	unsigned long	tx_buff_err;	/* Tx Queue error */

	/* ADSL Modem information */
	unsigned long	adsl;		/* SAR connection mode, 0=ATM25, 1=ADSL */
	unsigned long	phy;		/* PHY port number */
} SAR_STAT ; /*sar_stat ;*/	/* KTH modified */

typedef	struct sar_contbl_struct {
	unsigned long	cm_base;	/* connection memory base */
	unsigned long	sct_base;	/* scheduler connection table base */
	unsigned long	act_base;	/* AAL connection table base */
	unsigned long	sar_ct_base;	/* SAR connection table base */
	unsigned long	cb_base;	/* cell buffer base */
	unsigned long	cbr_st_base;	/* CBR sceduler table base */
	unsigned long	ubr_st_base;	/* UBR sceduler table base */
	unsigned long	vp_lt_base;	/* VP lookup table base */
	unsigned long	rlt_base;	/* 1/rate lookup table base */
} SAR_CONTBL ; /*sar_contbl ;*/	/* KTH modified */

/* DMT handle command interface */
typedef	struct dmt_version_struct {
	unsigned char	dev_name[16];
	unsigned short	version;
	unsigned short	build;
	unsigned short	api;
	unsigned short	NeSerial[32];
	unsigned short 	Reserved;
} DMT_VERSION ; /*dmt_version ;*/	/* KTH modified */

typedef	struct dmt_stat_struct {
	/* general information */
	unsigned char	modem_type[8]; 	/* modem type : ATU-R or ATU-C */
	unsigned int	linked;		/* 1:ADSL linked, 0: ADSL not linked */
	unsigned int	vpi;
	unsigned int	vci;

	/* ADSL connection information */
	unsigned long	d_snr;		/* downstream SNR margin */
	unsigned long	u_snr;		/* upstream SNR margin */
	unsigned long	d_ndr;		/* downstream network data rate */
	unsigned long	u_ndr;		/* upstream network data rate */
	unsigned long	max_rx;		/* downstream maximum data rate */
	unsigned long	max_tx;		/* upstream maximum data rate */
	int 	d_rf, u_rf, d_ri, u_ri;	/* parity byte */
	int 	d_s, u_s;		/* data frames */
	int 	d_d, u_d; 		/* interleave depth */

	unsigned long	max_down_rate;	/* maximum downstream data ratw */

	unsigned short			dw_f_as0;
	unsigned short			dw_f_as1;
	unsigned short			dw_f_as2;
	unsigned short			dw_f_as3;
	unsigned short			dw_i_as0;
	unsigned short			dw_i_as1;
	unsigned short			dw_i_as2;
	unsigned short			dw_i_as3;
	
	unsigned short			dw_f_ls0;
	unsigned short			dw_f_ls1;
	unsigned short			dw_f_ls2;
	unsigned short			dw_i_ls0;
	unsigned short			dw_i_ls1;
	unsigned short			dw_i_ls2;

	unsigned short			up_f_ls0;
	unsigned short			up_f_ls1;
	unsigned short			up_f_ls2;
	unsigned short			up_i_ls0;
	unsigned short			up_i_ls1;
	unsigned short			up_i_ls2;

	unsigned short			dw_rf;
	unsigned short			dw_ri;
	unsigned short			dw_s;
	unsigned short			dw_d;
	unsigned short			up_rf;
	unsigned short			up_ri;
	unsigned short			up_s;
	unsigned short			up_d;
	
	unsigned short			rev_num;
	unsigned short			self_test;
	unsigned short			s_n[4];
	unsigned short			trellis;
	unsigned short			issue;
	unsigned short			framing;
	unsigned short			dw_margin;
	unsigned short			dw_line_attn;
	unsigned short			dw_rel_cap;
	unsigned short			dw_out_pwr;
	unsigned short			up_margin;
	unsigned short			up_line_attn;
	unsigned short			up_rel_cap;
	unsigned short			up_out_pwr;
	unsigned short			reservedADSLI;
	
	unsigned int			dsl_lpr;
	
	/* AGC for diagnostic */
	unsigned long	init_agc;
	unsigned long	final_agc;
	unsigned long	peak_agc;

	/* general information */
	unsigned int	try;		/* DMT connection try */
	unsigned int	success;	/* how many ADSL link success connect */
	unsigned int	fail;		/* how many ADSL link fail */
	unsigned short	link_mode;
	unsigned short	link_type;
	unsigned short	dsp_fw;
	unsigned short	dsp_bn;
	unsigned short	nearE_vendor;
	unsigned short	ReservedShort;
	unsigned short	farE_vendor[8];

	unsigned short	NeSerial[32];

	unsigned int	afe_info;
}DMT_STAT ; /*dmt_stat ;*/	/* KTH modified */

typedef	struct dmt_stat_cnt{
	unsigned long	F_CRCERR_counter;
	unsigned long	I_CRCERR_counter;
	unsigned long	F_I_inform;
	unsigned long	FF_CRCERR_counter;
	unsigned long	FI_CRCERR_counter;
	unsigned long	FF_FI_inform;

	unsigned long	NE_LOF;
	unsigned long	NE_LOS;
	unsigned long	NE_ES;
	unsigned long	FE_LOF;
	unsigned long	FE_LOS;
	unsigned long	FE_ES;

	unsigned long	NE_FEC_I;
	unsigned long	NE_FEC_F;
	unsigned long	FE_FEC_I;
	unsigned long	FE_FEC_F;

	unsigned long	connect_times;
}dmt_stat_count;

typedef	struct tc_stat_cnt{
	unsigned long	tx_cell;
	unsigned long	rx_cell;
	unsigned long	hecErr;
} TC_STAT;

/* KTH++ for Qos 030520 */
/*
typedef struct forChannelQos{
	int interF;
    char    encap[16];
    int vpi;
    int vci;
    char    traffic_type[8];
    int pcr;
    int cdvt;
    int scr;
    int     mbs;
} sForChannelQos;
*/
typedef struct  atmqos{
	int aal_no;
    int vpi;
    int vci;
    int traffic_type;
    int pcr;
    int scr;
    int mbs;
    int cdvt;
    int mode;
    int num;
}atmQos;

#define ASKEY_LED_DISPLAY 1;



#endif /* __ASM_ARCH_ADSL_H */
