#include <linux/module.h>
#include <linux/types.h>

#include <asm/io.h>

/*
 * Copy data from IO memory space to "real" memory space.
 * This needs to be optimized.
 */
void _memcpy_fromio(void * to, unsigned long from, size_t count)
{
	while (count) {
		count--;
		*(char *) to = readb(from);
		((char *) to)++;
		from++;
	}
}

/*
 * Copy data from "real" memory space to IO memory space.
 * This needs to be optimized.
 */
void _memcpy_toio(unsigned long to, const void * from, size_t count)
{
	while (count) {
		count--;
		writeb(*(char *) from, to);
		((char *) from)++;
		to++;
	}
}

//ryc++ for S3C2510 I/O
#ifdef CONFIG_PCI
void _memcpy_to_s3c2510_io(unsigned long to, const void * from, size_t count)
{
	while (count) {
		//count-=4;
		count = count - 4;
		/*
		printk("addr[%x],count[0x%x],*(long*)from = %x\n",
				to,count,*(long*)from);
		*/
		PCIMemWrite32(to,*(unsigned long *) from);
		if( (PCIMemRead32(to)) != *(unsigned long *)from)
			printk("data write error\n");
		/*
		read_data = PCIMemRead32(to);
		printk("addr[%x],read data = %x\n",to,read_data);
		*/
		((unsigned long *) from)++;
		//to++;
		//to+=4;
		to = to + 4;
	}
	//printk("count = %x\n",count);
}
#endif

/*
 * "memset" on IO memory space.
 * This needs to be optimized.
 */
void _memset_io(unsigned long dst, int c, size_t count)
{
	while (count) {
		count--;
		writeb(c, dst);
		dst++;
	}
}

EXPORT_SYMBOL(_memcpy_fromio);
EXPORT_SYMBOL(_memcpy_toio);
#ifdef CONFIG_PCI
EXPORT_SYMBOL(_memcpy_to_s3c2510_io); //ryc++
#endif
EXPORT_SYMBOL(_memset_io);
