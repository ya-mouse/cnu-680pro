/***************************************************************************/

/*
 *	linux/arch/m68knommu/platform/5307/config.c
 *
 *	Copyright (C) 1999-2002, Greg Ungerer (gerg@snapgear.com)
 *	Copyright (C) 2000, Lineo (www.lineo.com)
 */

/***************************************************************************/

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/param.h>
#include <linux/init.h>
#include <linux/ledman.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/traps.h>
#include <asm/machdep.h>
#include <asm/coldfire.h>
#include <asm/mcftimer.h>
#include <asm/mcfsim.h>
#include <asm/mcfdma.h>
#include <asm/delay.h>

#if defined(CONFIG_eLIA)
#include <asm/elia.h>
#endif

#include <asm/mcfwdebug.h>

/***************************************************************************/

void	reset_setupbutton(void);
void	coldfire_profile_init(void);

/***************************************************************************/

/*
 *	DMA channel base address table.
 */
unsigned int   dma_base_addr[MAX_DMA_CHANNELS] = {
        MCF_MBAR + MCFDMA_BASE0,
        MCF_MBAR + MCFDMA_BASE1,
        MCF_MBAR + MCFDMA_BASE2,
        MCF_MBAR + MCFDMA_BASE3,
};

unsigned int dma_device_address[MAX_DMA_CHANNELS];

/***************************************************************************/

void coldfire_tick(void)
{
	volatile unsigned char	*timerp;

	/* Reset the ColdFire timer */
	timerp = (volatile unsigned char *) (MCF_MBAR + MCFTIMER_BASE1);
	timerp[MCFTIMER_TER] = MCFTIMER_TER_CAP | MCFTIMER_TER_REF;
}

/***************************************************************************/

void coldfire_timer_init(void (*handler)(int, void *, struct pt_regs *))
{
	volatile unsigned short	*timerp;
	volatile unsigned char	*icrp;

	/* Set up TIMER 1 as poll clock */
	timerp = (volatile unsigned short *) (MCF_MBAR + MCFTIMER_BASE1);
	timerp[MCFTIMER_TMR] = MCFTIMER_TMR_DISABLE;

	timerp[MCFTIMER_TRR] = (unsigned short) ((MCF_CLK / 16) / HZ);
	timerp[MCFTIMER_TMR] = MCFTIMER_TMR_ENORI | MCFTIMER_TMR_CLK16 |
		MCFTIMER_TMR_RESTART | MCFTIMER_TMR_ENABLE;

	icrp = (volatile unsigned char *) (MCF_MBAR + MCFSIM_TIMER1ICR);

#if defined(CONFIG_NETtel) || defined(CONFIG_eLIA) || \
    defined(CONFIG_DISKtel) || defined(CONFIG_SECUREEDGEMP3) || \
    defined(CONFIG_CLEOPATRA) 
	*icrp = MCFSIM_ICR_AUTOVEC | MCFSIM_ICR_LEVEL6 | MCFSIM_ICR_PRI3;
	request_irq(30, handler, SA_INTERRUPT, "ColdFire Timer", NULL);
#else
	*icrp = MCFSIM_ICR_AUTOVEC | MCFSIM_ICR_LEVEL5 | MCFSIM_ICR_PRI3;
	request_irq(29, handler, SA_INTERRUPT, "ColdFire Timer", NULL);
#endif

#if defined(CONFIG_NETtel) || defined(CONFIG_eLIA) || \
    defined(CONFIG_DISKtel) || defined(CONFIG_SECUREEDGEMP3)
	/* This is not really the right place to do this... */
	reset_setupbutton();
#endif
#ifdef CONFIG_HIGHPROFILE
	coldfire_profile_init();
#endif
	mcf_setimr(mcf_getimr() & ~MCFSIM_IMR_TIMER1);
}


unsigned long coldfire_timer_offset(void)
{
	volatile unsigned short *timerp;
	unsigned long trr, tcn;

	timerp = (volatile unsigned short *) (MCF_MBAR + MCFTIMER_BASE1);
	tcn = timerp[MCFTIMER_TCN];
	trr = timerp[MCFTIMER_TRR];
	return((tcn * (1000000 / HZ)) / trr);
}

/***************************************************************************/
#ifdef CONFIG_HIGHPROFILE
/***************************************************************************/

#define	PROFILEHZ	1013

/*
 *	Use the other timer to provide high accuracy profiling info.
 */

void coldfire_profile_tick(int irq, void *dummy, struct pt_regs *regs)
{
	volatile unsigned char	*timerp;

	/* Reset the ColdFire timer2 */
	timerp = (volatile unsigned char *) (MCF_MBAR + MCFTIMER_BASE2);
	timerp[MCFTIMER_TER] = MCFTIMER_TER_CAP | MCFTIMER_TER_REF;

        if (!user_mode(regs)) {
                if (prof_buffer && current->pid) {
                        extern int _stext;
                        unsigned long ip = instruction_pointer(regs);
                        ip -= (unsigned long) &_stext;
                        ip >>= prof_shift;
                        if (ip < prof_len)
                                prof_buffer[ip]++;
                }
        }
}

void coldfire_profile_init(void)
{
	volatile unsigned short	*timerp;
	volatile unsigned char	*icrp;

	printk("PROFILE: lodging timer2=%d as profile timer\n", PROFILEHZ);

	/* Set up TIMER 2 as poll clock */
	timerp = (volatile unsigned short *) (MCF_MBAR + MCFTIMER_BASE2);
	timerp[MCFTIMER_TMR] = MCFTIMER_TMR_DISABLE;

	timerp[MCFTIMER_TRR] = (unsigned short) ((MCF_CLK / 16) / PROFILEHZ);
	timerp[MCFTIMER_TMR] = MCFTIMER_TMR_ENORI | MCFTIMER_TMR_CLK16 |
		MCFTIMER_TMR_RESTART | MCFTIMER_TMR_ENABLE;

	icrp = (volatile unsigned char *) (MCF_MBAR + MCFSIM_TIMER2ICR);

	*icrp = MCFSIM_ICR_AUTOVEC | MCFSIM_ICR_LEVEL7 | MCFSIM_ICR_PRI3;
	request_irq(31, coldfire_profile_tick, (SA_INTERRUPT | IRQ_FLG_FAST),
		"Profile Timer", NULL);
	mcf_setimr(mcf_getimr() & ~MCFSIM_IMR_TIMER2);
}

/***************************************************************************/
#endif	/* CONFIG_HIGHPROFILE */
/***************************************************************************/

/*
 *	Program the vector to be an auto-vectored.
 */

void mcf_autovector(unsigned int vec)
{
	volatile unsigned char  *mbar;

	if ((vec >= 25) && (vec <= 31)) {
		mbar = (volatile unsigned char *) MCF_MBAR;
		vec = 0x1 << (vec - 24);
		*(mbar + MCFSIM_AVR) |= vec;
		mcf_setimr(mcf_getimr() & ~vec);
	}
}

/***************************************************************************/

extern e_vector	*_ramvec;

void set_evector(int vecnum, void (*handler)(void))
{
	if (vecnum >= 0 && vecnum <= 255)
		_ramvec[vecnum] = handler;
}

/***************************************************************************/

/* assembler routines */
asmlinkage void buserr(void);
asmlinkage void trap(void);
asmlinkage void system_call(void);
asmlinkage void inthandler(void);

#ifdef TRAP_DBG_INTERRUPT
asmlinkage void dbginterrupt(void);
#endif

void __init coldfire_trap_init(void)
{
	int i;

#ifndef ENABLE_dBUG
	mcf_setimr(MCFSIM_IMR_MASKALL);
#endif

	/*
	 *	There is a common trap handler and common interrupt
	 *	handler that handle almost every vector. We treat
	 *	the system call and bus error special, they get their
	 *	own first level handlers.
	 */
#ifndef ENABLE_dBUG
	for (i = 3; (i <= 23); i++)
		_ramvec[i] = trap;
	for (i = 33; (i <= 63); i++)
		_ramvec[i] = trap;
#endif
#ifdef TRAP_DBG_INTERRUPT
	_ramvec[12] = dbginterrupt;
#endif

	for (i = 24; (i <= 30); i++)
		_ramvec[i] = inthandler;
#ifndef ENABLE_dBUG
	_ramvec[31] = inthandler;  // Disables the IRQ7 button
#endif

	for (i = 64; (i < 255); i++)
		_ramvec[i] = inthandler;
	_ramvec[255] = 0;

	_ramvec[2] = buserr;
	_ramvec[32] = system_call;

#ifdef MCF_BDM_DISABLE
	/* Disable the BDM clocking.  This also turns off most of the rest of
	 * the BDM device.  This is good for EMC reasons.  This option is not
	 * incompatible with the memory protection option.
	 */
	wdebug(MCFDEBUG_CSR, MCFDEBUG_CSR_PSTCLK);
#endif
}

/***************************************************************************/

/*
 *	Generic dumping code. Used for panic and debug.
 */

void dump(struct pt_regs *fp)
{
	extern unsigned int sw_usp, sw_ksp;
	unsigned long	*sp;
	unsigned char	*tp;
	int		i;

#ifdef CONFIG_DUMPTOFLASH
	extern unsigned long	sys_getlog(char **bp);
	extern void		sys_resetlog(void);
	extern void		flash_erasedump(void);
	extern void		flash_writedump(char *buf, int len);
	flash_erasedump();
	sys_resetlog();
#endif

	printk("\nCURRENT PROCESS:\n\n");
	printk("COMM=%s PID=%d\n", current->comm, current->pid);

#if defined(CONFIG_WATCHDOG) && defined(CONFIG_OLDMASK)
{
	extern int	swt_inwatchdog;
	if (swt_inwatchdog) {
		extern int	swt_lastjiffies, swt_reference;
		printk("WATCHDOG: ref=%d diff=%d jiffies=%d lastjiffies=%d\n\n",
			swt_reference, ((int) (jiffies - swt_lastjiffies)),
			((int) jiffies), swt_lastjiffies);
	}
}
#endif

	if (current->mm) {
		printk("TEXT=%08x-%08x DATA=%08x-%08x BSS=%08x-%08x\n",
			(int) current->mm->start_code,
			(int) current->mm->end_code,
			(int) current->mm->start_data,
			(int) current->mm->end_data,
			(int) current->mm->end_data,
			(int) current->mm->brk);
		printk("USER-STACK=%08x  KERNEL-STACK=%08x\n\n",
			(int) current->mm->start_stack,
			(int) (((unsigned long) current) + 2 * PAGE_SIZE));
	}

	printk("PC: %08lx\n", fp->pc);
	printk("SR: %08lx    SP: %08lx\n", (long) fp->sr, (long) fp);
	printk("d0: %08lx    d1: %08lx    d2: %08lx    d3: %08lx\n",
		fp->d0, fp->d1, fp->d2, fp->d3);
	printk("d4: %08lx    d5: %08lx    a0: %08lx    a1: %08lx\n",
		fp->d4, fp->d5, fp->a0, fp->a1);
	printk("\nUSP: %08x   KSP: %08x   TRAPFRAME: %08x\n",
		sw_usp, sw_ksp, (unsigned int) fp);

	printk("\nCODE:");
	tp = ((unsigned char *) fp->pc) - 0x20;
	for (sp = (unsigned long *) tp, i = 0; (i < 0x40);  i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n");

	printk("\nKERNEL STACK:");
	tp = ((unsigned char *) fp) - 0x40;
	for (sp = (unsigned long *) tp, i = 0; (i < 0xc0); i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n");

	printk("\nUSER STACK:");
	tp = (unsigned char *) (sw_usp - 0x10);
	for (sp = (unsigned long *) tp, i = 0; (i < 0x80); i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n\n");

#ifdef CONFIG_DUMPTOFLASH
	i = sys_getlog((char **) &tp);
	flash_writedump(tp, i);
#endif
}

/***************************************************************************/

#if defined(CONFIG_NETtel) || defined(CONFIG_eLIA) || \
    defined(CONFIG_DISKtel) || defined(CONFIG_SECUREEDGEMP3)
static struct timer_list    resetbutton_timerlist;

/*
 *	Routines to support the NETtel software reset button.
 */
static void resetbutton_poll(unsigned long arg)
{
    static int inbutton = 0;
    if ((mcf_getipr() & MCFSIM_IMR_EINT7) != 0) {
        if (!inbutton) {
            inbutton = 1;
#ifdef CONFIG_LEDMAN
            ledman_signalreset();
#else 
			HARD_RESET_NOW();
			/* Should never get here... */
#endif
        }
    } else
        inbutton = 0;

	resetbutton_timerlist.expires = jiffies + 5;
    add_timer(&resetbutton_timerlist);
}

/***************************************************************************/

void reset_setupbutton(void)
{
	resetbutton_timerlist.expires = jiffies + 5;
    resetbutton_timerlist.function = resetbutton_poll;
    resetbutton_timerlist.data = 0;
    add_timer(&resetbutton_timerlist);
}

#endif /* CONFIG_NETtel || CONFIG_eLIA || CONFIG_DISKtel || CONFIG_SECUREEDGEMP3 */

/***************************************************************************/

void coldfire_reset(void)
{
	HARD_RESET_NOW();
}

/***************************************************************************/

void config_BSP(char *commandp, int size)
{
#ifdef CONFIG_BOOTPARAM
	strncpy(commandp, CONFIG_BOOTPARAM_STRING, size);
	commandp[size-1] = 0;
#elif defined(CONFIG_NETtel) || defined(CONFIG_eLIA) || \
    defined(CONFIG_DISKtel) || defined(CONFIG_SECUREEDGEMP3)
	/* Copy command line from FLASH to local buffer... */
	memcpy(commandp, (char *) 0xf0004000, size);
	commandp[size-1] = 0;
#else
	memset(commandp, 0, size);
#endif /* CONFIG_NETtel || CONFIG_eLIA || CONFIG_DISKtel || CONFIG_SECUREEDGEMP3 */

	mach_sched_init = coldfire_timer_init;
	mach_tick = coldfire_tick;
	mach_trap_init = coldfire_trap_init;
	mach_reset = coldfire_reset;
	mach_gettimeoffset = coldfire_timer_offset;
}

/***************************************************************************/
#ifdef TRAP_DBG_INTERRUPT

asmlinkage void dbginterrupt_c(struct frame *fp)
{
	extern void dump(struct pt_regs *fp);
	printk("%s(%d): BUS ERROR TRAP\n", __FILE__, __LINE__);
	dump((struct pt_regs *) fp);
	asm("halt");
}

#endif
/***************************************************************************/
