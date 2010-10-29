/*
 * drivers/char/gigabyte_gpio.c
 *
 * GPIO driver for LED access and board information.
 *
 * Copyright (C) 2003 Gigabyte Technology
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * --------------------------------------------------------------------------- 
 * HISTORY:
 *
 *        Date,by          Modification Description
 *    ---------------  --------------------------------
 *    25Mar2003 Frank       Create
 * 
 */

/* INCLUDE FILE DECLARATIONS  
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <asm/irq.h>
#include <linux/mtd/mtd.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>
#include <linux/tqueue.h>
#include <linux/gigabyte_gpio.h>

#include <linux/swap.h>
#include <linux/swapctl.h>

#define  GPIO_UNIT_TEST 0 	/* for uint test only */

 
/* STATIC VARIABLE DECLARATIONS
 */ 
//int ResetConf_flag = 0;
int nDisableSwap = 0;
static struct tq_struct ResetConf_task;
static struct timer_list stTimer;


/* GPIO device driver operation function */
struct file_operations stGigabyte_gpio_fops = {
    ioctl   : gigabyte_gpio_ioctl,
    open    : gigabyte_gpio_open,
    release : gigabyte_gpio_release
};

/* misc device struct */
static struct miscdevice sh_gpio_miscdev = {
    minor   : GIGABYTE_GPIO_MINOR,
    name    : "gigabyte_gpio",
    fops    : &stGigabyte_gpio_fops
};    

/* KenC, 20031218 modify GPIO config for samsung 2510i, GPIO5-init, GPIO6-wlanLed, GPIO7-statusLed */
static unsigned int *pnLEDRegister = 0xF0030000;
static unsigned int *pnLedIoAddr = 0xF003001C;		
static unsigned int nWirelessBlink = GPIO_LED_STATUS_INIT; /* initial ==2, LED On ==1, LED Off==0 */
static unsigned int nWirelessPacket = 0;		/* wireless 1 packet counter */
static unsigned int nPostErr = 0;			/* system error led flag */
static unsigned int *pnInitButtonRegister = 0xF0030000;
static unsigned int *pnInitButtonIoAddr = 0xF003001C; 


/* -----------------------------------------------------------------
 * ROUTINE NAME - gigabyte_gpio_ioctl
 * -----------------------------------------------------------------
 * FUNCTION:  Provide a I/O control function for LED and Board number
 *            inforamtion access 
 *          
 * INPUT  : ucGPIOType  
 * INPUT  : ucRWMode    
 * OUTPUT : None
 * RETUEN :  
 *          OK, FAIL or BOARD_NUMBER
 * -----------------------------------------------------------------
 */
unsigned char gigabyte_gpio_ioctl(struct inode *inode, struct file *file,unsigned char ucGPIOType,unsigned char ucRWMode)
{
    /* combine to one function with gpio_kernel_ioctl */
    return gpio_kernel_ioctl(ucGPIOType,ucRWMode);
}


/* -----------------------------------------------------------------
 * ROUTINE NAME - gigabyte_gpio_open
 * -----------------------------------------------------------------
 * FUNCTION:  Provide a open function for device driver
 *  
 * INPUT  : None    
 * OUTPUT : None
 * RETUEN :  
 *          OK, or FAIL 
 * -----------------------------------------------------------------
 */
int gigabyte_gpio_open(struct inode *inode, struct file *file)
{
    /* printk("gigabyte_gpio open here\n"); */
    return 0;
}


/* -----------------------------------------------------------------
 * ROUTINE NAME - gigabyte_gpio_release
 * -----------------------------------------------------------------
 * FUNCTION:  Provide a release function for device driver
 *  
 * INPUT  : None    
 * OUTPUT : None
 * RETUEN :  
 *          OK, or FAIL 
 * -----------------------------------------------------------------
 */
void gigabyte_gpio_release(struct inode *inode, struct file *file)
{
    /* printk("giagbyte release here\n");    */
    return;
}


/* -----------------------------------------------------------------
 * ROUTINE NAME - gigabyte_gpio_init
 * -----------------------------------------------------------------
 * FUNCTION:  Provide a initail function to register a device driver
 *            
 *  
 * INPUT  : None    
 * OUTPUT : None
 * RETUEN :  
 *          OK, or FAIL 
 * MODIFY:  1. KenC, 20031218 modify for Samsung B41G platform 
 * -----------------------------------------------------------------
 */
int __init gigabyte_gpio_init(void)
{
    printk("gigabyte_gpio: misc_register, and initial GPIO input or output mode\n");

    /* config gpio7, gpio6 for LED, output mode */
    *pnLEDRegister &= 0xffffff7f;
    *pnLEDRegister &= 0xffffffbf;

    /* config gpio5 for init button, input mode */
#if 0
    *pnInitButtonRegister |= 0x00000020;
#endif
  
    if ( misc_register(&sh_gpio_miscdev) ) {
        printk("KERNEL : gigabyte_gpio: can't register misc device\n");
        return -EINVAL;
    }else{ 
        /* NOP */
    }
    
    /* Added a timer to process LED */
    init_timer(&stTimer);
    stTimer.function = gigabyte_do_led;
    stTimer.data = 0;
    mod_timer(&stTimer, jiffies + GPIO_LED_TIME_INTERVAL);                                            
    /* printk("gigabyte_gpio init success\n"); */
    return 0;
}


/* -----------------------------------------------------------------
 * ROUTINE NAME - gpio_kernel_ioctl
 * -----------------------------------------------------------------
 * FUNCTION:  Provide a I/O control function for LED and Board number
 *            inforamtion access 
 *          
 * INPUT  : ucGPIOType  
 * INPUT  : ucRWMode       
 * OUTPUT : None
 * RETUEN :  
 *          OK, FAIL or BOARD_NUMBER
 * MODIFY:  1. KenC, 20031218 modify for Samsung B41G platform, and 
 *             modify WIRELESS_OFF 
 * -----------------------------------------------------------------
 */
unsigned char gpio_kernel_ioctl(unsigned char ucGPIOType,unsigned char ucRWMode)
{
    unsigned int nReturn=0;
     
    if ( ucGPIOType == GPIO_LED_CMD ) {
        
        switch ( ucRWMode ) {    /* which command mode  */
        
        case GPIO_WRITE_ERROR_LED_ON:
            /* set bit 2 to 1, check hw data sheet 3.4.26.8 for detail */
            *pnLedIoAddr |= 0x00000080;	
            nPostErr=GPIO_LED_STATUS_ON;
            return GPIO_RETURN_OK;
            break;
            
        case GPIO_WRITE_ERROR_LED_OFF: 
            /* set bit 2 to 0, check hw data sheet 3.4.26.8 for detail */
            *pnLedIoAddr &= 0xffffff7f;	
            nPostErr=GPIO_LED_STATUS_OFF;
            return GPIO_RETURN_OK;
            break;
        
        case GPIO_WRITE_WIRELESS_LED_ON: 
            nWirelessPacket++;
            return GPIO_RETURN_OK;   
            break;
        
        case GPIO_WRITE_WIRELESS_LED_OFF:
            nWirelessPacket = 0;
	    /* KenC, 20031218 add for disable or drop line, will keep off,
	       and whenever traffic go through, will LED_ON back to normal status */ 
            *pnLedIoAddr &= 0xffffffbf;
	    nWirelessBlink = GPIO_LED_STATUS_INIT; 
            return GPIO_RETURN_OK;   
            break;
    
        default:  /* Fail here */
            return GPIO_RETURN_FAIL;
        }    

    } else if ( ucGPIOType == SWAP_CMD ) {
	    
        switch ( ucRWMode ) {    /* which command mode  */

	case WAKEUP_SWAPD:
            wakeup_swapd();
            return GPIO_RETURN_OK;   
            break;
		
        case DISABLE_SWAPD_IN_GET_PAGE:
            disable_swapd_in_get_page();
            return GPIO_RETURN_OK;   
            break;
	    
        default:  /* Fail here */
            return GPIO_RETURN_FAIL;
	    
        }    
    }
    else if ( ucGPIOType == GPIO_BOARD_CMD)
	return BOARD_NUMBER_B41G;

    return GPIO_RETURN_FAIL;
}

static void mtd_erase_callback (struct erase_info *instr)
{
    wake_up((wait_queue_head_t *)instr->priv);
}


void wakeup_swapd()
{
    if (waitqueue_active(&kswapd_wait))
        wake_up_interruptible(&kswapd_wait);
}
	

void disable_swapd_in_get_page()
{
    nDisableSwap = 1;
}
	

#if 0
/* -----------------------------------------------------------------
 * ROUTINE NAME - erase_config
 * -----------------------------------------------------------------
 * FUNCTION:  erase configuration in mtd block 2
 *            
 *  
 * INPUT  : None    
 * OUTPUT : None
 * RETUEN :  
 *          None 
 * -----------------------------------------------------------------
 */
void erase_config()
{

// KenC, 20031218 we already remove into mach_SMDK2510\time.c for init action
    int ret = 0;
    struct mtd_info *mtd_if;

    struct erase_info *erase=kmalloc(sizeof(struct erase_info),GFP_KERNEL);
    u_int32_t start_addr=0;
    mtd_if = __get_mtd_device(NULL,2); /* get mtd 2 */
    if (!erase)	{
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

            printk("Erase MTD 2 Address:%X Size:%d KBytes\n",erase->addr,erase->len/1024);

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

}
#endif


/* -----------------------------------------------------------------
 * ROUTINE NAME - gigabyte_do_led
 * -----------------------------------------------------------------
 * FUNCTION:  Provide a initail function to register a device driver
 *            To emulate the traffic behavior, wireless led will initial 
 *            on, and whenever traffic is here (GPIO_LED_STATUS_ON), will 
 *            become off and back to on next time cycle.
 *  
 * INPUT  : None    
 * OUTPUT : None
 * RETUEN :  
 *          None 
 * -----------------------------------------------------------------
 */
void gigabyte_do_led( void )
{
    // unsigned int tmp;

#if GPIO_UNIT_TEST
    static int nTestStatus = 0;
    if (nTestStatus == 0) {
        gpio_kernel_ioctl(GPIO_LED_CMD, GPIO_WRITE_ERROR_LED_ON);
        nTestStatus = 1;
    } else {
        gpio_kernel_ioctl(GPIO_LED_CMD, GPIO_WRITE_ERROR_LED_OFF);
        nTestStatus = 0;
    }
    printk("gigabyte_do_led: init button polling: %x\n", *pnInitButtonIoAddr & 0x00000004);
    nWirelessPacket++;
#endif

    /* check init button (low active) */
    /* remove into time.c */
#if 0
    tmp = *pnInitButtonIoAddr;
    tmp &= 0x00000020;
    if (!ResetConf_flag && tmp==0 ) {
        printk("gigabyte_do_led: init button polling: %x\n", *pnInitButtonIoAddr & 0x00000004);
        erase_config();
	ResetConf_flag = 1;
        ResetConf_task.routine = erase_config;
        schedule_task(&ResetConf_task);
        return;
    }    
#endif

    /* Process it at initial state */
    if ( nWirelessBlink == GPIO_LED_STATUS_INIT ) {
        if ( nWirelessPacket != 0 ) {   
            nWirelessPacket = 0;
            nWirelessBlink = GPIO_LED_STATUS_ON; 
            /* initial bit 6 to 1, check hw data sheet for detail */
            *pnLedIoAddr |= 0x00000040;
	} else {
            /* NOP */
        }
    } else if ( nWirelessBlink == GPIO_LED_STATUS_ON ) {
        if (nWirelessPacket != 0) { 
            nWirelessPacket = 0;
            nWirelessBlink = GPIO_LED_STATUS_OFF;
            /* set bit 6 back to 0 (light off), check hw data sheet for detail */
            *pnLedIoAddr &= 0xffffffbf;
        } else { 
            /* NOP */
        }
    } else if ( nWirelessBlink == GPIO_LED_STATUS_OFF ) {
        nWirelessPacket = 0;
        nWirelessBlink = GPIO_LED_STATUS_ON;
        /* set bit 6 back to 1 (light on), check hw data sheet for detail */
        *pnLedIoAddr |= 0x00000040;
    } else { 
        /* NOP */
    }
    mod_timer(&stTimer, jiffies + GPIO_LED_TIME_INTERVAL); /* for 40 msec interval */       
}

module_init (gigabyte_gpio_init);

