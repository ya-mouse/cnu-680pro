/*
 * linux/include/asm/arch-samsung/serial.h
 * 2001 Mac Wang <mac@os.nctu.edu.tw>
 */
#ifndef __ASM_ARCH_SERIAL_H
#define __ASM_ARCH_SERIAL_H

#include <asm/arch/hardware.h>
#include <asm/irq.h>

#define RS_TABLE_SIZE	1
#define BASE_BAUD	9600
#define STD_COM_FLAGS	(ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)
#define STD_SERIAL_PORT_DEFNS		\
	/* UART CLK PORT IRQ FLAGS */	\
	{ 0, BASE_BAUD, S3C3410X_UART_BASE, S3C3410X_INTERRUPT_URX, STD_COM_FLAGS },	/* ttyS0 */

#define EXTRA_SERIAL_PORT_DEFNS

#define UART_LCR	0x00		/* Line Control Register	*/
#define UART_IER	0x04		/* Global Control Register	*/
#define UART_GCR	0x04		/* Global Control Register	*/
#define UART_LSR	0x08		/* Line Status Reigster		*/
#define UART_TX		0x0C		/* TX buffer Register		*/
#define UART_RX		0x10		/* RX buffer Register		*/
#define UART_BDR	0x14		/* Baudrate Divisor Reigster	*/

#define UART_LSR_OE	0x01		// Overrun error
#define UART_LSR_PE	0x02		// Parity error
#define UART_LSR_FE	0x04		// Frame error
#define UART_LSR_BI	0x08		// Break detect
#define UART_LSR_DTR	0x10		// Data terminal ready
#define UART_LSR_DR	0x20		// Receive data ready
#define UART_LSR_THRE	0x40		// Transmit buffer register empty
#define UART_LSR_TEMT	0x80		// Transmit complete

#define UART_LCR_WLEN5	0x00
#define UART_LCR_WLEN6	0x01
#define UART_LCR_WLEN7	0x02
#define UART_LCR_WLEN8	0x03
#define UART_LCR_PARITY	0x20
#define UART_LCR_NPAR	0x00
#define UART_LCR_OPAR	0x20
#define UART_LCR_EPAR	0x28
#define UART_LCR_SPAR	0x10
#define UART_LCR_SBC	0x40

#define UART_GCR_RX_INT	0x01
#define UART_GCR_TX_INT	0x08
#define UART_GCR_RX_STAT_INT	0x04

#define UART_IER_MSI
#define UART_IER_RLSI	0x04
#define UART_IER_THRI	0x08
#define UART_IER_RDI	0x01
	
#define UART_MSR		0
#define UART_MSR_DCD		0
#define UART_MSR_RI		0
#define UART_MSR_DSR		0
#define UART_MSR_CTS		0
#define UART_MSR_DDCD		0
#define UART_MSR_TERI		0
#define UART_MSR_DDSR		0
#define UART_MSR_DCTS		0
#define UART_MSR_ANY_DELTA	0

#define PORT_S3C3410X		15


struct serial_baudtable
{
	unsigned int baudrate;
	unsigned int div;
};

struct serial_baudtable uart_baudrate[] = /* @todo: correct baudrates */
{
	{  1200, 0x5150},
	{  2400, 0x28A0},
	{  4800, 0x1440},
	{  9600, 0x0A20},
	{ 19200, 0x0500},
	{ 38400, 0x0280},
	{ 57600, 0x01A0},
	{115200, 0x00D0},
	{187500, 0x00D0},
};

unsigned int baudrate_div(unsigned int baudrate)
{
	int i;
	int len = sizeof(uart_baudrate)/sizeof(struct serial_baudtable);
	for(i = 0; i < len; i++)
		if(uart_baudrate[i].baudrate == baudrate)
			return uart_baudrate[i].div;
	return 0;
}

#if 0 /* no longer used */
#define disable_uart_tx_interrupt(line)		\
{						\
	INT_DISABLE(S3C3410X_INTERRUPT_UTX);	\
	CLEAR_PEND_INT(S3C3410X_INTERRUPT_UTX);	\
}

#define disable_uart_rx_interrupt(line)		\
{						\
	INT_DISABLE(S3C3410X_INTERRUPT_URX);	\
	CLEAR_PEND_INT(S3C3410X_INTERRUPT_URX);	\
}

#define enable_uart_tx_interrupt(line)		\
{						\
	INT_ENABLE(S3C3410X_INTERRUPT_UTX);	\
	SET_PEND_INT(S3C3410X_INTERRUPT_UTX);	\
}

#define enable_uart_rx_interrupt(line)		\
{						\
	INT_ENABLE(S3C3410X_INTERRUPT_URX);	\
}
#endif

#endif /* __ASM_ARCH_SERIAL_H */
