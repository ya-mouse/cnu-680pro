/*
 * time.c Timer functions for SMDK2510-AN
 *
 * Copyright (C) 2002 Arcturus Networks Inc. 
 * by Oleksandr Zhadan <oleks@arcturusnetworks.com>
 *
 */
 /* choish added SMDK2510 */
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/reboot.h>
#include <linux/tqueue.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <asm/arch/timers.h> //ryc++
#include <linux/gigabyte_gpio.h> // Storm 20040813  add init LED showing.

//#include <asm/arch/pci.h> //ryc-- for compile error

#define HZMSEC	10000	/* (HZ*100) */

int ResetConf_flag = 0;
static struct tq_struct ResetConf_task;

unsigned long s3c2510_gettimeoffset (void)
{
    struct s3c2510_timer *tt = (struct s3c2510_timer *) (TIMER_BASE);
    return ((tt->td[2*KERNEL_TIMER] - tt->td[2*KERNEL_TIMER+1]) / (tt->td[2*KERNEL_TIMER]/HZMSEC));
}

static void mtd_erase_callback (struct erase_info *instr)
{
	    wake_up((wait_queue_head_t *)instr->priv);
}


/* OUTINE NAME - erase_config
 ** -----------------------------------------------------------------
 ** FUNCTION:  erase configuration in mtd block 2
 **            
 **          
 ** INPUT  : None    
 ** OUTPUT : None
 ** RETUEN :     
 **          None    
 ** -----------------------------------------------------------------
 **/             
 void erase_config()
{               

    int ret = 0;
    struct mtd_info *mtd_if;
    struct erase_info *erase;
    
    u_int32_t start_addr=0;
    mtd_if = __get_mtd_device(NULL,1); /* get mtd 1 */
    erase=kmalloc(sizeof(*erase),GFP_KERNEL);
	   
	   if (!erase) {
	       printk("malloc memory fail\n");
	       return;
	   } else {
               /* erase each block */
               for (start_addr = 0;start_addr < mtd_if->size;start_addr += mtd_if->erasesize) {
                   wait_queue_head_t waitq;
                   DECLARE_WAITQUEUE(wait, current);	    

		   init_waitqueue_head(&waitq);

		   memset (erase,0,sizeof(struct erase_info));

		   erase->addr = start_addr;
		   erase->len = mtd_if->erasesize;

		   erase->mtd = mtd_if;
		   erase->callback = mtd_erase_callback;
		   erase->priv = (unsigned long)&waitq;

		   printk("Erase MTD 1 Address:%X Size:%d KBytes\n",erase->addr,erase->len/1024);

		   ret = mtd_if->erase(mtd_if, erase);
		   if (!ret) {
			   set_current_state(TASK_UNINTERRUPTIBLE);
			   add_wait_queue(&waitq, &wait);
			   if (erase->state != MTD_ERASE_DONE && erase->state != MTD_ERASE_FAILED) {
				   schedule();
		           }
			   remove_wait_queue(&waitq, &wait);
			   set_current_state(TASK_RUNNING);
			   ret = (erase->state == MTD_ERASE_FAILED)?-EIO:0;
			   if (ret == -EIO)
				   printk("Erase %X (size %d KBytes)fail\n",erase->addr,erase->len/1024);
		   }
	       }
	       kfree(erase);
           }
	   /* reboot */
	   machine_restart(NULL);	   
           //WDT_reboot();
}


// KenC, 20031105
// 2003.12.16/storm/add init function to here
void scan_init_button(void)
{
    static int nCount;
    unsigned long tmp;
    
    tmp = *(unsigned int *)IOPDATA1;
    tmp &= 0x00000020;	// GPIO5
    if ( !tmp )		// low
    { 
	gpio_kernel_ioctl(GPIO_LED_CMD,GPIO_WRITE_ERROR_LED_ON);
	nCount++;
	if (nCount >= 10)
	{
	   printk("==== init button detect %x ========\n", tmp);
	   // TBD, we shall add here to do default setting and reboot 
	   ResetConf_flag = 1;
           ResetConf_task.routine = erase_config;
	   schedule_task(&ResetConf_task);	
	   nCount = 0;
	}
    }
    else{
	   nCount = 0;
    }
}

void s3c2510_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    static int mS;
//  static int led;
    static int sec, on;

    /* entry per 10 ms */    
#if 0
    if  (mS++ > 49) {
	if  ( on ) {
	    
	     Vol32(0xf0030000) &= 0xffffff00;	
	     Vol32(0xf003001c) = (255-sec);
	     if  (sec++ > 255) sec = 0;
	     on = 0;
	     }
	else {
	     Vol32(0xf0030000) |= 0x000000ff;
	     on = 1;
	     }
	mS = 0;
	}
#endif
    mS++;
    if (mS%10 == 0){ 	// per 100ms
	   printk("==== scan init button ========\n");
	scan_init_button();
    }

    if  (mS > 49) {
	if  ( on ) {
#if 0
	     Vol32(0xf0030000) &= 0xffffff7f;	// GPIO 7 output mode (set 0)
	     Vol32(0xf003001c) |= 0x00000080;	// GPIO 7 write data 1
	     sec++;
	     if (sec > 3) {
	     	Vol32(0xf0030000) &= 0xffffffbf;	// GPIO 6 output mode (set 0)
	     	Vol32(0xf003001c) |= 0x00000040;	// GPIO 6 write data 1
	        sec=0;
		}
#endif
		
	     on = 0;
	     }
	else {
#if 0
	     Vol32(0xf0030000) |= 0x000000c0;
#endif
	     on = 1;
	     }
	mS = 0;
	}


    /* choish modified for SMDK2510   timer0 32 --> 29 */	
    REG_WRITE(IC, (1<<(irq-28)));
    
    do_timer(regs);
}

