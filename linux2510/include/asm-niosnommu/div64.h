#ifndef __NIOS_DIV64
#define __NIOS_DIV64

/* wentao: We're not 64-bit, but many architectures do this way */
#define do_div(n,base) ({ \
	int __res; \
	__res = ((unsigned long) n) % (unsigned) base; \
	n = ((unsigned long) n) / (unsigned) base; \
	__res; })

#endif /* __NIOS_DIV64 */
