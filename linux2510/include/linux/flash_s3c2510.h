//-----------------------------------------------------------------------------
// File		: flash_s3c2510.h
// Title 	: Define structures and constants for flash device driver
// Author	: Jongil Park <lamainaz@samsung.co.kr>
// Revision :
// 1.0	Define the structure of saving flash information, FlashID
// 		Define the ioctl command.
// 		Define the FLASH_BASE_ADDR. this is dependent on s5n8947.
// 		so if you want to use this program in other chip, you have to fix this
// 		constant.
//-----------------------------------------------------------------------------
#ifndef	__FLASH_S3C2510_H__
#define __FLASH_S3C2510_H__

#include <linux/kernel.h>
#define EXPORT_SYMTAB
#include <linux/module.h>

//#ifdef CONFIG_MODVERSIONS
#define MODVERSIONS
//#include <linux/modversions.h>
//#endif

#include <linux/fs.h>
#include <linux/kdev_t.h>
#ifdef __KERNEL__
#include <asm/uaccess.h>
#endif
#include <asm/errno.h>
#include <linux/init.h>
#include <linux/slab.h>

#define D_10    0x10
#define D_30    0x30
#define D_50    0x50
#define D_55    0x55
#define D_80    0x80
#define D_90    0x90
#define D_A0    0xA0
#define D_AA    0xAA
#define D_F0    0xF0
#define D_FF    0xFFFF

#define FLASH_BASE_ADDR		FLASH_MEM_BASE	// ROMBANK 0 Base Addr for S3C2510
#define FLASH_MEM_BUSY          0x10000     	// IOPDATA[16] set as an input

#define IMAGE_START_ADDRESS        0x1000
#define DEVICE_NAME                "cflash"
#define BUFF_SIZE                  1024*16

typedef unsigned int    UINT;
typedef unsigned short  USHORT;
typedef unsigned long   ULONG;

// Flash ID check
typedef struct FlashID 
{
	UINT    ManuID;                 // Maunfacture ID
    	UINT    DeviceID;               // Device ID
    	UINT    FLASH_MAX_SECTOR_NUM;   // Maximum Sector Number
    	UINT    FlashSectorAddr[100];	// sector address
	UINT	uiCurSectNum;		// start img sector
    	UINT    uiWriteAddr;            // currently writeable addr
	UINT	Detectfail;
} FlashID;

#define FL_MAGIC        	'g'
#define FL_ERASE_SECTOR   _IO(FL_MAGIC, 0)
#define FL_INTG_DISABLE   _IO(FL_MAGIC, 1)
#define FL_WT_ENABLE	  _IO(FL_MAGIC, 2)
#define FL_INTG_ENABLE    _IO(FL_MAGIC, 3)
#define FL_INT_ONLYETH    _IO(FL_MAGIC, 4)
#define FL_FUZE_IMAGE     _IO(FL_MAGIC, 5)
#define FL_MAXNR    		5

#undef DEBUG

#ifdef DEBUG
#define dprintk(format, args...) do { char __buf[100]; sprintf(__buf, format ,##args); printk(DEVICE_NAME " : %-60s %s[%d], %s()\n", __buf, __FILE__, __LINE__, __FUNCTION__ ); }while(0)
#else
#define dprintk(fmt, args...)
#endif

#endif /* __FLASH_S3C2510_H__ */
