/*
 * linux/gigabyte_gpio.h
 *
 * GPIO header file for LED access and board information.
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
 *    25Mar2003 Frank		Create
 *    18Dec2003 KenC		Modify for B41G, Samsung platform
 * 
 */
#ifndef _GIGABYTE_GPIO_H
#define _GIGABYTE_GPIO_H

/* Header file */


#define  GIGABYTE_GPIO_MINOR      0      /* minor number assignment         */
#define  GPIO_LED_TIME_INTERVAL   4      /*  40 msec for led status check   */     

/* LED data bit definition */
#define  LED_STATUS_FLAG      (0x00000001) 
#define  LED_WIRELESS_FLAG    (0x00000002) 

/* IO address definition */
#define  LED_IO_ADDRESS        (0xA4000000)
#define  JUMPER_J6_IO_ADDRESS  (0xA8000000) 

/* Board number definition */
#define  BOARD_NUMBER_BRW101AB        0x4 /* Frank, 01Apr2003 : added for new board number */
#define  BOARD_NUMBER_BRW404_A_OR_B   0x5 /*Frank 21-04-03 : Modified the board number  */
#define  BOARD_NUMBER_AP102AB         0x0
#define  BOARD_NUMBER_MASK            0xE /* Only this three bits for board number */
#define  BOARD_NUMBER_OFFSET          0x1 /* Offset the first bit, first bit is for Boot process */
#define  BOARD_NUMBER_B41G            0x6 /* temperary use */

/* LED ON OFF status definition */
#define  GPIO_LED_STATUS_OFF          0x00
#define  GPIO_LED_STATUS_ON           0x01
#define  GPIO_LED_STATUS_INIT         0x02

/* Command tpye definition */
#define  GPIO_BOARD_INFO              0x01
#define  GPIO_LED_INFO                0x02
#define  GPIO_BOARD_CMD               0x01 /* Use new definition command type name */
#define  GPIO_LED_CMD                 0x02
#define  SWAP_CMD                     0x04 /* by jkglee, to fix problem after wlan0 up in msh init.
					      then other system command will get an memory allocate 
					      error response.
					      Solution, after wlan0 up. disable swap implement in
					      get_page. Check page_alloc2.c for more detail */

/* Read and write command */
#define  GPIO_READ_BOARD_NUMBER		0x01
#define  GPIO_READ_ERROR_LED		0x02
#define  GPIO_READ_WIRELESS_LED		0x03
#define  GPIO_WRITE_ERROR_LED_ON	0x05
#define  GPIO_WRITE_ERROR_LED_OFF	0x06
#define  GPIO_WRITE_WIRELESS_LED_ON	0x07
#define  GPIO_WRITE_WIRELESS_LED_OFF	0x08 

/* swap cmd */
#define WAKEUP_SWAPD			0x01
#define DISABLE_SWAPD_IN_GET_PAGE	0x02

/* Return vaule definition, others value are true */
#define  GPIO_RETURN_FAIL       0xff
#define  GPIO_RETURN_OK         0x01
  
/* EXPORTED SUBPROGRAM DECLARATIONS
 */
extern unsigned char gigabyte_gpio_ioctl(struct inode *inode, struct file *file,unsigned char ucGPIOType,unsigned char ucRWMode);
extern unsigned char gpio_kernel_ioctl(unsigned char ucGPIOType,unsigned char ucRWMode); 
extern int  gigabyte_gpio_open(struct inode *inode, struct file *file);
extern void gigabyte_gpio_release(struct inode *inode, struct file *file);
extern int  gigabyte_gpio_init(void);
extern void gigabyte_do_led(void);

#endif
