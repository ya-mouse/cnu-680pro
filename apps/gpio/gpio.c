#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>

#define	IOPMODE1	0xF0030000	/* I/O port mode select lower R		 */
#define	IOPMODE2	0xF0030004	/* I/O port mode select upper R		 */
#define	IOPCON1		0xF0030008	/* I/O port function select lower R	 */
#define	IOPCON2		0xF003000C	/* I/O port function select upper R	 */
#define	IOP4DMA		0xF0030010	/* I/O port special function R for DMA	 */
#define	IOP4xINT	0xF0030014	/* I/O port spec. function R for Ext.IRQ */
#define	IOP4xINT_PEND	0xF0030018	/* External IRQ clear R			 */
#define	IOPDATA1	0xF003001c	/* I/O port data R			 */
#define	IOPDATA2	0xF0030020	/* I/O port data R			 */
#define	IOPDRV1		0xF0030024	/* I/O port drive control R		 */
#define	IOPDRV2		0xF0030028	/* I/O port drive control R		 */

#define REG(x) *(unsigned long *)(x)

void usage(void)
{
    printf("Usage: gpio [-g GPIO_bit] [-s GPIO_bit] [-c GPIO_bit] [-v] [-h]\n");
    printf("This is tool for manipulating GPIO pins of S3C2510 SoC\n");
    printf("  -g GPIO_bit  get selected GPIO bit\n");
    printf("  -s GPIO_bit  set selected GPIO bit\n");
    printf("  -c GPIO_bit  clear selected GPIO bit\n");
    printf("  -d           dump GPIO registers\n");
    printf("  -v           verbose output, shows bit state\n");
    printf("  -h           shows this help\n");
}

int main(int argc, char **argv)
{
  int ret;

    if(argc < 2) { usage(); return -1; }
  
    while ((ret = getopt(argc, argv, "g:s:c:dvh")) != -1)
    {
	switch(ret)
	{
	    case 'g':
		printf("%s\n", (REG(IOPDATA1) & (1 << strtoul(optarg, NULL, 10))) ? "1" : "0");
		break;
	    case 's':
		REG(IOPDATA1) |= 1 << strtoul(optarg, NULL, 10);
		break;
	    case 'c':
		REG(IOPDATA1) &= ~(1 << strtoul(optarg, NULL, 10));
		break;
	    case 'd':
		printf("S3C2510 GPIO registers state:\n");
		printf("IOPMODE1: 0x%08lX\n", REG(IOPMODE1));
		printf("IOPMODE2: 0x%08lX\n", REG(IOPMODE2));
		printf("IOPCON1:  0x%08lX\n", REG(IOPCON1));
		printf("IOPCON2:  0x%08lX\n", REG(IOPCON2));
		printf("IOPDATA1: 0x%08lX\n", REG(IOPDATA1));
		printf("IOPDATA2: 0x%08lX\n", REG(IOPDATA2));
		printf("IOPDRV1:  0x%08lX\n", REG(IOPDRV1));
		printf("IOPDRV2:  0x%08lX\n", REG(IOPDRV2));
		break;
	    case 'v': 
		break;
	    case 'h':
		usage();
		break;
	    default:
		usage();
		return -1;
	}
    }

    return 0;
}
