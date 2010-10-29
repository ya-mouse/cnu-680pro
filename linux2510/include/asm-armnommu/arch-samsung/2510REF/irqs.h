/*
 * irqs.h  modified by choi.s.h
 *
 * Copyright (C) 2002 Arcturus Networks Inc. 
 * by Oleksandr Zhadan <oleks@arcturusnetworks.com>
 *
 * This file includes the interupt controller definitions 
 * of the S3C2510X RISC microcontroller
 * based on the Samsung's "S3C2510X 32-bit RISC
 * microcontroller pre. User's Manual"
 */
 
#ifndef __ASM_ARCH_IRQS_H
#define __ASM_ARCH_IRQS_H

#define	INTMOD		0xF0140000	/* Internal interrupt mode R */
#define	EXTMOD		0xF0140004	/* External interrupt mode R */
#define	INTMASK		0xF0140008	/* Internal interrupt mask R */
#define	EXTMASK		0xF014000C	/* External interrupt mask R */
#define IPRIORHI	0xF0140010	/* HIgh bits, 5-0 bit, Interrupt by priority R */
#define IPRIORLO	0xF0140014	/* LOw bits, 31-0 bit, Interrupt by priority R */
#define INTOFFSET_FIQ	0xF0140018	/* FIQ interrupt offset R */
#define INTOFFSET_IRQ	0xF014001C	/* IRQ interrupt offset R */
#define INTPRIOR0	0xF0140020	/* Interrupt priority R 0 */
#define INTPRIOR1	0xF0140024	/* Interrupt priority R 1 */
#define INTPRIOR2	0xF0140028	/* Interrupt priority R 2 */
#define INTPRIOR3	0xF014002C	/* Interrupt priority R 3 */
#define INTPRIOR4	0xF0140030	/* Interrupt priority R 4 */
#define INTPRIOR5	0xF0140034	/* Interrupt priority R 5 */
#define INTPRIOR6	0xF0140038	/* Interrupt priority R 6 */
#define INTPRIOR7	0xF014003C	/* Interrupt priority R 7 */
#define INTPRIOR8	0xF0140040	/* Interrupt priority R 8 */
#define INTPRIOR9	0xF0140044	/* Interrupt priority R 9 */
#define INTTSTHI	0xF0140048	/* HIgh bits, 5-0 bit, Interrupt test R */
#define INTTSTLO	0xF014004C	/* LOw bits, 31-0 bit, Interrupt test R */


#define	NR_IRQS		36		/* There are 39 sources of the interrupts */

#define	SRC_IRQ_0		0
#define	SRC_IRQ_1		1
#define	SRC_IRQ_2		2
#define	SRC_IRQ_3		3
#define	SRC_IRQ_4		4
#define	SRC_IRQ_5		5
#define	SRC_IRQ_IICC		6
#define	SRC_IRQ_HUART0_TX	7
#define	SRC_IRQ_HUART0_RX	8
#define	SRC_IRQ_HUART1_TX	9
#define	SRC_IRQ_HUART1_RX	10
#define	SRC_IRQ_CUART_TX	11
#define	SRC_IRQ_CUART_RX	12
#define	SRC_IRQ_USB_HOST	13
#define	SRC_IRQ_USB_DEVICE	14
#define	SRC_IRQ_PCI_PCCARD	15
#define	SRC_IRQ_SAR_DONE	16
#define	SRC_IRQ_SAR_ERROR	17
#define	SRC_IRQ_ETH_TX0		18
#define	SRC_IRQ_ETH_RX0		19
#define	SRC_IRQ_ETH_TX1		20
#define	SRC_IRQ_ETH_RX1		21
#define	SRC_IRQ_DES		22
#define	SRC_IRQ_GDMA0		23
#define	SRC_IRQ_GDMA1		24
#define	SRC_IRQ_GDMA2		25
#define	SRC_IRQ_GDMA3		26
#define	SRC_IRQ_GDMA4		27
#define	SRC_IRQ_GDMA5		28
#define	SRC_IRQ_TIMER0		29
#define	SRC_IRQ_TIMER1		30
#define	SRC_IRQ_TIMER2		31
#define	SRC_IRQ_TIMER3		32
#define	SRC_IRQ_TIMER4		33
#define	SRC_IRQ_TIMER5		34
#define	SRC_IRQ_TIMER_WD	35


#define NR_INT_IRQS	30		/* Internal interrupt sources number */

#define	INT_IRQ_IICC		0
#define	INT_IRQ_HUART0_TX	1
#define	INT_IRQ_HUART0_RX	2
#define	INT_IRQ_HUART1_TX	3
#define	INT_IRQ_HUART1_RX	4
#define	INT_IRQ_CUART_TX	5
#define	INT_IRQ_CUART_RX	6
#define	INT_IRQ_USB_HOST	7
#define	INT_IRQ_USB_DEVICE	8
#define	INT_IRQ_PCI_PCCARD	9
#define	INT_IRQ_SAR_DONE	10
#define	INT_IRQ_SAR_ERROR	11
#define	INT_IRQ_ETH_TX0		12
#define	INT_IRQ_ETH_RX0		13
#define	INT_IRQ_ETH_TX1		14
#define	INT_IRQ_ETH_RX1		15
#define	INT_IRQ_DES			16
#define	INT_IRQ_GDMA0		17
#define	INT_IRQ_GDMA1		18
#define	INT_IRQ_GDMA2		19
#define	INT_IRQ_GDMA3		20
#define	INT_IRQ_GDMA4		21
#define	INT_IRQ_GDMA5		22
#define	INT_IRQ_TIMER0		23
#define	INT_IRQ_TIMER1		24
#define	INT_IRQ_TIMER2		25
#define	INT_IRQ_TIMER3		26
#define	INT_IRQ_TIMER4		27
#define	INT_IRQ_TIMER5		28
#define	INT_IRQ_TIMER_WD	29

#define IRQ_USBH    SRC_IRQ_USB_HOST				// 20020916 added by drsohn

#define	MASK_IRQ_IICC		0x00000001
#define	MASK_IRQ_HUART0_TX	0x00000002
#define	MASK_IRQ_HUART0_RX	0x00000004
#define	MASK_IRQ_HUART1_TX	0x00000008
#define	MASK_IRQ_HUART1_RX	0x00000010
#define	MASK_IRQ_CUART_TX	0x00000020
#define	MASK_IRQ_CUART_RX	0x00000040
#define	MASK_IRQ_USB_HOST	0x00000080
#define	MASK_IRQ_USB_DEVICE	0x00000100
#define	MASK_IRQ_PCI_PCCARD	0x00000200
#define	MASK_IRQ_SAR_DONE	0x00000400
#define	MASK_IRQ_SAR_ERROR	0x00000800
#define	MASK_IRQ_ETH_TX0	0x00001000
#define	MASK_IRQ_ETH_RX0	0x00002000
#define	MASK_IRQ_ETH_TX1	0x00004000
#define	MASK_IRQ_ETH_RX1	0x00008000
#define	MASK_IRQ_DES		0x00010000
#define	MASK_IRQ_GDMA0		0x00020000
#define	MASK_IRQ_GDMA1		0x00040000
#define	MASK_IRQ_GDMA2		0x00080000
#define	MASK_IRQ_GDMA3		0x00100000
#define	MASK_IRQ_GDMA4		0x00200000
#define	MASK_IRQ_GDMA5		0x00400000
#define	MASK_IRQ_TIMER0		0x00800000
#define	MASK_IRQ_TIMER1		0x01000000
#define	MASK_IRQ_TIMER2		0x02000000
#define	MASK_IRQ_TIMER3		0x04000000
#define	MASK_IRQ_TIMER4		0x08000000
#define	MASK_IRQ_TIMER5		0x10000000
#define	MASK_IRQ_TIMER_WD	0x20000000


#define	NR_EXT_IRQS	6		/* External interrupt sources number */

#define	EXT_IRQ_0		0
#define	EXT_IRQ_1		1
#define	EXT_IRQ_2		2
#define	EXT_IRQ_3		3
#define	EXT_IRQ_4		4
#define	EXT_IRQ_5		5

#define	MASK_IRQ_0		0x00000001
#define	MASK_IRQ_1		0x00000002
#define	MASK_IRQ_2		0x00000004
#define	MASK_IRQ_3		0x00000008
#define	MASK_IRQ_4		0x00000010
#define	MASK_IRQ_5		0x00000020
#define	MASK_IRQ_GLOBAL		0x80000000

	
#endif	/* __ASM_ARCH_IRQS_H */
