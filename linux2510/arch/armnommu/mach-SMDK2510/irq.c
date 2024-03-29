/*
 * irq.c 
 *
 */

#include <linux/init.h>

#include <asm/mach/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

void __inline__ s3c2510_mask_irq(unsigned int irq)
{
    unsigned long mask;

    if	 ( irq >= NR_EXT_IRQS ) {
	 mask = inl(INTMASK);
	 mask |= ( 1 << (irq - NR_EXT_IRQS));
	 outl(mask, INTMASK);
	 } 
    else {
	 mask = inl(EXTMASK);
	 mask |= ( 1 << irq);
	 outl(mask, EXTMASK);
	} 
}	

void __inline__ s3c2510_unmask_irq(unsigned int irq)
{
    unsigned long mask;

    if	 ( irq >= NR_EXT_IRQS ) {
	 mask = inl(INTMASK);
	 mask &= ~( 1 << (irq - NR_EXT_IRQS));
	 outl(mask, INTMASK);
	 } 
    else {
	 mask = inl(EXTMASK);
	 mask &= ~( 1 << irq);
	 outl(mask, EXTMASK);
	} 
}

void __inline__ s3c2510_mask_global(void)
{
    unsigned long mask;
    mask = inl(EXTMASK);
    mask |= MASK_IRQ_GLOBAL;
    outl(mask, EXTMASK);
}

void __inline__ s3c2510_unmask_global(void)
{
    unsigned long mask;
    mask = inl(EXTMASK);
    mask &= ~(MASK_IRQ_GLOBAL);
    outl(mask, EXTMASK);
}

void __inline__ s3c2510_mask_ack_irq(unsigned int irq)
{
    s3c2510_mask_irq (irq);
}
