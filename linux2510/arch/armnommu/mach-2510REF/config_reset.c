/* 
 * config_reset.c
 *
 * Controls source and action of reset on some board which do not use generic reset input
 * but use some GPIO for reset input.
 *
 * Rev. 1.0
 * 8, 2002, WG.Kim
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/serial.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/fcntl.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/arch/irq.h>
#include <asm/system.h>

#ifndef REG_OUT_LONG
#   define REG_OUT_LONG(hwbase,addr,value) (*(ulong *)(hwbase+addr) = (value))
#endif

#ifndef REG_IN_LONG
#   define REG_IN_LONG(hwbase,addr,pData) ((pData) = *(ulong *)(hwbase+addr))
#endif

#if	1
extern void	factory_default(void);		//	import from ~/linux/arch/armnommu/mach-CM47/factory_default.c
#else
extern int board_env_save_1(); /*wbk*/
#endif
/* definition GPIO Port for PCMCIA */
#define GPIO_PORT_5	(1<<5)
#define GPIO_PORT_6	(1<<6)
#define GPIO_PORT_7	(1<<7)
#define GPIO_PORT_8	(1<<8)
#define GPIO_PORT_10	(1<<10)
#define GPIO_PORT_11	(1<<11)
#define GPIO_PORT_12	(1<<12)
#define GPIO_PORT_13	(1<<13)
#define GPIO_PORT_14	(1<<14)
#define GPIO_PORT_15	(1<<15)
#define GPIO_PORT_17	(1<<17)

/* values for setting IOPCON0,1 register */
#define XIRQ_ENABLE     	0x10
#define ACTIVE_LOW      	0
#define ACTIVE_HIGH     	0x8
#define FILTERING_OFF   	0
#define FILTERING_ON    	0x4    
#define LEVEL           	0
#define RISING_EDGE     	0x1
#define FALLING_EDGE    	0x2
#define BOTH_EDGE       	0x3

#define	nEXT0_INT           0
#define HI_STATUS			 1
#define LOW_STATUS			 0

/*
 * GPIO port 5 is used as reset input.
 * This is implemented using external interrupt 0.
 * So, here IRQ0 is controlled.
 */
 int toggle_status=HI_STATUS;
int  *three_times_ok;

/*extern unsigned long wall_jiffies;*/
struct timeval /*tm_val,*/ Start_tv, End_tv;
int detect_hi;
#ifdef CONFIG_RESET_GPIO5

static void irq_handle_reset(int irq, void *dev_id, struct pt_regs *regs)
{
  	unsigned int tmpiopmod;


	if(!detect_hi)
		do_gettimeofday(&Start_tv);
/*	
    printk("reset start...[%d]:0x%x, 0x%x\n",cnt++, tm_val.tv_sec, tm_val.tv_usec);
*/
   	REG_IN_LONG(IOPDATA, 0, tmpiopmod);


	if(tmpiopmod & GPIO_PORT_5)
		{
		if(toggle_status == LOW_STATUS)
			{
			/*do_gettimeofday(&hi_tv);*/
			//printk("GPIO_5 Low!!\n");
			detect_hi++;
			/*if((hi_tv.tv_sec - low_tv.tv_sec) < 3)*/
			if(detect_hi < 3)
				{			
				REG_IN_LONG(IOPCON0, 0, tmpiopmod);
				tmpiopmod &= ~(0x1f<<8);
	  			/* xIRQ0 settings for PCMCIA (port5 : PCMCIA card interrupt) */
	  			tmpiopmod |= ((XIRQ_ENABLE|ACTIVE_LOW|FILTERING_ON|FALLING_EDGE) << 8);		
			  	REG_OUT_LONG(IOPCON0, 0, tmpiopmod);
			  	toggle_status = HI_STATUS;
				}
			else
				{
				do_gettimeofday(&End_tv);
				if((End_tv.tv_sec - Start_tv.tv_sec) < 5)
					{
					/* Reset start.. start watchdog timer */
					printk("Reset Start.. Start Watchdog timer!\n");
					//three_times_ok = 0x33;
				#if	1	//	Jemings Ko	for factory default setting
					factory_default();
				#else
					board_env_save_1();
				#endif
					outl(0xff01, WDCON); 
					}
				else
					detect_hi = 0;
					REG_IN_LONG(IOPCON0, 0, tmpiopmod);
					tmpiopmod &= ~(0x1f<<8);
	  				/* xIRQ0 settings for PCMCIA (port5 : PCMCIA card interrupt) */
	  				tmpiopmod |= ((XIRQ_ENABLE|ACTIVE_LOW|FILTERING_ON|FALLING_EDGE) << 8);		
			  		REG_OUT_LONG(IOPCON0, 0, tmpiopmod);
			  		toggle_status = HI_STATUS;
				}
			}
		}
	else
		{
		if(toggle_status == HI_STATUS)
			{
			/*do_gettimeofday(&low_tv);*/
			//printk("GPIO_5 Hi!!\n");
			REG_IN_LONG(IOPCON0, 0, tmpiopmod);
			tmpiopmod &= ~(0x1f<<8);
		  	/* xIRQ0 settings for PCMCIA (port5 : PCMCIA card interrupt) */
  			tmpiopmod |= ((XIRQ_ENABLE|ACTIVE_HIGH|FILTERING_ON|RISING_EDGE) << 8);		
		  	REG_OUT_LONG(IOPCON0, 0, tmpiopmod);
		 	
		  	toggle_status = LOW_STATUS;
			}

		} 
}

static int __init reset_activate(void)
{
   	unsigned int i, tmpiopmod, ret, irq;
	unsigned long flags;

	printk("Initialize reset input.\n");

	save_flags(flags);
	
	/* All those are inputs - GPIO_5 */
  	REG_IN_LONG(IOPMOD, 0, tmpiopmod);
  	tmpiopmod &= ~(GPIO_PORT_5) ;
  	REG_OUT_LONG(IOPMOD, 0, tmpiopmod);
  			
  	/* Set transition detect */
	/* I/O port settings for PCMCIA */
  	REG_IN_LONG(IOPCON0, 0, tmpiopmod);
  	tmpiopmod &= ~(0x1f<<8);

  	/* xIRQ0 settings for PCMCIA (port5 : PCMCIA card interrupt) */
  	tmpiopmod |= ((XIRQ_ENABLE|ACTIVE_LOW|FILTERING_ON|FALLING_EDGE) << 8);		
  	REG_OUT_LONG(IOPCON0, 0, tmpiopmod);

#if 0
  	REG_IN_LONG(IOPDATA, 0, tmpiopmod);
	/* Reset PCMCIA - GPIO5 */
   	tmpiopmod |= GPIO_PORT_5;
   	REG_OUT_LONG(IOPDATA, 0, tmpiopmod);
#endif	/*if 0*/

	irq = nEXT0_INT;
  	ret = request_irq( irq, irq_handle_reset, SA_INTERRUPT, "RESET_HND", NULL );
  	if( ret < 0 ) goto irq_err;

	restore_flags(flags);
	
	return 0;
	
irq_err:
  printk( KERN_ERR "%s: Request for IRQ %u failed\n", __FUNCTION__, irq );
  return -1;

}


//#ifdef CONFIG_RESET_GPIO5
module_init(reset_activate);
#endif

