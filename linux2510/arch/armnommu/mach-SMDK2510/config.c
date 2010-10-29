/*
 *  linux/arch/armnommu/$(MACHINE)/config.c
 *
 *  Copyright (C) 1993 Hamish Macdonald
 *  Copyright (C) 1999 D. Jeff Dionne
 *  Copyright (C) 2001-2003 Oleksandr Zhadan
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <stdarg.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/init.h>
#include <asm/uCbootstrap.h>
#include <asm/arch/hardware.h>

#if defined(CONFIG_SUPPORT_ENVM)
#include <linux/envm.h>
#endif
#undef CONFIG_DEBUG

unsigned char *sn;

extern unsigned int arm940_get_creg_0(void);
extern unsigned int arm940_get_creg_1(void);
extern unsigned int arm940_get_creg_2(unsigned int);
extern unsigned int arm940_get_creg_3(void);
extern unsigned int arm940_get_creg_5(unsigned int);
extern unsigned int arm940_get_creg_6_0(void);
extern unsigned int arm940_get_creg_6_1(void);
extern unsigned int arm940_get_creg_6_2(void);
extern unsigned int arm940_get_creg_6_3(void);
extern unsigned int arm940_get_creg_6_4(void);
extern unsigned int arm940_get_creg_6_5(void);
extern unsigned int arm940_get_creg_6_6(void);
extern unsigned int arm940_get_creg_6_7(void);
extern void arm940_uncache_region_4(unsigned int, unsigned int);
extern void arm940_uncache_region_5(unsigned int, unsigned int);
extern void arm940_uncache_region_6(unsigned int, unsigned int);
extern void arm940_uncache_region_7(unsigned int, unsigned int);

unsigned char *get_MAC_address(char *);

unsigned int CPU_CLOCK=0;  		/* CPU clock	*/
unsigned int SYS_CLOCK=0;  		/* System clock	*/
unsigned int CONFIG_ARM_CLK=0;

void	get_system_clock(void)
{
    unsigned int status = *(unsigned int *)CLKST;
    unsigned int pll = *(unsigned int *)SYSCFG;

    if	 ( pll & 0x80000000 ) {
	 CPU_CLOCK = (*(unsigned int *)CPLL & 0xff)+8;
         switch (status>>30) {
		case 0:	    SYS_CLOCK = CPU_CLOCK;
			    break;
		case 1:
		case 2:	    SYS_CLOCK = CPU_CLOCK >> 1;
			    break;
		case 3:	    SYS_CLOCK = (*(unsigned int *)SPLL & 0xff)+8;
			    break;
		}
	 }
    else {
	 pll = (status & 0x00000fff);
	 CPU_CLOCK += ((pll&0xF00)>>8)*100;
	 CPU_CLOCK += ((pll&0x0F0)>>4)*10;
	 CPU_CLOCK += ((pll&0x00F)>>0)*1;

	 switch (status>>30) {
		case 0:	    SYS_CLOCK = CPU_CLOCK;
			    break;
		case 1:
		case 2:	    SYS_CLOCK = CPU_CLOCK >> 1;
			    break;
		case 3:	    pll = ((status & 0x00fff000)>>12);
			    SYS_CLOCK += ((pll&0xF00)>>8)*100;
			    SYS_CLOCK += ((pll&0x0F0)>>4)*10;
			    SYS_CLOCK += ((pll&0x00F)>>0)*1;
			    break;
		}
	}

    CONFIG_ARM_CLK = (SYS_CLOCK*1000000);
    asm("nop");
}

void config_BSP()
{
  sn = (unsigned char *)"123456789012345";
# ifdef	CONFIG_DEBUG
    printk("Serial number [%s]\n",sn);
# endif

#if defined(CONFIG_SUPPORT_ENVM)
	envm_load();
#endif
			
#ifdef CONFIG_UNCACHED_MEM
    {
    extern void set_top_uncached_region(void);
    set_top_uncached_region();
    }
#endif
}

static inline unsigned char str2hexnum(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return 0; /* foo */
}

static inline void str2eaddr(unsigned char *ea, unsigned char *str)
{
	int i;
	str++;
	for (i = 0; i < 6; i++) {
		unsigned char num;

	if(*str == ':')
		str++;
		num = str2hexnum(*str++) << 4;
		num |= (str2hexnum(*str++));
		ea[i] = num;
	}
}

// choish_shared_lib_porting, replace global variable from get_MAC_address function
static unsigned char getptr0[20], getptr1[20];

unsigned char *get_MAC_address(char *eth_name)
{
//	unsigned char getptr0[20], getptr1[20];
	unsigned char *get_env_str0, *get_env_str1;
	int i;

	unsigned char ethaddr0[18], ethaddr0_i[17] = {"00:09:30:28:12:22"};
	unsigned char ethaddr1[18], ethaddr1_i[17] = {"00:09:30:28:12:24"};
	unsigned char eth0_str[9], eth0_str_i[8] = {"ethaddr0"};
	unsigned char eth1_str[9], eth1_str_i[8] = {"ethaddr1"};

	memcpy(ethaddr0, ethaddr0_i, sizeof(ethaddr0_i));
	memcpy(ethaddr1, ethaddr1_i, sizeof(ethaddr1_i));
	ethaddr0[17] = NULL;
	ethaddr1[17] = NULL;

	memcpy(eth0_str, eth0_str_i, sizeof(eth0_str_i));
	memcpy(eth1_str, eth1_str_i, sizeof(eth1_str_i));
	eth0_str[8] = NULL;
	eth1_str[8] = NULL;

	
	if(!strcmp(eth_name, "eth0"))
	{
		get_env_str0 = (unsigned char *)envm_read("HWADDR0");
		if(get_env_str0 == -1)
		{
			/*
			envm_write("ethaddr0",ethaddr0 );
			get_env_str0 = envm_read("ethaddr0");
			*/
			printk("ethaddr0 not found in env value. default set!\n");
			get_env_str0 = ethaddr0;
		}
		printk("ethaddr0=%s\n", (unsigned char *)get_env_str0);
		str2eaddr(getptr0, get_env_str0);
		return (getptr0);
	}

	if(!strcmp(eth_name, "eth1"))
	{
		get_env_str1 = envm_read("HWADDR1");
		if(get_env_str1 == -1)
		{
			/*
			envm_write("ethaddr1",ethaddr1 );
			get_env_str1 = envm_read("ethaddr1");
			*/
			printk("ethaddr1 not found in env value. default set!\n");
			get_env_str1 = ethaddr1;
		}
		printk("ethaddr1=%s\n", (char *)get_env_str1);
		str2eaddr(getptr1, (char *)get_env_str1);
		return (getptr1);
		}

	return (NULL);

}

unsigned int reg_map[8] = { 1, 1, 1, 1, 0, 0, 0, 0 };

int	ask_s3c2500_uncache_region( void *addr, unsigned int size, unsigned int flag)
{
    static unsigned int reg_map[8] = { 1, 1, 1, 1, 0, 0, 0, 0 };
    unsigned int i, j, n;
    unsigned int baseandsize=0;
    
    if	((unsigned int)addr & (size-1))
	return	-1;				/* addr is not alligned */
	
    for ( n = 4; n < 8; n++ )
	if  ( !reg_map[n] ) {
	    for	( i = 11, j=0x1000; i < 31; i++, j<<=1 )
		if  ( j == size ) {
		    baseandsize = (i << 1);		/* Area size	 */
		    baseandsize |= ((unsigned int)addr);/* Base address  */
		    baseandsize |= 1;			/* Enable region */
		    switch ( n ) {
			    case 4 :	arm940_uncache_region_4(baseandsize, flag);
					break;
			    case 5 :	arm940_uncache_region_5(baseandsize, flag);
					break;
			    case 6 :	arm940_uncache_region_6(baseandsize, flag);
					break;
			    case 7 :	arm940_uncache_region_7(baseandsize, flag);
					break;
			    default :	return -1;
			    }
		    reg_map[n] = 1;
		    return 0;
		    }
	    return -2;	/* Size is not correct */
	    }
    return -3;		/* No free region found */
}

#ifdef CONFIG_UNCACHED_MEM

void	set_top_uncached_region()
{
#ifdef CONFIG_UNCACHED_256
unsigned int val = 0x23 | (END_MEM-0x40000);
#endif

#ifdef CONFIG_UNCACHED_512
unsigned int val = 0x25 | (END_MEM-0x80000);
#endif

#ifdef CONFIG_UNCACHED_1024
unsigned int val = 0x27 | (END_MEM-0x100000);
#endif

#ifdef CONFIG_UNCACHED_2048
unsigned int val = 0x29 | (END_MEM-0x200000);
#endif

#ifdef CONFIG_UNCACHED_4096
unsigned int val = 0x2B | (END_MEM-0x400000);
#endif

#ifdef CONFIG_UNCACHED_8192
unsigned int val = 0x2D | (END_MEM-0x800000);
#endif

    arm940_uncache_region_7(val, 0);
    reg_map[7] = 1;
}

#endif	/* CONFIG_UNCACHED_MEM */


void crinfo(void)
{
    unsigned int tmp, size, i;
    printk("\nCoprocessor register info:\n");
    
    tmp = arm940_get_creg_0();
    printk("ID code                  : %8x\n", tmp);
    
    tmp = arm940_get_creg_1();
    printk("Control register         : %8x\n", tmp);

    tmp = arm940_get_creg_2(0);
    printk("Data cacheable register  : %8x\n", tmp);

    tmp = arm940_get_creg_2(1);
    printk("Inst cacheable register  : %8x\n", tmp);

    tmp = arm940_get_creg_3();
    printk("Write buffer register    : %8x\n", tmp);

    tmp = arm940_get_creg_5(0);
    printk("Data access register     : %8x\n", tmp);

    tmp = arm940_get_creg_5(1);
    printk("Inst access register     : %8x\n", tmp);

    for	(i=0; i < 8; i++ ) {
	switch (i) {
	    case 0 :	tmp = arm940_get_creg_6_0();	break;
	    case 1 :	tmp = arm940_get_creg_6_1();	break;
	    case 2 :	tmp = arm940_get_creg_6_2();	break;
	    case 3 :	tmp = arm940_get_creg_6_3();	break;
	    case 4 :	tmp = arm940_get_creg_6_4();	break;
	    case 5 :	tmp = arm940_get_creg_6_5();	break;
	    case 6 :	tmp = arm940_get_creg_6_6();	break;
	    case 7 :	tmp = arm940_get_creg_6_7();	break;
	    }
	printk("Region%d: addr=%8x, ", i, (tmp & 0xfffff000));
	size = tmp>>1;
	size &= 0x1f;
	size -= 0xb;
	printk("size=%dK, ", (0x4<<size));
	if 	( tmp & 1 )	printk("enabled\n");
	else			printk("disabled\n");
	}
}

EXPORT_SYMBOL(ask_s3c2500_uncache_region);

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
