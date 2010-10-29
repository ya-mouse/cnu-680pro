/*
 * envm-s3c2510.c
 *
 * environment variable read/write ....
 *
 */


#include <linux/types.h>
#include <linux/errno.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/module.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/arch/irq.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/bitops.h>
#include <asm/delay.h>


#include <asm/uaccess.h>
#include <linux/types.h>
#include <asm/ioctl.h>

#include <linux/config.h>
#include <asm/page.h>
#include <asm/arch/hardware.h>


#include <linux/ctype.h>
#include "envm-s3c2510.h"

/* definitions */
#define ENVM_IOC_MAGIC	'g'
#define ENVM_IOCRESET		_IO(ENVM_IOC_MAGIC, 0)
#define ENVM_IOCSETNAME 	_IO(ENVM_IOC_MAGIC, 1)
#define ENVM_IOCSETENV  	_IO(ENVM_IOC_MAGIC, 2)
#define ENVM_IOCGETENV  	_IO(ENVM_IOC_MAGIC, 3)
#define ENVM_IOCSAVEENV		_IO(ENVM_IOC_MAGIC, 4)
#define ENVM_IOCGETBUF		_IO(ENVM_IOC_MAGIC, 5)

#define ENVM_IOC_MAXNR 5



#define CFG_ENV_SIZE            0x2000			/* Total Size of Env */
#define CONFIG_NR_DRAM_BANKS    1			/* we have 1 banks of DRAM */
#define bi_env_data bi_env->data
#define bi_env_crc  bi_env->crc


#define PHYS_FLASH_1            0x80000000		/* Flash Bank #1 for S3C2510 */
#ifdef CONFIG_BOARD_SMDK2510
#define PHYS_FLASH_SIZE         0x00200000		/* 2 MB */
#define CFG_ENV_ADDR		(PHYS_FLASH_1 + 0x4000) /* Addr of Environment Sector */
#endif
#ifdef CONFIG_BOARD_2510REF
// ryc-- because AP ref.board have 2M flash rom
//#define PHYS_FLASH_SIZE			0x00400000		/* 4MB  for 2510 REF board */
//#define CFG_ENV_ADDR		(PHYS_FLASH_1 + 0x2000) /* Addr of Environment Sector */
#define PHYS_FLASH_SIZE         0x00200000		/* 2 MB */
#define CFG_ENV_ADDR		(PHYS_FLASH_1 + 0x4000) /* Addr of Environment Sector */
#endif
#define CFG_LOAD_ADDR           0x0500000               /* default load address */

#define CFG_MAX_FLASH_BANKS     1			/* max number of memory banks */
#define CFG_FLASH_BASE          PHYS_FLASH_1

#define Z_NULL  0					/* for initializing zalloc, zfree, opaque */

#ifdef CONFIG_BOARD_SMDK2510
#define FLASH_BANK_SIZE		0x200000
#endif
#ifdef CONFIG_BOARD_2510REF
#define FLASH_BANK_SIZE		0x400000
#endif
#define MAIN_SECT_SIZE  	0x10000
#define PARAM_SECT_SIZE 	0x2000


/*
 *  * Split minors in two parts
 *   */
#define TYPE(dev)   (MINOR(dev) >> 4)  /* high nibble */
#define NUM(dev)    (MINOR(dev) & 0xf) /* low  nibble */


typedef struct environment_s
{
	ulong   crc;                    /* CRC32 over data bytes        */
        unchar   data[CFG_ENV_SIZE - sizeof(ulong)];
} env_t;

struct bd_info_ext
{
    /* helper variable for board environment handling
     *
     * env_crc_valid == 0    =>   uninitialised
     * env_crc_valid  > 0    =>   environment crc in flash is valid
     * env_crc_valid  < 0    =>   environment crc in flash is invalid
     */
     int        env_crc_valid;
};

typedef struct bd_info
{
	int		bi_baudrate;    /* serial console baudrate */
	unsigned long	bi_ip_addr;             /* IP Address */
	unsigned char	bi_enetaddr[6]; /* Ethernet adress */
	env_t		*bi_env;
	ulong		bi_arch_number; /* unique id for this board */
	ulong		bi_boot_params; /* where this board expects params */
	
	struct                              /* RAM configuration */
	{
		ulong start;
		ulong size;
	} bi_dram[CONFIG_NR_DRAM_BANKS];

	struct bd_info_ext  bi_ext;         /* board specific extension */
} bd_t;  

/* global variables */
#define    MAX_ENVNAME_LEN     256
#define    MAX_ENVVALUE_LEN    256
char envname[MAX_ENVNAME_LEN];
char envvalue[MAX_ENVVALUE_LEN];

int envm_major =   ENVM_MAJOR;


bd_t bd_body;
bd_t *bd = &bd_body;
bd_t bd_newbody;
bd_t *bd_new = &bd_newbody;

env_t env_body;
ulong load_addr = CFG_LOAD_ADDR;                /* Default Load Address */

flash_info_t    flash_info[CFG_MAX_FLASH_BANKS];

const ulong crc_table[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};


#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);


ulong crc32(ulong crc, unchar *buf, uint len)
{
	if (buf == Z_NULL)
	    	return 0L;
#ifdef  DYNAMIC_CRC_TABLE
	if (crc_table_empty)
	    	make_crc_table();
#endif
	crc = crc ^ 0xffffffffL;
	while (len >= 8) {
	    	DO8(buf);
		len -= 8;
	}

	if (len) do {
	    	DO1(buf);
	} while (--len);

	return crc ^ 0xffffffffL;
}



static int check_crc(bd_t *bd)
{
	if (bd->bi_ext.env_crc_valid == 0) {
	    	env_t *env = (env_t *)CFG_ENV_ADDR;

		if (crc32(0, env->data, sizeof(env->data)) == env->crc)
			bd->bi_ext.env_crc_valid = 1;
                else
                        bd->bi_ext.env_crc_valid = -1;
	}

	return bd->bi_ext.env_crc_valid > 0;
}



unchar *board_env_getaddr(bd_t * bd, int index)
{
	env_t *env = (env_t *)CFG_ENV_ADDR;

	/* check environment crc */
	if (index < sizeof(env->data) && check_crc(bd))
	    	return &env->data[index];
	return 0;
}


static unchar *get_env_addr(bd_t *bd, int index)
{
	unchar *p = 0;

	/* use RAM copy, if possible */


	if (bd->bi_env) {
	    	if (index < sizeof(bd->bi_env_data))
		    	p = &bd->bi_env_data[index];
		else
			panic("bad size for get_env_char! %s[%d]\n", __FILE__, __LINE__);
	}
	else {
		/* try a board specific lookup */

		if ((p = board_env_getaddr(bd, index)) == 0) {
			printk("no env.\n");
                }
	}
	return p;
}



static unchar get_env_char(bd_t *bd, int index)
{
	unchar c=0;

	/* use RAM copy, if possible */

	if (bd->bi_env) {
        	if (index < sizeof(bd->bi_env_data))
		    	c = bd->bi_env_data[index];
	} 
	else {
	    	printk("no bi_env\n");
		
	}

	return c;
}


static int envmatch (bd_t *bd, unchar *s1, int i2)
{
	unsigned int i, i2save;
	char *saving;
	unsigned int saving_length = 0;
	unsigned int saved_length = 0;

	saving = s1;
	i2save = i2;

	while( *saving++ != '\0')
		saving_length++;

	while( get_env_char(bd, i2++) != '\0')
		saved_length++;

	while( get_env_char(bd, --i2) != '=')
		saved_length--;

	if ( saving_length != (saved_length) )
		return -1;

	i2 = i2save;
	for(i = 0; i < saving_length; i++)
	{
		if( *s1++ != get_env_char(bd, i2++))
			return -1;
	}

	return(i2);
}

int getenv (bd_t * bd, unchar *name, int *env_addr )
{
	int i, nxt;

	for (i=0; get_env_char(bd, i) != '\0'; i=nxt+1) {
	    	
	    	int val; 
		for (nxt=i; get_env_char(bd, nxt) != '\0'; ++nxt) {
			if (nxt >= sizeof(bd->bi_env_data)) {
			    	return -1;
			}
                }
                if ((val=envmatch(bd, name, i)) < 0)
                        continue;

		*env_addr= (int)get_env_addr(bd, val);
		return 0;
	}
	return -1;
}


int _do_setenv (bd_t *bd, int flag, int argc, char *argv[])
{
	int   i, len, oldval;
	unchar *env, *nxt = 0;
	unchar *name;

	/* need writable copy in RAM */
	if (!bd->bi_env_data)
	    	return -1;

	name = argv[1];

	/*
	 * search if variable with this name already exists
	 */

	oldval = -1;

	for (env = bd->bi_env_data; *env; env = nxt+1) {
	    	for (nxt = env; *nxt; ++nxt)
		    	;
		if ((oldval = envmatch(bd, name, (ulong)env - (ulong)bd->bi_env_data)) >= 0)
		    	break;
	}

    
	/*
	 * Delete any existing definition
	 */

	if (oldval >= 0) {
#ifndef CONFIG_ENV_OVERWRITE

        	/*
	         * Ethernet Address and serial# can be set only once
        	 */
	        if ( (strcmp (name, "serial#") == 0) ||
        	    ((strcmp (name, "ethaddr") == 0)
# if defined(CONFIG_OVERWRITE_ETHADDR_ONCE) && defined(CONFIG_ETHADDR)
	             && (strcmp (get_env_addr(bd, oldval),MK_STR(CONFIG_ETHADDR)) != 0)
# endif /* CONFIG_OVERWRITE_ETHADDR_ONCE && CONFIG_ETHADDR */
        	     ) )
		{
        		printk ("Can't overwrite \"%s\"\n", name);
			return -1;
        	}
#endif

	        if (*++nxt == '\0') {
	    		if ((ulong)env > (ulong)bd->bi_env_data) {
			    	env--;
			} 
			else {
			    	*env = '\0';
			}
		} 
		else {
	    		for (;;) {
			    	*env = *nxt++;
				if ((*env == '\0') && (*nxt == '\0'))
			    		break;
				++env;
			}
        	}
	        *++env = '\0';
	}

	
	/* Delete only ? */
	if ((argc < 3) || argv[2] == NULL) {
	    	/* Update CRC */
	    	bd->bi_env_crc = crc32(0, bd->bi_env_data, sizeof(bd->bi_env_data));
		return 0;
	}

    
	/*
	 *Append new definition at the end
	 */

	for (env = bd->bi_env_data; *env || *(env+1); ++env)
	    	;

	if ((ulong)env > (ulong)bd->bi_env_data)
	    	++env;

	/*
	 * Overflow when:
	 * "name" + "=" + "val" +"\0\0"  > sizeof(bd->bi_env_data) - (env-bd->bi_env_data)
	 */

	len = strlen(name) + 2;

	/* add '=' for first arg, ' ' for all others */
	for (i=2; i<argc; ++i) {
	    	len += strlen(argv[i]) + 1;
	}

	if (len > sizeof(bd->bi_env_data)) {
	    	printk ("## Error: environment overflow, \"%s\" deleted\n", name);
		return -1;
	}

	while ((*env = *name++) != '\0')
	    	env++;

	for (i=2; i<argc; ++i) {
	    	char *val = argv[i];

		*env = (i==2) ? '=' : ' ';
		while ((*++env = *val++) != '\0')
		    	;
	}

	/* end is marked with double '\0' */
	*++env = '\0';

	/* Update CRC */
	bd->bi_env_crc = crc32(0, bd->bi_env_data, sizeof(bd->bi_env_data));

	/*
	 * Some variables should be updated when the corresponding
	 * entry in the enviornment is changed
	 */

	if (strcmp(argv[1],"ethaddr") == 0) {
	    	char *s = argv[2];      /* always use only one arg */
		char *e;

		for (i=0; i<6; ++i) {
		    	bd->bi_enetaddr[i] = s ? simple_strtoul(s, &e, 16) : 0;
			if (s) 
			    	s = (*e) ? e+1 : e;
		}

		return 0;
	}

	if (strcmp(argv[1],"ipaddr") == 0) {
	    	char *s = argv[2];      /* always use only one arg */
		char *e;

		bd->bi_ip_addr = 0;
		for (i=0; i<4; ++i) {
		    	ulong val = s ? simple_strtoul(s, &e, 10) : 0;
			bd->bi_ip_addr <<= 8;
			bd->bi_ip_addr  |= (val & 0xFF);
			if (s)
			    	s = (*e) ? e+1 : e;
		}

		return 0;
	}

	if (strcmp(argv[1],"loadaddr") == 0) {
	    	load_addr = simple_strtoul(argv[2], NULL, 16);
		return 0;
	}
	

#if (CONFIG_COMMANDS & CFG_CMD_NET)
	if (strcmp(argv[1],"bootfile") == 0) {
	    	copy_filename (BootFile, argv[2], sizeof(BootFile));
		return 0;
	}
#endif  /* CFG_CMD_NET */

	return 0;
}



flash_info_t *addr2info (ulong addr)
{
	flash_info_t *info;
	int i;
	
	for (i=0, info=(flash_info_t *)&flash_info[0]; i<CFG_MAX_FLASH_BANKS; ++i, ++info)
	{
		if (info->flash_id != FLASH_UNKNOWN &&
			addr >= info->start[0] &&
	            /* WARNING - The '- 1' is needed if the flash
        	     * is at the end of the address space, since
	             * info->start[0] + info->size wraps back to 0.
        	     * Please don't change this unless you understand this.
	             */
        		addr <= info->start[0] + info->size - 1) 
		{
			return (info);
                }
	}
	
	return (NULL);
}

void Delay1(unsigned int nSec)
{
        unsigned int i;

        while(nSec--)
        {
                i=20;
                while(i--) ;
        }
}
void Delay1Sec(unsigned int nSec)
{
        unsigned int i;

        while(nSec--)
        {
                printk("\n");
                i=200000;
                while(i--) ;
        }
}


void FlashDelay(unsigned int  nusecs)
{
        ulong j, k, i=0x1234;

        while(nusecs--)
        {
                for (j=0; j<30; j++)
                        k = j * i;
        }
}

int FlashCompareData( unsigned int BaseAddr, unsigned int SrcAddr, unsigned int SrcSize )
{
        unsigned short *src, *tgt;
        unsigned int i;
        unsigned int size;

        src = (unsigned short *)SrcAddr;
        tgt = (unsigned short *)BaseAddr;

        size  = SrcSize/sizeof(unsigned short);

        for ( i=0; i<size; i++, src++, tgt++ ) {
                if ( *tgt != *src ) {
                        printk("\n>> Data Compare Error!\n");
                        return(ERR_PROG_ERROR);
                }
	}
        return(ERR_OK);
}


static int write_word (flash_info_t *info, ulong dest, ushort data)
{
    	unsigned short *addr;
	u16 barf;
	int rc = ERR_OK;
	//int flag;

        unsigned short  *Flash;

        addr = (unsigned short *)dest ;

	//    flag = disable_interrupts();

	Flash = (unsigned short *)CFG_FLASH_BASE;

        *(Flash+0x555 ) = 0xAA;
        Delay1(1);
        *(Flash+0x2AA ) = 0x55;
        Delay1(1);
        *(Flash+0x555 ) = 0xA0;
        Delay1(1);

        *addr = (unsigned short)data;

        FlashDelay(10);

        *((unsigned short *)(CFG_FLASH_BASE)) = 0xF0;


    /* arm simple, non interrupt dependent timer */
    //reset_timer_masked();

    /* wait while polling the status register */

#if 0
        if (get_timer_masked() > CFG_FLASH_WRITE_TOUT)
        {
                rc = ERR_TIMOUT;
                Log(DEBUG,"ZZZ");
                return rc;
        }
#endif

        //if(FlashCompareData((unsigned int)dest, dest, 1))
	if(FlashCompareData((unsigned int)dest, (int)&data, 1)) {
                barf = *addr;
                printk("Flash write error 0x%08x at address 0x%08x\n",(u32)barf, (int)dest);
                rc = ERR_PROG_ERROR;
                return rc;
        }

	return rc;
}



int write_buff (flash_info_t *info, unchar *src, ulong addr, ulong cnt)
{
	ulong cp, wp;
	int l;
	int i, rc;
	ushort data=0;

	wp = (addr & ~3);   /* get lower word aligned address */

	/*
	 * handle unaligned start bytes
	 */

	if ((l = addr - wp) != 0) {
		data = 0;
                for (i=0, cp=wp; i<l; ++i, ++cp)
		    	data = (data >> 8) | (*(unchar *)cp << 24);

                for (; i<4 && cnt>0; ++i) {
			data = (data >> 8) | (*src++ << 24);
			--cnt;
			++cp;
                }

                for (; cnt==0 && i<4; ++i, ++cp)
		    	data = (data >> 8) | (*(unchar *)cp << 24);

		if ((rc = write_word(info, wp, data)) != 0)
		    	return (rc);

                wp += 4;
	}

	/*
	 * handle word aligned part
	 */

	while (cnt >= 2) {
	    	data = *((unsigned short *)src);
		
		if ((rc = write_word(info, addr, data)) != 0)
                        return (rc);

                src += 2;
                addr  += 2;
                cnt -= 2;
        }

	if (cnt == 0)
                return ERR_OK;


	/*
	 * handle unaligned tail bytes
	 */

	data = 0;
	for (i=0, cp=wp; i<4 && cnt>0; ++i, ++cp) {
                data = (data >> 8) | (*src++ << 24);
                --cnt;
	}

	for (; i<4; ++i, ++cp)
                data = (data >> 8) | (*(unchar *)cp << 24);

	return write_word(info, wp, data);
}




int flash_write (unchar *src, ulong addr, ulong cnt)
{
	int i;
	ulong end = addr + cnt - 1;
	flash_info_t *info_first = addr2info (addr);
	flash_info_t *info_last  = addr2info (end );
	flash_info_t *info;

	if (cnt == 0)
		return (ERR_OK);

	if (!info_first || !info_last){
		return (ERR_INVAL);
	}

   
	for (info = info_first; info <= info_last; ++info) {
                ulong b_end = info->start[0] + info->size;      /* bank end addr */
                short s_end = info->sector_count - 1;

                for (i=0; i<info->sector_count; ++i) {
		    	ulong e_addr = (i == s_end) ? b_end : info->start[i + 1];

			if ((end >= info->start[i]) && (addr < e_addr) &&(info->protect[i] != 0)) {
			    	return (ERR_PROTECTED);
			}
                }
	}

	/* finally write data to flash */
	for (info = info_first; info <= info_last && cnt>0; ++info) {
	    	ulong len;

                len = info->start[0] + info->size - addr;
                if (len > cnt) {
                        len = cnt;
                }
		
                if ((i = write_buff(info, src, addr, len)) != 0) {
		    	return (i);
                }
                cnt  -= len;
                addr += len;
                src  += len;
	}
	
	return (ERR_OK);
}

int flash_erase (flash_info_t *info, int s_first, int s_last)
{
	int prot, sect;
	int rc = ERR_OK;
	//int flag;

	if (info->flash_id == FLASH_UNKNOWN)
	    	return ERR_UNKNOWN_FLASH_TYPE;

	if ((s_first < 0) || (s_first > s_last))
	    	return ERR_INVAL;

	if ((info->flash_id & FLASH_VENDMASK) !=(SAMSUNG_MANUFACT & FLASH_VENDMASK))
	    	return ERR_UNKNOWN_FLASH_VENDOR;

	prot = 0;

	for (sect=s_first; sect<=s_last; ++sect) {
                if (info->protect[sect])
                        prot++;
	}

	if (prot) 
	    	return ERR_PROTECTED;
	
	/* 
	 * Disable interrupts which might cause a timeout
	 * here. Remember that our exception vectors are
	 * at address 0 in the flash, and we don't want a
	 * (ticker) exception to happen while the flash
	 * chip is in programming mode.
	 */

	//flag = disable_interrupts();
	/* Start erase on unprotected sectors */
	for (sect = s_first; sect<=s_last /*&& !ctrlc()*/; sect++) {
	    	unsigned short  *Flash;
                unsigned long   temp;
                unsigned long   temp_E;
                unsigned long   SectorAddr;

                Flash = (unsigned short *)(CFG_FLASH_BASE);

		printk("Erasing sector %2d ... ", sect);
		/* arm simple, non interrupt dependent timer */
                /*reset_timer_masked();*/

		Delay1(5);
		SectorAddr = flash_info[0].start[sect]; /* total address */
		*(Flash+0x555) = 0xAA;
		Delay1(1);
		*(Flash+0x2AA) = 0x55;
		Delay1(1);
                *(Flash+0x555) = 0x80;
                Delay1(1);
                *(Flash+0x555) = 0xAA;
                Delay1(1);
                *(Flash+0x2AA) = 0x55;
                Delay1(1);

                *((volatile unsigned short *)(SectorAddr)) = 0x30;

                temp_E = SectorAddr;
                Delay1Sec(1);
                temp = (*((volatile unsigned short *)(temp_E)) & 0x00FF);
                while( temp != 0x00FF ) {
                        temp = (*((volatile unsigned short *)(temp_E)) & 0x00FF);
                }

                *((volatile unsigned short *)(CFG_FLASH_BASE)) = 0xF0;
	}
	/* if (ctrlc())
		printk("User Interrupt!\n"); */

#if 0

outahere:
	/* allow flash to settle - wait 10 ms */
	udelay_masked(10000);
   
	if (flag)
	    	enable_interrupts();
#endif 
	return rc;
}


int flash_sect_erase (ulong addr_first, ulong addr_last)
{
	flash_info_t *info;
	ulong bank;
	int s_first, s_last;
	int erased;
	int rc = ERR_OK;

	erased = 0;

	for (bank=0,info = &flash_info[0]; bank < CFG_MAX_FLASH_BANKS; ++bank, ++info) {
                ulong b_end;
                int sect;

                if (info->flash_id == FLASH_UNKNOWN)
                        continue;

                b_end = info->start[0] + info->size - 1; /* bank end addr */

                s_first = -1;           /* first sector to erase        */
                s_last  = -1;           /* last  sector to erase        */

                for (sect=0; sect < info->sector_count; ++sect) {
		    	ulong end;              /* last address in current sect */
                	short s_end;
		
			s_end = info->sector_count - 1;
			end = (sect == s_end) ? b_end : info->start[sect + 1] - 1;

			if (addr_first > end)
			    	continue;
			if (addr_last < info->start[sect])
			    	continue;
			if (addr_first == info->start[sect])
			    	s_first = sect;
			if (addr_last  == end)
			    	s_last  = sect;
		}

		if (s_first>=0 && s_first<=s_last) {
		    	erased += s_last - s_first + 1;
			rc = flash_erase (info, s_first, s_last);
                }
                else
                        return ERR_ALIGN;

                if (rc)
                        return rc;
	}
	return erased ? erased : rc;
}

int board_env_save(bd_t *bd, env_t *env, int size)
{
	int rc;
	ulong start_addr, end_addr;

	start_addr = CFG_ENV_ADDR;
	end_addr   = start_addr + CFG_ENV_SIZE - 1;

/*	rc = flash_sect_protect(0, CFG_ENV_ADDR, end_addr);

	if (rc < 0)
		return rc;
*/
	rc = flash_sect_erase(start_addr, end_addr);

	/*if (rc < 0)
	{
		flash_sect_protect(1, start_addr, end_addr);
		flash_perror(rc);
		return rc;
	}*/

	/*env->crc = crc32(0, env->data, sizeof(env->data));*/
	printk("Saving Environment to Flash...\n");
	rc = flash_write((unchar*)env, start_addr, size);

	if (rc < 0)
		printk("Flash write error.\n");
		//flash_perror(rc);
	else
		printk("done.\n");

/*	(void)flash_sect_protect(1, start_addr, end_addr);*/

	return 0;
}



#if 0 /*wbk*/
int setenv (bd_t * bd, char *varname, char *varvalue)
{
    	int ret;
    	char *argv[4] = { "setenv", varname, varvalue, NULL };
	ret = _do_setenv (bd, 0, 3, argv);
	return ret;
}
#endif

#if 1
int setenv (bd_t * bd, char *varname, char *varvalue)
{
        int ret;
        char *argv[4] = { "setenv", varname, varvalue, NULL };

        if(!strcmp(varvalue, "-"))
                ret = _do_setenv (bd, 0, 2, argv);
        else
                ret = _do_setenv (bd, 0, 3, argv);
        return ret;
}
#endif

int envm_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, char *buf)
{
	int ret;
	int env_addr;

	if (_IOC_TYPE(cmd) != ENVM_IOC_MAGIC) {
	    	printk("ENVM: wrong magic:0x%x.\n", _IOC_TYPE(cmd));
		return -ENOTTY;
	}
        if (_IOC_NR(cmd) > ENVM_IOC_MAXNR){
	    	printk("ENVM: exceed max number, cmd = 0x%x.\n", _IOC_NR(cmd));
	    	return -ENOTTY;
	}

	
        switch (cmd)
        {
                case ENVM_IOCSETNAME:
			memcpy(envname, buf, MAX_ENVNAME_LEN);
			return 0;

		
                case ENVM_IOCSETENV:
			memcpy(envvalue, buf, MAX_ENVVALUE_LEN);
			ret = setenv(bd, envname, envvalue);
                        return ret;

		case ENVM_IOCSAVEENV:
			board_env_save(bd, bd->bi_env, sizeof(env_t));
			return 0;

		case ENVM_IOCGETBUF:
			*(int*)buf = (int)(bd->bi_env_data);
			return 0;
						
                default:
			printk("ENVM: No matching cmd[0x%lx]\n", (int)cmd);
			return -ENOTTY;
        }
	return 0;
}

int envm_count = 0;
spinlock_t envm_lock;

int envm_open(struct inode *inode, struct file *filp)
{
	spin_lock(&envm_lock);
	
	if (envm_count) {
		spin_unlock(&envm_lock);
		return -EBUSY; /* already open */
	}

	envm_count++;
	spin_unlock(&envm_lock);
	MOD_INC_USE_COUNT;
	return 0;          /* success */
}

int envm_release(struct inode *inode, struct file *filp)
{
	envm_count--; /* release the device */
	MOD_DEC_USE_COUNT;
	return 0;
}



struct file_operations envm_fops = {
    	ioctl:      envm_ioctl,
	open:       envm_open,
	release:    envm_release
};

ulong flash_init(bd_t *bd)
{
	int i, j;
	ulong size = 0;

/* for AMD flash */
	for (i = 0; i < CFG_MAX_FLASH_BANKS; i++) {
	    	ulong flashbase = 0;
		flash_info[i].flash_id = (SAMSUNG_MANUFACT & FLASH_VENDMASK) 
		    			|(SAMSUNG_ID_K8B1616UBA & FLASH_TYPEMASK);

		flash_info[i].size = FLASH_BANK_SIZE;
		flash_info[i].sector_count = CFG_MAX_FLASH_SECT;
                memset(flash_info[i].protect, 0, CFG_MAX_FLASH_SECT);

                flashbase = PHYS_FLASH_1;
#ifdef CONFIG_BOARD_SMDK2510
		flash_info[i].start[0] = flashbase;
		flash_info[i].start[1] = flashbase + 0x4000;
                flash_info[i].start[2] = flashbase + 0x6000;
                flash_info[i].start[3] = flashbase + 0x8000;

                for (j = 4; j < flash_info[i].sector_count; j++) {
			flash_info[i].start[j] = flashbase + 0x10000 + (j - 4)*MAIN_SECT_SIZE;
                }
#endif
#ifdef CONFIG_BOARD_2510REF
		for (j = 0; j < flash_info[i].sector_count; j++)
		{
			if ((0 <= j)&&(j <= 7))
				flash_info[i].start[j] = flashbase + j * PARAM_SECT_SIZE;
			else
				flash_info[i].start[j] = flashbase + (j-7)*MAIN_SECT_SIZE;
		}
#endif

                size += flash_info[i].size;
        }

	return size;
}

int envm_load(void)
{

        char *envaddr = (char *)(CFG_ENV_ADDR);

        printk("ENVM: Loading ENV Memory.\n");

        flash_init(bd);
        bd->bi_env = &env_body;

        memcpy((char*)bd->bi_env_crc, envaddr, 4);
        envaddr +=4;
        memcpy(bd->bi_env_data, envaddr, (CFG_ENV_SIZE -4));

	return 0;
}



int envm_init(void)
{
    	int result;
	
	printk("ENVM: Register envm driver.\n");
        result = register_chrdev(envm_major, "envm", &envm_fops);
        if (result < 0) {
                printk(KERN_WARNING "envm: can't get major %d\n",envm_major);
                return result;
        }

        if (envm_major == 0)
                envm_major = result; /* dynamic */

        return result;
}



/* for kernel level calls */

char *envm_read(char *name)
{
	int ret;
	int addr;

	ret = getenv(bd, name, &addr);
	if (ret < 0)		/* error occurred */
		return ret;
	else
		return (char *)addr;

}

int envm_write(char *name, char *value)
{
	return setenv(bd, name, value);
}

#if 0
/* string conversion */
unsigned long simple_strtoul(const char *cp,char **endp,unsigned int base)
{
        unsigned long result = 0,value; 

        if (*cp == '0') {
                cp++;
                if ((*cp == 'x') && isxdigit(cp[1])) {
                        base = 16;
                        cp++;
                }
                if (!base) {
                        base = 8;
                }
        }
        if (!base) {
                base = 10;
        }
        while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp-'0' : (islower(*cp)
            ? toupper(*cp) : *cp)-'A'+10) < base) {
                result = result*base + value;
                cp++;
        }
        if (endp)
                *endp = (char *)cp;
        return result;
}

long simple_strtol(const char *cp,char **endp,unsigned int base)
{
        if(*cp=='-')
                return -simple_strtoul(cp+1,endp,base);
        return simple_strtoul(cp,endp,base);
}

#endif
