/*
 * time.h 
 *
 * Copyright (C) 2002 Arcturus Networks Inc. 
 * by Oleksandr Zhadan <oleks@arcturusnetworks.com>
 *
 */

#ifndef __ASM_ARCH_TIME_H
#define __ASM_ARCH_TIME_H

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/arch/timex.h>

extern struct irqaction timer_irq;

extern unsigned long s3c2510_gettimeoffset(void);
extern void s3c2510_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs);

extern __inline__ void setup_timer (void)
{
    volatile struct s3c2510_timer* tt = (struct s3c2510_timer*) (TIMER_BASE);
    unsigned long v, tmp;

    gettimeoffset     = s3c2510_gettimeoffset;
    timer_irq.handler = s3c2510_timer_interrupt;
    
    tt->td[2*KERNEL_TIMER] = (unsigned long)(ARM_CLK/HZ);
    tmp = tt->tmr & ~(0x7 << (3*KERNEL_TIMER));
    tt->tmr = tmp | (0x1 << (3*KERNEL_TIMER));
// modified 
	setup_arm_irq((KERNEL_TIMER+29), &timer_irq);
    enable_irq ( (KERNEL_TIMER+29) );
}

#endif /* __ASM_ARCH_TIME_H */
