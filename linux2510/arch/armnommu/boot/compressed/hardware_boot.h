/********************************************************/
/*							*/
/* Samsung KS32C4510b					*/
/* Mac Wang <mac@os.nctu.edu.tw>			*/
/*							*/
/********************************************************/
#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

/*
 * define S3C4510b CPU master clock
 */
#define MHz		1000000
#define fMCLK_MHz	(72 * MHz)
#define fMCLK		(fMCLK_MHz / MHz)
#define MCLK2		(fMCLK_MHz / 2)

/*
 * ASIC Address Definition
 */

#define Base_Addr	0x3FF0000

#define VPint	*(volatile unsigned int *)
#define VPshort	*(volatile unsigned short *)
#define VPchar	*(volatile unsigned char *)

#ifndef CSR_WRITE
#   define CSR_WRITE(addr,data)	(VPint(addr) = (data))
#endif

#ifndef CSR_READ
#   define CSR_READ(addr)	(VPint(addr))
#endif

#ifndef CAM_Reg
#   define CAM_Reg(x)		(VPint(CAMBASE+(x*0x4)))
#endif

/* ************************ */
/* System Manager Registers */
/* ************************ */
#define SYSCFG		(Base_Addr+0x0000)
#define CLKCON		(Base_Addr+0x3000)
#define EXTACON0	(Base_Addr+0x3008)
#define EXTACON1	(Base_Addr+0x300C)
#define EXTDBWTH	(Base_Addr+0x3010)
#define ROMCON0		(Base_Addr+0x3014)
#define ROMCON1		(Base_Addr+0x3018)
#define ROMCON2		(Base_Addr+0x301C)
#define ROMCON3		(Base_Addr+0x3020)
#if 0
#define ROMCON4		(Base_Addr+0x3024)
#define ROMCON5		(Base_Addr+0x3028)
#endif
#define DRAMCON0	(Base_Addr+0x3024)
#define DRAMCON1	(Base_Addr+0x3028)
#define DRAMCON2	(Base_Addr+0x302C)
#define DRAMCON3	(Base_Addr+0x3030)
#define REFEXTCON	(Base_Addr+0x3034)

/* *********************** */
/* Ethernet BDMA Registers */
/* *********************** */
#define BDMATXCON	(Base_Addr+0x9000)
#define BDMARXCON	(Base_Addr+0x9004)
#define BDMATXPTR	(Base_Addr+0x9008)
#define BDMARXPTR	(Base_Addr+0x900C)
#define BDMARXLSZ	(Base_Addr+0x9010)
#define BDMASTAT	(Base_Addr+0x9014)
#define CAMBASE		(Base_Addr+0x9100)
/*
 * CAM		0x9100 ~ 0x917C
 * BDMATXBUF	0x9200 ~ 0x92FC
 * BDMARXBUF	0x9800 ~ 0x99FC
 */

/* ********************** */
/* Ethernet MAC Registers */
/* ********************** */
#define MACON		(Base_Addr+0xA000)
#define CAMCON		(Base_Addr+0xA004)
#define MACTXCON	(Base_Addr+0xA008)
#define MACTXSTAT	(Base_Addr+0xA00C)
#define MACRXCON	(Base_Addr+0xA010)
#define MACRXSTAT	(Base_Addr+0xA014)
#define STADATA		(Base_Addr+0xA018)
#define STACON		(Base_Addr+0xA01C)
#define CAMEN		(Base_Addr+0xA028)
#define EMISSCNT	(Base_Addr+0xA03C)
#define EPZCNT		(Base_Addr+0xA040)
#define ERMPZCNT	(Base_Addr+0xA044)
#define EXTSTAT		(Base_Addr+0x9040)

/* ************************ */
/* HDLC Channel A Registers */
/* ************************ */

/* ************************ */
/* HDLC Channel B Registers */
/* ************************ */

/* ******************* */
/* I/O Ports Registers */
/* ******************* */
#define IOPMOD		(Base_Addr+0x5000)
#define IOPCON		(Base_Addr+0x5004)
#define IOPDATA		(Base_Addr+0x500C)
#if 0 /*wbk 0419*/
#define IOPDATA		(Base_Addr+0x5008)
#endif

/* ****************************** */
/* Interrupt Controller Registers */
/* ****************************** */
#define INTMOD		(Base_Addr+0x4000)
#define INTPND		(Base_Addr+0x4004)
#define INTMSK		(Base_Addr+0x4008)
#define INTPRI0		(Base_Addr+0x400C)
#define INTPRI1		(Base_Addr+0x4010)
#define INTPRI2		(Base_Addr+0x4014)
#define INTPRI3		(Base_Addr+0x4018)
#define INTPRI4		(Base_Addr+0x401C)
#define INTPRI5		(Base_Addr+0x4020)
#define INTOFFSET	(Base_Addr+0x4024)
#define INTPNDTST	(Base_Addr+0x402C)
#define INTOSET_FIQ	(Base_Addr+0x4030)
#define INTOSET_IRQ	(Base_Addr+0x4034)

#define IntMode		(VPint(INTMOD))
#define IntPend		(VPint(INTPND))
#define IntMask		(VPint(INTMSK))
#define IntOffset	(VPint(INTOFFSET))
#define IntPndTst	(VPint(INTPNDTST))
#define IntOffset_FIQ	(VPint(INTOSET_FIQ))
#define IntOffset_IRQ	(VPint(INTOSET_IRQ))

#define INT_MODE_IRQ	0x000000
#define INT_MODE_FIQ	0x1FFFFF
#define INT_MASK_DIS	0x1FFFFF
#define INT_MASK_ENA	0x000000

#define INT_ENABLE(n)		IntMask &= ~(1<<(n))
#define INT_DISABLE(n)		IntMask |= (1<<(n))
#define CLEAR_PEND_INT(n)	IntPend = (1<<(n))
#if 0 /*wbk  0419*/
#define SET_PEND_INT(n)		IntPndTst |= (1<<(n))
#else
#define SET_PEND_INT(n)		IntPndTst = (1<<(n))
#endif

/* ***************** */
/* I2C Bus Registers */
/* ***************** */

/* ************** */
/* GDMA Registers */
/* ************** */

/* ************** */
/* UART Registers */
/* ************** */

#define DEBUG_CONSOLE	(0)

#define ULCON0		(Base_Addr+0xD000)
#define ULCON1		(Base_Addr+0xE000)
#define UCON0		(Base_Addr+0xD004)
#define UCON1		(Base_Addr+0xE004)
#define USTAT0		(Base_Addr+0xD008)
#define USTAT1		(Base_Addr+0xE008)
#define UTXBUF0		(Base_Addr+0xD00C)
#define UTXBUF1		(Base_Addr+0xE00C)
#define URXBUF0		(Base_Addr+0xD010)
#define URXBUF1		(Base_Addr+0xE010)
#define UBRDIV0		(Base_Addr+0xD014)
#define UBRDIV1		(Base_Addr+0xE014)

#define UART_BASE0	ULCON0
#define UART_BASE1	ULCON1

#if DEBUG_CONSOLE == 0
	#define DEBUG_TX_BUFF_BASE	UTXBUF0	
	#define DEBUG_RX_BUFF_BASE	URXBUF0
	#define DEBUG_UARTLCON_BASE	ULCON0
	#define DEBUG_UARTCONT_BASE	UCON0
	#define DEBUG_UARTBRD_BASE	UBRDIV0
	#define DEBUG_CHK_STAT_BASE	USTAT0
#elif DEBUG_CONSOLE == 1
	#define DEBUG_TX_BUFF_BASE	UTXBUF1	
	#define DEBUG_RX_BUFF_BASE	URXBUF1
	#define DEBUG_UARTLCON_BASE	ULCON1
	#define DEBUG_UARTCONT_BASE	UCON1
	#define DEBUG_UARTBRD_BASE	UBRDIV1
	#define DEBUG_CHK_STAT_BASE	USTAT1
#endif

#if 0
#define DEBUG_ULCON_REG_VAL	(0x3)
#define DEBUG_UCON_REG_VAL	(0x9)
#define DEBUG_UBRDIV_REG_VAL	(0x500)
#else
#define DEBUG_ULCON_REG_VAL	(0x3)
#if 0 /*wbk 0419*/
#define DEBUG_UCON_REG_VAL	(0xD)
#endif
#define DEBUG_UCON_REG_VAL	(0x9)
#define DEBUG_UBRDIV_REG_VAL	(0x390)
#endif

#define DEBUG_RX_CHECK_BIT	(0X20)
#define DEBUG_TX_CAN_CHECK_BIT	(0X40)
#define DEBUG_TX_DONE_CHECK_BIT	(0X80)


/* **************** */
/* Timers Registers */
/* **************** */
#define TMOD		(Base_Addr+0x6000)
#define TDATA0		(Base_Addr+0x6004)
#define TDATA1		(Base_Addr+0x6008)
/* wbk for S5N8947 */
#if 0
#define TCNT0		(Base_Addr+0x600C)
#define TCNT1		(Base_Addr+0x6010)
#else
#define TCNT0		(Base_Addr+0x6010)
#define TCNT1		(Base_Addr+0x6014)
#endif

/*******************/
/* SYSCFG Register */
/*******************/
#define SYS_INIT_BASE	EXTDBWTH
/* wbk */
/* #define rSYSCFG		(0x87FFFF90) */	/* disable Cache/Write buffer */
#define rSYSCFG		(0x83FF0000)	/* disable Cache/Write buffer */

/**********************************/
/* System Memory Control Register */
/**********************************/


#define DEBUG_RX_CHECK_BIT	(0X20)
#define DEBUG_TX_CAN_CHECK_BIT	(0X40)
#define DEBUG_TX_DONE_CHECK_BIT	(0X80)

/*******************/
/* SYSCFG Register */
/*******************/
#define SYS_INIT_BASE	EXTDBWTH
/* wbk */
/* #define rSYSCFG		(0x87FFFF90) */	/* disable Cache/Write buffer */
#define rSYSCFG		(0x83FF0000)	/* disable Cache/Write buffer */


/**********************************/
/* System Memory Control Register */
/**********************************/
#if 0
#define DSR0		(2<<0)	/* ROM Bank0 */
#define DSR1		(0<<2)	/* 0: Disable, 1: Byte, 2: Half-Word, 3: Word */
#define DSR2		(0<<4)
#define DSR3		(0<<6)
#define DSR4		(0<<8)
#define DSR5		(0<<10)
#define DSD0		(2<<12) /* RAM Bank0 */
#define DSD1		(0<<14)
#define DSD2		(0<<16)
#define DSD3		(0<<18)
#define DSX0		(0<<20)	/* EXTIO0 */
#define DSX1		(0<<22)
#define DSX2		(0<<24)
#define DSX3		(0<<26)
#endif

/* wbk
#define rEXTDBWTH	(DSR0|DSR1|DSR2|DSR3|DSR4|DSR5 | DSD0|DSD1|DSD2|DSD3 | DSX0|DSX1|DSX2|DSX3)
*/

#define rEXTDBWTH 0x0FFFF0BA

/****************************************/
/* ROMCON0: ROM Bank 0 Control Register */
/****************************************/
#if 0
#define PMC0		(0x0<<0)	/*00: Normal ROM   01: 4 word page*/
					/*10: 8 word page  11:16 word page*/
#define tPA0		(0x0<<2)	/*00: 5 cycles     01: 2 cycles*/
					/*10: 3 cycles     11: 4 cycles*/
#define tACC0		(0x6<<4)	/*000: Disable bank 001: 2 cycles*/
					/*010: 3 cycles     011: 4 cycles*/
					/*110: 7 cycles     111: Reserved*/
#define ROM_BASE0_R	((0x00000000>>16)<<10)
#define ROM_NEXT0_R	((0x00200000>>16)<<20)
#define ROM_BASE0_B	((0x01000000>>16)<<10)
#define ROM_NEXT0_B	((0x01200000>>16)<<20)
#endif

#if 0
#define rROMCON0_R	(ROM_NEXT0_R|ROM_BASE0_R|tACC0|tPA0|PMC0)
#define rROMCON0_B	(ROM_NEXT0_B|ROM_BASE0_B|tACC0|tPA0|PMC0)
#else
/* wbk for 8947                                             */
/* This is art of programming                                 */
/* Same address sapce, but device is changing from rom to ram */
#define rROMCON0_R	0x020000FF
#define rROMCON0_B	0x120400FF
#endif

#if 0
#define rROMCON1	0x0
#define rROMCON2	0x0
#define rROMCON3	0x0
#define rROMCON4	0x0
#define rROMCON5	0x0
#else
/* wbk */
/* #define rROMCON1	0x040083ff */
/* #define rROMCON2	0x06010040 */
#define rROMCON1	0x0
#define rROMCON2	0x0
#define rROMCON3	0x0
#endif


/********************************************/
/* SDRAMCON0: SDRAM Bank 0 Control Register */
/********************************************/
#if 0
#define StRC0		(0x1<<7)
#define StRP0		(0x3<<8)
#define SDRAM_BASE0_R	((0x01000000>>16)<<10)
#define SDRAM_NEXT0_R	((0x01800000>>16)<<20)
#define SDRAM_BASE0_B	((0x00000000>>16)<<10)
#define SDRAM_NEXT0_B	((0x00800000>>16)<<20)
#define SCAN0		(0x0<<30)
#endif

#if 0
#define rSDRAMCON0_R	(SCAN0|SDRAM_NEXT0_R|SDRAM_BASE0_R|StRP0|StRC0)
#define rSDRAMCON0_B	(SCAN0|SDRAM_NEXT0_B|SDRAM_BASE0_B|StRP0|StRC0)
#else
/* #define rSDRAMCON0_R    0x18040380 			      */
/* #define rSDRAMCON0_B    0x18040380                         */
/* wbk for 8947                                             */
/* This is art of programming                                 */
/* Same address sapce, but device is changing from rom to ram */
#define rSDRAMCON0_R	0x18040380
#define rSDRAMCON0_B	0x08000380
#endif


#if 0
#define rSDRAMCON1	0x0
#define rSDRAMCON2	0x0
#define rSDRAMCON3	0x0
#else
/* wbk */
#define rSDRAMCON1	0x0
#define rSDRAMCON2	0x0
#define rSDRAMCON3	0x0
#endif

/************************************************/
/* DRAM Refresh & External I/O Control Register */
/************************************************/
#if 0
#define ExtIOBase	(0x360<<0)
#define VSF		(0x1<<15)
#define REN		(0x1<<16)
#define tCHR		(0x0<<17)
#define tCSR		(0x0<<20)
#define RefCountValue	((2048+1-(16*fMCLK))<<21)
#endif

#if 0
#define rREFEXTCON	(RefCountValue|tCSR|tCHR|REN|VSF|ExtIOBase)
#else
#define rREFEXTCON	0xCE27E360
#endif

/********/
/* Misc */
/********/

#define HARD_RESET_NOW()
#if 0
#define TMOD_TIMER0_VAL	0x3	/* Timer0  TOGGLE, and Run */
#else
#define TMOD_TIMER0_VAL	0x1	/* Timer0  TOGGLE, and Run */
#endif
#define TAG_BASE	0x11000000

#endif /* __ASM_ARCH_HARDWARE_H */
