//-----------------------------------------------------------------------------
// File 	: flash_s5n8947.c
// Tile 	: Flash Fusing Device Driver for s5n8947
// Author	: Jongil Park <laminaz@samsung.co.kr>
// Revision :
// 1.3  Now Some function is added for Huawei SNMP Spec
//      First, runflprog_flag is added. we notify the fact image uaged to cramfs
//      Second, cramfs_lookup is renewed.
// 1.2  Support for Fujitsu MBM29LV160BE
// 1.1  Support for STMicro M29W160DB Flash
// 1.0	The problem of FlashProgram() only supported the even size data.
// 		so, now fixed the loop count.
// 		In FlashProgram(), variable "Flash" is fixed. 
// 			Flash = (USHORT *)(FLASH_BASE_ADDR+FID.FlashSectorAddr[0]);
// 		Before Flash variable is changed by BaseAddr argument, so command is 
// 		incorrectly passed to the flash device.
// 		To allocate memory space for buffer, I used the __get_free_page(GFP_KERNEL);
// 		This program is based on EMFlash.c in diagnostic code
//-----------------------------------------------------------------------------

//hans
#define __KERNEL_SYSCALLS__
#include <linux/autoconf.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/unistd.h>

#include "linux/flash_s3c2510.h"
#include <linux/malloc.h>
#include <linux/fs.h>

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------
static FlashID  FID;
static int s_nMajor = 0;
static int s_bDeviceReadOpen = 0;
static int s_bDeviceWriteOpen = 0;
//static char gacBuff[BUFF_SIZE];
static char *gpBuff;
static DECLARE_WAIT_QUEUE_HEAD(s_wq);
int runflprog_flag;

//-----------------------------------------------------------------------------
// Declare Functions
//-----------------------------------------------------------------------------
static int device_open(struct inode *inode, struct file *filp);
static int device_release(struct inode *inode, struct file *filp);
//static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset);
static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset);
static int device_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
int CheckFlashID(void);
void ReturnNormalOP(void);
int SetAMDSector(UINT DeviceID);
int SetSSTSector(UINT DeviceID);
int SetSamsungSector(UINT DeviceID);
int SetATMELSector(UINT DeviceID);
UINT FlashProgram(UINT, UINT, UINT);
void FlashResetReadMode( void );
UINT FlashEraseSector(UINT BaseAddr, UINT SectorNum);
static void Delay1Sec(UINT);
static void FlashDelay(UINT nusecs);
static int FlashCompareData( UINT,UINT,UINT );
int AM29LV160_WAIT(void);
static void Delay1(unsigned int nSec);


//-----------------------------------------------------------------------------
// Device Operations
//-----------------------------------------------------------------------------

struct file_operations device_fops = 
{
	open 	: device_open,
	release : device_release,
	write 	: device_write,
	ioctl	: device_ioctl
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void Delay1(unsigned int nSec)
{
    unsigned int i;

    while(nSec--)
    {
        i=20;
        while(i--) ;
    }
}

//-----------------------------------------------------------------------------
// Fuction : CheckFlashID(void)
//-----------------------------------------------------------------------------
int CheckFlashID(void)
{
	USHORT  *Flash;
	int 	rc;

	FID.Detectfail = 0;

	// make addr into noncache
	Flash = (USHORT *)FLASH_BASE_ADDR;	

	// command to get Flash ID
	*(Flash+0x5555 ) = D_AA;
	    FlashDelay(5);
	*(Flash+0x2AAA ) = D_55;
	    FlashDelay(5);
	*(Flash+0x5555 ) = D_90;
	FlashDelay(5);

	FID.ManuID = *Flash;
	FID.DeviceID = *(Flash+0x1);

	ReturnNormalOP();

	printk("Manufacture ID = 0x%x\n", FID.ManuID);
	printk("Device ID      = 0x%x\n", FID.DeviceID);

	switch(FID.ManuID)
	{
		case 0x01:		// AMD AM29LV160D
		case 0x20:		// STMicro M29W160DB(bottom boot)
		case 0x04:		// Fujitsu MBM29LV160BE
		case 0xC2:		// Macronix 29LV160BTC-90
			rc = SetAMDSector(FID.DeviceID);
			return rc;
		case 0xBF:
			rc = SetSSTSector(FID.DeviceID);
			return rc;
		case 0xEC:
			rc = SetSamsungSector(FID.DeviceID);
			return rc;
		case 0x1F:
			rc = SetATMELSector(FID.DeviceID);
			return rc;
		default:
			printk("Use Default Sector Information (AMD AM29LV160D) !!! \n");
			rc = SetAMDSector(FID.DeviceID);
			return rc;
		//	FID.Detectfail = 1;
		//	return -1;
	}
}

//-----------------------------------------------------------------------------
// Function : ReturnNormalOP(void)
//-----------------------------------------------------------------------------
void ReturnNormalOP(void)
{
	USHORT    *Flash;

	Flash = (USHORT *)FLASH_BASE_ADDR;

	*(Flash+0x5555 ) = D_AA;
   	*(Flash+0x2AAA ) = D_55;
   	*(Flash+0x5555 ) = D_F0;
   	FlashDelay(10);
}

//-----------------------------------------------------------------------------
// Function : FlashDelay(UINT nusecs)
//-----------------------------------------------------------------------------
void FlashDelay(UINT nusecs)
{
    ULONG j, k, i=0x1234;

    while(nusecs--)
    {
        for (j=0; j<30; j++)
            k = j * i;
    }
}

//-----------------------------------------------------------------------------
// Function : SetAMDSector(UINT DeviceID)
// Descript : AMD flash memory sector setting
//-----------------------------------------------------------------------------
int SetAMDSector(UINT DeviceID)  
{
	USHORT 	selectID;
	UINT 	temp;
	int  	i;

	selectID = 0;

	if(FID.DeviceID==0x225B) 	selectID=1;   	// AMD flash, in case of AM29LV800BB
	else if(FID.DeviceID==0x2249) 	selectID=2;   	// AMD flash, in case of AM29LV160BB
	else if(FID.DeviceID==0x22F9) 	selectID=3;	// AMD flash AM29LV320DB
	else 
	{
		dprintk("Unknown AMD flash memory");
		return -1;
	}

   	switch(selectID)
   	{
		// in case of AM29LV800BB
   		case 1:    
   			temp = 0x10000;
   			FID.FLASH_MAX_SECTOR_NUM  = 19;
   			FID.FlashSectorAddr[0]=0x00000;
   			FID.FlashSectorAddr[1]=0x04000;
   			FID.FlashSectorAddr[2]=0x06000;
   			FID.FlashSectorAddr[3]=0x08000;
   			for(i=4; i<19; i++)
   			{
   				FID.FlashSectorAddr[i]=temp;
   				temp+=0x10000;
				if(FID.FlashSectorAddr[i] == 0x20000)
				{
					// sector addr for saving image: 0x20000
					FID.uiWriteAddr = FID.FlashSectorAddr[i];
					FID.uiCurSectNum = i;
				}
   			}
			for(i=0; i<19; i++)
			{
				dprintk("Flash Sector %d Addr [0x%x]", i, FID.FlashSectorAddr[i]);
			}
   			dprintk("AM29LV800BB detected!!");
   			break;

		// in case of AM29LV160
   		case 2:  
   			temp=0x10000;
   			FID.FLASH_MAX_SECTOR_NUM  = 35;
   			FID.FlashSectorAddr[0]=0x00000;
   			FID.FlashSectorAddr[1]=0x04000;
   			FID.FlashSectorAddr[2]=0x06000;
   			FID.FlashSectorAddr[3]=0x08000;
   			for(i=4; i<35; i++)
   			{
   				FID.FlashSectorAddr[i]=temp;
   				temp+=0x10000;
				if(FID.FlashSectorAddr[i] == 0x20000)
				{
					// sector addr for saving image: 0x20000
					FID.uiWriteAddr = FID.FlashSectorAddr[i];
					FID.uiCurSectNum = i;
				}
   			}
			for(i=0; i<35; i++)
			{
				dprintk("Flash Sector %d Addr [0x%x]", i, FID.FlashSectorAddr[i]);
			}
   			dprintk("AM29LV160DB detected!!");
   			break;

   		case 3:
   			temp=0x10000;
   			FID.FLASH_MAX_SECTOR_NUM  = 71;
   			FID.FlashSectorAddr[0]=0x00000;
   			FID.FlashSectorAddr[1]=0x02000;
   			FID.FlashSectorAddr[2]=0x04000;
   			FID.FlashSectorAddr[3]=0x06000;
   			FID.FlashSectorAddr[4]=0x08000;
   			FID.FlashSectorAddr[5]=0x0a000;
   			FID.FlashSectorAddr[6]=0x0c000;
   			FID.FlashSectorAddr[7]=0x0e000;
   			for(i=8; i<71; i++)
   			{
   				FID.FlashSectorAddr[i]=temp;
   				temp+=0x10000;
				if(FID.FlashSectorAddr[i] == 0x20000)
				{
					// sector addr for saving image: 0x20000
					FID.uiWriteAddr = FID.FlashSectorAddr[i];
					FID.uiCurSectNum = i;
				}
   			}
			for(i=0; i<71; i++)
			{
				dprintk("Flash Sector %d Addr [0x%x]", i, FID.FlashSectorAddr[i]);
			}
   			dprintk("AM29LV320DB detected!!");
   			break;
  	}
	return 1;
}

//-----------------------------------------------------------------------------
// Function : SetSamsungSector(UINT DeviceID)  
// Descript : Samsung flash memory sector setting
//-----------------------------------------------------------------------------
int SetSamsungSector(UINT DeviceID)  
{
	UINT temp,temp1;
	int  i;

	// samsung flash , in case of K8B1616UBA
   	if(FID.DeviceID != 0x2245) 
	{
		dprintk("Unknown Samsung NOR flash memory");     
		return -1;
	}
	
  	// in case of K8B1616UBA (Samsung Flash)
	temp=0x00000;
	temp1=0x10000;
	FID.FLASH_MAX_SECTOR_NUM  = 39;	
	
	for(i=0;i<8;i++)
	{           
		FID.FlashSectorAddr[i]=temp;
		temp+=0x2000;
	}                    
	for(i=8;i<39;i++)
	{
		FID.FlashSectorAddr[i]=temp1;
		temp1+=0x10000;
		if(FID.FlashSectorAddr[i] == 0x20000)
		{
			// sector addr for saving image: 0x20000
			FID.uiWriteAddr = FID.FlashSectorAddr[i];
			FID.uiCurSectNum = i;
		}
	}            
	dprintk("K8B1616UBA detected!!");
	return 1;
}

//-----------------------------------------------------------------------------
// Function : SetSSTSector(UINT DeviceID)  
// Descript : SST flash memory sector setting
//-----------------------------------------------------------------------------
int SetSSTSector(UINT DeviceID)  
{
	USHORT 	selectID;
	UINT 	temp;
	int  	i;

	temp=0x0;
	selectID=0;
   
	if(FID.DeviceID == 0x2781)      selectID=1;  // in case of SST39L(V)F800
	else if(FID.DeviceID == 0x2782) selectID=2;  // in case of SST39L(V)F160
	else 
	{
		dprintk("Unknown SST flash memory");
		return -1;
	}

   	switch(selectID)
	{
		// in case of SST39L(V)F800
		case 1:   
			FID.FLASH_MAX_SECTOR_NUM  = 16;	
		   	for(i=0; i<16; i++)
			{
				FID.FlashSectorAddr[i]=temp;
				temp+=0x10000;
				if(FID.FlashSectorAddr[i] == 0x20000)
				{
					// sector addr for saving image: 0x20000
					FID.uiWriteAddr = FID.FlashSectorAddr[i];
					FID.uiCurSectNum = i;
				}
			}           
		   	dprintk("SST39VF800A detected!!");
		  	break;

		// in case of SST39L(V)F160
		case 2:  
			FID.FLASH_MAX_SECTOR_NUM  = 63;	
			for(i=0;i<63;i++)
			{
				if(i <= 31)
				{
					FID.FlashSectorAddr[i]=i*0x800;
				}
				else
				{
					FID.FlashSectorAddr[i]=(i-31)*0x10000;
				}
				if(FID.FlashSectorAddr[i] == 0x20000)
				{
					// sector addr for saving image: 0x20000
					FID.uiWriteAddr = FID.FlashSectorAddr[i];
					FID.uiCurSectNum = i;
				}
			}
			dprintk("SST39VF160A detected!!");
			break;
   	}
	return 1;
}

//-----------------------------------------------------------------------------
// Function : SetATMELSector(UINT DeviceID)
// Descript : SST flash memory sector setting
//-----------------------------------------------------------------------------
int SetATMELSector(UINT DeviceID)
{
	UINT temp,temp1;
	int  i;

	if(FID.DeviceID != 0x00C8)
	{
		dprintk("Unknown ATMEL flash memory");
		return -1;
	}

	//AT49BV321
	temp=0x00000;
	temp1=0x10000;
   	FID.FLASH_MAX_SECTOR_NUM   =71;	
            
	for(i=0; i<8; i++)
	{
		FID.FlashSectorAddr[i]=temp;
		temp+=0x2000;
	}
	for(i=8; i<71; i++)
	{
		FID.FlashSectorAddr[i]=temp1;
		temp1+=0x10000;
		if(FID.FlashSectorAddr[i] == 0x20000)
		{
			// sector addr for saving image: 0x20000
			FID.uiWriteAddr = FID.FlashSectorAddr[i];
			FID.uiCurSectNum = i;
		}
	}
	dprintk("AT49BV321 detected!!");
	return 1;
}

//-----------------------------------------------------------------------------
// Module startup
//-----------------------------------------------------------------------------
static int __init init_flash_program(void)
{
	printk("Loading flash_s5n8947 Module\n");

    	s_nMajor = register_chrdev(0, DEVICE_NAME, &device_fops);
    	if (s_nMajor < 0) 
	{
		printk(DEVICE_NAME " : Device registration failed (%d)\n", s_nMajor);
		return s_nMajor;
    	}
	
    	printk(DEVICE_NAME " : Device registered with Major Number = %d\n", s_nMajor);
	return 0;
}

module_init(init_flash_program);

//-----------------------------------------------------------------------------
// Module cleanup
//-----------------------------------------------------------------------------
void cleanup_module(void)
{
    	int nRetCode;

    	printk("Unloading Mini Buf Module\n");
    
    	if ((nRetCode = unregister_chrdev(s_nMajor, DEVICE_NAME)) < 0)
	{
        	printk(DEVICE_NAME " : Device unregistration failed (%d)\n", nRetCode);
	}
}

//-----------------------------------------------------------------------------
// Open device
//-----------------------------------------------------------------------------
int device_open(struct inode *inode, struct file *filp)
{
	int 			rc;

    	dprintk(DEVICE_NAME " : Device open (%d, %d)\n", MAJOR(inode->i_rdev), MINOR(inode->i_rdev));
	gpBuff = (char *)__get_free_page(GFP_KERNEL);
	if(!gpBuff)
	{
		printk("Fail: no memory\n");
		return -ENOMEM;
	}
	else
	{
		memset(gpBuff, 0x00, PAGE_SIZE);
	}
	// clear the buffer for flash
	memset(&FID, 0x00, sizeof(FlashID));

    	if ((filp->f_flags & O_ACCMODE) & (O_WRONLY | O_RDWR)) 
	{
		if(s_bDeviceWriteOpen) 
		{
		    	printk(DEVICE_NAME " : Device already open for writing\n");
		    	return -EBUSY;
		} 
		else
		{
			++s_bDeviceWriteOpen;
		}
    	} 
	else 
	{
        	if (s_bDeviceReadOpen) 
		{
		    printk(DEVICE_NAME " : Device already open for reading\n");
		    return -EBUSY;
        	} 
		else
		{
            		++s_bDeviceReadOpen;
		}
    	}

	rc = CheckFlashID();
	if(rc < 0)
	{
		printk(DEVICE_NAME" : fail to check flash memory\n");
        	return -ENOMEDIUM;
	}

    	MOD_INC_USE_COUNT;

    	return 0;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
#if 1	// hans

#define ARMBOOT_SIZE	1024*128
#define IH_MAGIC	0x27051956      /* Image Magic Number       */
#define IH_NMLEN	32                      /* Image Name Length        */
#define BUFF_SIZE	1024

typedef struct image_header {   
    uint32_t    ih_magic;   /* Image Header Magic Number    */
    uint32_t    ih_hcrc;    /* Image Header CRC Checksum    */
    uint32_t    ih_time;    /* Image Creation Timestamp */
    uint32_t    ih_size;    /* Image Data Size      */
    uint32_t    ih_load;    /* Data  Load  Address      */
    uint32_t    ih_ep;      /* Entry Point Address      */
    uint32_t    ih_dcrc;    /* Image Data CRC Checksum  */
    uint8_t     ih_os;      /* Operating System     */
    uint8_t     ih_arch;    /* CPU architecture     */
    uint8_t     ih_type;    /* Image Type           */
    uint8_t     ih_comp;    /* Compression Type     */
    uint8_t     ih_name[IH_NMLEN];  /* Image Name       */
} image_header_t;

int fuze_image(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int		iFdFile;
	unsigned char	pacBuff[BUFF_SIZE];
	unsigned int	iFileSize, iReadLen, iWriteLen, iSectorNum, iLoopCount;
	unsigned int	i;

        int		error_flag=0;
	int		cool_size =0;

	struct file	*file;
	mm_segment_t	old_fs;

	unsigned char filename[64];

	copy_from_user(filename, (void *)arg, 64);

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	iFdFile = open(filename, O_RDONLY, 0);
	if(IS_ERR(iFdFile))
	{
		set_fs(old_fs);
		printk("Fail: open %s. return =0x%x\n", filename, iFdFile);
		return -ENOTTY;
	}
	set_fs(old_fs);

	file = fget(iFdFile);
	iFileSize = file->f_dentry->d_inode->i_size;
	if((FLASH_SIZE - ARMBOOT_SIZE) < (iFileSize-sizeof(image_header_t)))
	{
		printk("Fail : too large image file size\n");
		goto out_fput;
	}
	printk("Succ: Open %s: size[%d]\n", filename, iFileSize);

	// jipark: seek file position
	printk("Current Position: %ld\n", file->f_pos);
	file->f_pos = file->f_pos + sizeof(image_header_t);
	printk("Changed Position: %ld\n", file->f_pos);
			
	// ioctl( , FL_INTG_DISABLE);
	*((unsigned int *)(0x3FF4008))=0x800000;

	// rc = ioctl( , FL_ERASE_SECTOR, iFileSize);
	printk("Start erasing flash sector...\n");
	iSectorNum = iFileSize / (64*1024);
	if(iFileSize % (64*1024))
	{
		iSectorNum++;
	}
	dprintk("iFileSize [%d], iSectorNum [%d]", iFileSize, iSectorNum);
	printk("Erase Sector: ");
	for(i=0; i<iSectorNum; i++, FID.uiCurSectNum++)
	{
		printk("%2d ", FID.uiCurSectNum);
		FlashEraseSector(FLASH_BASE_ADDR, FID.uiCurSectNum);
		printk("\b\b\b");
	}
	printk("\nDone.\n");

	iLoopCount = iFileSize / BUFF_SIZE;
	if(iFileSize % BUFF_SIZE)
	{
		iLoopCount++;
	}
	else
	{
		cool_size = 1;
	}

	i=0;
	while(1)
	{
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		iReadLen = read(iFdFile, pacBuff, BUFF_SIZE);
		set_fs(old_fs);

		i++;
#if 1
		if(iReadLen <= 0)
		{
			if(iReadLen == 0)
			{
				printk("\nEnd of File...");
				break;
			}
			printk("Fail: can't read data in image data file [Len:%d]\n", iReadLen);
			goto out_fput;
		}
		else if(iReadLen != BUFF_SIZE)
		{
			if(cool_size) 
				goto out;
        
			error_flag++;   
		}
        
		if(error_flag>1)
			goto out;
#endif
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		iWriteLen = device_write(filp, pacBuff, iReadLen, &filp->f_pos);
		set_fs(old_fs);
		if(iWriteLen <= 0)
		{
			printk("Fail: can't write data in flash driver [Len:%d]\n", iWriteLen);
			goto out_fput;
		}
	}

	printk(" Done\n");

	//ioctl(iFdDevice, FL_INTG_ENABLE);
	*((unsigned short *)(0x3FF4008))=0x0;

	fput(file);

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	close(iFdFile);
	set_fs(old_fs);

//	sys_reboot(0xfee1dead, 672274793, 0x01234567);

// 	static void find_some_memory(int n)

	runflprog_flag = 1;
	if(runflprog_flag == 1)
	{
		struct nameidata nd;
		struct inode *inode;
		struct dentry	*file_dentry, *dir_dentry, *root_dentry;
		struct inode *root_inode, *dir_inode, *file_inode;
		char	buf[16];
		int err = 0;

		//-------------- /ETC ----------------------
		sprintf(buf, "/etc/inittab");
		if (path_init(buf, LOOKUP_FOLLOW|LOOKUP_POSITIVE, &nd))
			err = path_walk(buf, &nd);
		
		file_dentry = nd.dentry;
		dir_dentry = file_dentry->d_parent;
		root_dentry = dir_dentry->d_parent;
		
		root_inode = root_dentry->d_inode;
		dir_inode = dir_dentry->d_inode;
		file_inode = file_dentry->d_inode;

		if(cramfs_lookup_renew(root_inode, dir_dentry, dir_inode) != NULL)
			printk("cramfs inode(/etc) renew error\n");
		if(cramfs_lookup_renew(dir_inode, file_dentry, file_inode) != NULL)
			printk("cramfs inode(/etc/services) renew error\n");
		path_release(&nd);	
			
		//-------------- /DEV ----------------------

		sprintf(buf, "/dev/");
		if (path_init(buf, LOOKUP_FOLLOW|LOOKUP_POSITIVE, &nd))
			err = path_walk(buf, &nd);
		
		dir_dentry = nd.dentry;
		dir_inode = dir_dentry->d_inode;
		if(cramfs_lookup_renew(root_inode, dir_dentry, dir_inode) != NULL)
			printk("cramfs inode(/dev) renew error\n");
		path_release(&nd);	

		//-------------- /LIB/LIB1.SO ----------------------
		sprintf(buf, "/lib/lib1.so");
		if (path_init(buf, LOOKUP_FOLLOW|LOOKUP_POSITIVE, &nd))
			err = path_walk(buf, &nd);
		
		file_dentry = nd.dentry;
		dir_dentry = file_dentry->d_parent;
		root_dentry = dir_dentry->d_parent;
		
		root_inode = root_dentry->d_inode;
		dir_inode = dir_dentry->d_inode;
		file_inode = file_dentry->d_inode;

		if(cramfs_lookup_renew(root_inode, dir_dentry, dir_inode) != NULL)
			printk("cramfs inode(/lib/ renew error\n");
		if(cramfs_lookup_renew(dir_inode, file_dentry, file_inode) != NULL)
			printk("cramfs inode(/lib/lib1.so) renew error\n");
		path_release(&nd);	
	}
	return 1;

out:
	printk("\nError %d!\n", i);
out_fput:
	fput(file);

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	close(iFdFile);
	set_fs(old_fs);
	return -ENOTTY;
#else
	{
		unsigned char filename[50]="/var/webpassword.conf";
		int	len, curr, fd;
		unsigned char buff[200];
		mm_segment_t old_fs;

		curr =0;
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		fd = open(filename, O_RDONLY, 0);
		if(!IS_ERR(fd))
		{
			printk("FILE OPEN SUCCESS\n");
			while((len = read(fd, &buff[curr], 10))> 0)
				curr += len;
			close(fd);
		}else{
			printk("FILE OPEN FAILED\n");
		}
		set_fs(old_fs);

		buff[curr+1] = '\0';
		if(curr != 0)
			printk("length=%d, %s\n", curr, buff);
	}
#endif
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int device_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	int iFileSize;
	int iSectorNum;
	int i;

	dprintk(">>");
	dprintk("cmd = %x, arg = %ld", cmd, arg);
	if (_IOC_TYPE(cmd) != FL_MAGIC) 
	{
		printk(DEVICE_NAME" : wrong magic:0x%x.\n", _IOC_TYPE(cmd));
		return -ENOTTY;
    	}
    	if (_IOC_NR(cmd) > FL_MAXNR)
	{
		printk(DEVICE_NAME" : exceed max number, cmd = 0x%x.\n", _IOC_NR(cmd));
		return -ENOTTY;
	}

	switch(cmd)
	{
		case FL_ERASE_SECTOR:
			printk("Start erasing flash sector...\n");
			iFileSize = arg;
			iSectorNum = iFileSize / (64*1024);
			if(iFileSize % (64*1024))
			{
				iSectorNum++;
			}
			dprintk("iFileSize [%d], iSectorNum [%d]", iFileSize, iSectorNum);
   			printk("Erase Sector: ");
			for(i=0; i<iSectorNum; i++, FID.uiCurSectNum++)
			{
   				printk("%2d ", FID.uiCurSectNum);
				FlashEraseSector(FLASH_BASE_ADDR, FID.uiCurSectNum);
   				printk("\b\b\b");
			}
   			printk("\nDone.\n");
			return 1;
		case FL_INTG_DISABLE:
		//	*((unsigned short *)(0x3FF4008))=0x7FFE7F;
		//	*((unsigned short *)(0x3FF4008))=0xFFFEFF;
		//	*((unsigned int *)(0x3FF4008))=0xFFFEFF;
			*((unsigned int *)(0x3FF4008))=0x800000;
			return 1;

		case FL_INTG_ENABLE:
		//	*((unsigned short *)(0x3FF4008))=0xF06E;
			*((unsigned short *)(0x3FF4008))=0x0;
			return 1;
#if 1
		case FL_WT_ENABLE:
			*((unsigned short *)(0x3FF601C))=0x101;
			printk("\nWT Enable...\n");
			return 1;
#endif
		case FL_INT_ONLYETH:
			*((unsigned short *)(0x3FF4008))=0xF06E;
			return 1;

// hans
		case FL_FUZE_IMAGE:
			return fuze_image(filp, cmd, arg);

		default:
			return -ENOTTY;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int device_release(struct inode *inode, struct file *filp)
{
//	kfree(gpBuff);
	free_page((unsigned long)gpBuff);
    	printk(DEVICE_NAME " : Device release (%d, %d)\n", MAJOR(inode->i_rdev), MINOR(inode->i_rdev));
    	if ((filp->f_flags & O_ACCMODE) & (O_WRONLY | O_RDWR))
	{
        	--s_bDeviceWriteOpen;
	}
    	else
	{
        	--s_bDeviceReadOpen;
	}

    	MOD_DEC_USE_COUNT;

    	return 0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
ssize_t device_write(struct file *filp, const char *pcBuffer, size_t length, loff_t *offset)
{
	UINT 	FlashAddr;
	UINT 	MemAddr;
	int 	rc;
	int		flags;

	if(copy_from_user(gpBuff, pcBuffer, length))
//	if(copy_from_user(gacBuff, pcBuffer, length))
	{
		return -EFAULT;
	}
    	dprintk("copy data from user");

	if(FID.uiCurSectNum > FID.FLASH_MAX_SECTOR_NUM)
	{
    	printk(DEVICE_NAME " : Fail: Current sector number excess the MAX sector number\n");
		return -EFAULT;
	}

	FlashAddr = FLASH_BASE_ADDR + FID.uiWriteAddr;
//	MemAddr = (UINT)gacBuff;
	MemAddr = (UINT)gpBuff;

	save_flags(flags);
	cli();
	
    	dprintk("FlashAddr[0x%x], MemAddr[0x%x]", FlashAddr, MemAddr);
	rc = FlashProgram(FlashAddr, MemAddr, length);
	if(rc < 0)
	{
		return -EFAULT;
	}
	FID.uiWriteAddr = FID.uiWriteAddr + length;
	sti();
	restore_flags(flags);
	printk(".");

    return length;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
UINT FlashProgram(UINT BaseAddr, UINT SrcAddr, UINT SrcSize)
{
	//volatile USHORT	*src, *tgt, *Flash;
	USHORT	*src, *tgt, *Flash;
	UINT	i;
	int		iLoopCount;
	int		rc;

	src   = (USHORT *)SrcAddr;
	tgt   = (USHORT *)BaseAddr;
	//Flash = (USHORT *)(FLASH_BASE_ADDR+FID.FlashSectorAddr[0]);
	Flash = (USHORT *)FLASH_BASE_ADDR;
	iLoopCount  = SrcSize/sizeof(USHORT);
	if(SrcSize % sizeof(USHORT)) 
	{
		iLoopCount=iLoopCount+1;
	}

	dprintk("src [0x%p], tgt [0x%p], Flash [0x%p], size [%d]\n", src, tgt, Flash, SrcSize);
	dprintk("LoopCount [%d]\n", iLoopCount);
	
	for ( i=0; i<iLoopCount; i++, src++, tgt++ ) 
	{
		if(FID.ManuID == 0xBF)
		{
			*(Flash+0x5555 ) = D_AA;
			FlashDelay(1);
			*(Flash+0x2AAA ) = D_55;
			FlashDelay(1);
			*(Flash+0x5555 ) = D_A0;
			FlashDelay(1);
		}
		else
		{
			*(Flash+0x555 ) = D_AA;
			FlashDelay(1);
			*(Flash+0x2AA ) = D_55;
			FlashDelay(1);
			*(Flash+0x555 ) = D_A0;
			FlashDelay(1);
		}

		/* Write data to the next ROM address */
		*tgt = *src;
		FlashDelay(10);
	} 
	FlashResetReadMode();
	rc = FlashCompareData(BaseAddr, SrcAddr, SrcSize);
	return rc;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int FlashCompareData( UINT BaseAddr, UINT SrcAddr, UINT SrcSize )
{
	USHORT		*src, *tgt, *Flash;
	UINT		i;
	UINT		size;

	src   = (USHORT *)SrcAddr;
	tgt   = (USHORT *)BaseAddr;
	Flash = (USHORT *)BaseAddr;
	size  = SrcSize/sizeof(USHORT);

	for ( i=0; i<size; i++, src++, tgt++ ) 
	{
		if ( *tgt != *src ) 
		{
			printk(DEVICE_NAME " : Fail: Data Compare between FLASH and DRAM [%d][src:0x%x][tgt:0x%x]\n", i, *src, *tgt);
			return -1;
		}	
	}
	return 1;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void FlashResetReadMode( void )
{
	*((USHORT *)FLASH_BASE_ADDR) = D_F0;
	Delay1(1);
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
UINT FlashEraseSector(UINT BaseAddr, UINT SectorNum)
{
	USHORT	*Flash;
	UINT 	temp;
	ULONG 	temp_E;
	ULONG	SectorAddr;
	
	SectorAddr = FID.FlashSectorAddr[SectorNum]; 
	Flash = (USHORT *)FLASH_BASE_ADDR;

	dprintk(DEVICE_NAME ": Flash [0x%x], SectorAddr [0x%x]", Flash, SectorAddr);

	if(FID.ManuID==0xBF)
   	{
   		*(Flash+0x5555) = D_AA;
		FlashDelay(1);
   		*(Flash+0x2AAA) = D_55;
		FlashDelay(1);
   		*(Flash+0x5555) = D_80;
		FlashDelay(1);
   		*(Flash+0x5555) = D_AA;
		FlashDelay(1);
   		*(Flash+0x2AAA) = D_55;
		FlashDelay(1);
		if(SectorNum < 32)
		{
   			*((USHORT *)(FLASH_BASE_ADDR+SectorAddr)) = D_30;
		}
		else
		{
   			*((USHORT *)(FLASH_BASE_ADDR+SectorAddr)) = D_50;
		}
		FlashDelay(1);
	}
	else
	{
		*(Flash+0x555) = D_AA;
		FlashDelay(1);
		*(Flash+0x2AA) = D_55;
		FlashDelay(1);
		*(Flash+0x555) = D_80;
		FlashDelay(1);
		*(Flash+0x555) = D_AA;
		FlashDelay(1);
		*(Flash+0x2AA) = D_55;
		FlashDelay(1);
		*((USHORT *)(FLASH_BASE_ADDR+SectorAddr)) = D_30;
		FlashDelay(1);
	}

	// Required Time : Typ=1sec, Max=15sec.                                 */
    	temp_E = SectorAddr;  
	Delay1Sec(1);
	FlashResetReadMode();
	temp = (*((USHORT *)(FLASH_BASE_ADDR+temp_E)) & 0x00FF);
	while( temp != 0x00FF )
	{
		Delay1Sec(1);
		temp = (*((USHORT *)(FLASH_BASE_ADDR+temp_E)) & 0x00FF);
	}
	
//	if(FID.ManuID != 0x1F)
//		FlashResetReadMode();
	
	return 1;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void Delay1Sec(UINT nSec)
{
	UINT i; 

	while(nSec--)
	{
		i=200000;
		while(i--) ;
	}		
}

#if 0
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CheckFlashBusy(void)
{
	unsigned int 	uiVal;	

	uiVal = *((volatile unsigned int *)0x3FF500C);

	uiVal = ((uiVal >> 16) & 0x01);
	return uiVal;
}
#endif
