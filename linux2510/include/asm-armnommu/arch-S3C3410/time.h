/*
 * uclinux/include/asm-armnommu/arch-S3C3410/time.h
 *
 * 2003 Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
 *
 * Setup for 16 bit timer 0, used as system timer.
 *
 */

#ifndef __ASM_ARCH_TIME_H__
#define __ASM_ARCH_TIME_H__

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/arch/timex.h>

extern struct irqaction timer_irq;

extern unsigned long s3c3410_gettimeoffset(void);
extern void s3c3410_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs);

extern __inline__ void setup_timer (void)
{
	u_int8_t tmod;
	u_int16_t period;

	/*
	 * disable and clear timer 0, set to
	 * internal clock and interval mode
	 */
	tmod = S3C3410X_T16_OMS_INTRV | S3C3410X_T16_CL;
	outb(tmod, S3C3410X_TCON0);

	/* initialize the timer period */
	period = (u_int32_t)((CONFIG_ARM_CLK/10)/HZ);
	outw(period, S3C3410X_TDAT0);
	outb(10-1, S3C3410X_TPRE0); /* use prescaler 10 */

	/*
	 * @todo do those really need to be function pointers ?
	 */
	gettimeoffset     = s3c3410_gettimeoffset;
	timer_irq.handler = s3c3410_timer_interrupt;

	/* set up the interrupt vevtor for timer 0 overflow */
	setup_arm_irq(S3C3410X_INTERRUPT_TOF0, &timer_irq);

	/* let timer 0 run... */
	tmod |= S3C3410X_T16_TEN;
	outb(tmod, S3C3410X_TCON0);
}

#endif /* __ASM_ARCH_TIME_H__ */
