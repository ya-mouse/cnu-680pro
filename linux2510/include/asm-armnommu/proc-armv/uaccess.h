/*
 *  linux/include/asm-arm/proc-armv/uaccess.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_PROC_UACCESS_H__
#define __ASM_PROC_UACCESS_H__

#include <asm/arch/memory.h>
#include <asm/proc/domain.h>

/*
 * Note that this is actually 0x1,0000,0000
 */
#define KERNEL_DS	0x00000000
/*
 * REVISIT_gjm
 * Be careful here for the DSC21.
 */
#define USER_DS		PAGE_OFFSET

/*
 * m68knommu defines this to be empty.
 * --gmcnutt
 */
/* ryc++
extern uaccess_t uaccess_user, uaccess_kernel;
    
extern __inline__ void set_fs (mm_segment_t fs)
{
    current->addr_limit = fs;
    current->thread.uaccess = fs == USER_DS ? &uaccess_user : &uaccess_kernel;
}
*/
extern __inline__ void set_fs (mm_segment_t fs)
{
}
/*
 * REVISIT_gjm -- __rnage_ok & __addr_ok
 *         Not sure if we might really want to do these checks in
 *         uClinux. For now I'm assuming they'll always succeed.
 */
#include <linux/config.h>
#ifdef CONFIG_UCLINUX
#define __range_ok(addr,size) 0
#define __addr_ok(addr) 1

#else
/* We use 33-bit arithmetic here... */
#define __range_ok(addr,size) ({ \
	unsigned long flag, sum; \
	__asm__("adds %1, %2, %3; sbcccs %1, %1, %0; movcc %0, #0" \
		: "=&r" (flag), "=&r" (sum) \
		: "r" (addr), "Ir" (size), "0" (current->addr_limit) \
		: "cc"); \
	flag; })

#define __addr_ok(addr) ({ \
	unsigned long flag; \
	__asm__("cmp %2, %0; movlo %0, #0" \
		: "=&r" (flag) \
		: "0" (current->addr_limit), "r" (addr) \
		: "cc"); \
	(flag == 0); })
#endif /* CONFIG_UCLINUX */

#define __put_user_asm_byte(x,addr,err)				\
	__asm__ __volatile__(					\
	"1:	strbt	%1,[%2],#0\n"				\
	"2:\n"							\
	"	.section .fixup,\"ax\"\n"			\
	"	.align	2\n"					\
	"3:	mov	%0, %3\n"				\
	"	b	2b\n"					\
	"	.previous\n"					\
	"	.section __ex_table,\"a\"\n"			\
	"	.align	3\n"					\
	"	.long	1b, 3b\n"				\
	"	.previous"					\
	: "=r" (err)						\
	: "r" (x), "r" (addr), "i" (-EFAULT), "0" (err))

#ifdef __ARMEB__
#define __put_user_asm_half(x,addr,err)				\
({								\
	unsigned long __temp = (unsigned long)(x);		\
	__put_user_asm_byte(__temp, (int)(addr) + 1, err);	\
	__put_user_asm_byte(__temp >> 8, addr, err);		\
})
#else
#define __put_user_asm_half(x,addr,err)				\
({								\
	unsigned long __temp = (unsigned long)(x);		\
	__put_user_asm_byte(__temp, addr, err);			\
	__put_user_asm_byte(__temp >> 8, (int)(addr) + 1, err);	\
})
#endif

#define __put_user_asm_word(x,addr,err)				\
	__asm__ __volatile__(					\
	"1:	strt	%1,[%2],#0\n"				\
	"2:\n"							\
	"	.section .fixup,\"ax\"\n"			\
	"	.align	2\n"					\
	"3:	mov	%0, %3\n"				\
	"	b	2b\n"					\
	"	.previous\n"					\
	"	.section __ex_table,\"a\"\n"			\
	"	.align	3\n"					\
	"	.long	1b, 3b\n"				\
	"	.previous"					\
	: "=r" (err)						\
	: "r" (x), "r" (addr), "i" (-EFAULT), "0" (err))

#define __get_user_asm_byte(x,addr,err)				\
	__asm__ __volatile__(					\
	"1:	ldrbt	%1,[%2],#0\n"				\
	"2:\n"							\
	"	.section .fixup,\"ax\"\n"			\
	"	.align	2\n"					\
	"3:	mov	%0, %3\n"				\
	"	mov	%1, #0\n"				\
	"	b	2b\n"					\
	"	.previous\n"					\
	"	.section __ex_table,\"a\"\n"			\
	"	.align	3\n"					\
	"	.long	1b, 3b\n"				\
	"	.previous"					\
	: "=r" (err), "=&r" (x)					\
	: "r" (addr), "i" (-EFAULT), "0" (err))
	
#ifdef __ARMEB__
#define __get_user_asm_half(x,addr,err)				\
({								\
	unsigned long __b1, __b2;				\
	__get_user_asm_byte(__b1, addr, err);			\
	__get_user_asm_byte(__b2, (int)(addr) + 1, err);	\
	(x) = (__b1 << 8) | __b2;				\
})
#else
#define __get_user_asm_half(x,addr,err)				\
({								\
	unsigned long __b1, __b2;				\
	__get_user_asm_byte(__b1, addr, err);			\
	__get_user_asm_byte(__b2, (int)(addr) + 1, err);	\
	(x) = __b1 | (__b2 << 8);				\
})
#endif


#define __get_user_asm_word(x,addr,err)				\
	__asm__ __volatile__(					\
	"1:	ldrt	%1,[%2],#0\n"				\
	"2:\n"							\
	"	.section .fixup,\"ax\"\n"			\
	"	.align	2\n"					\
	"3:	mov	%0, %3\n"				\
	"	mov	%1, #0\n"				\
	"	b	2b\n"					\
	"	.previous\n"					\
	"	.section __ex_table,\"a\"\n"			\
	"	.align	3\n"					\
	"	.long	1b, 3b\n"				\
	"	.previous"					\
	: "=r" (err), "=&r" (x)					\
	: "r" (addr), "i" (-EFAULT), "0" (err))

extern unsigned long __arch_copy_from_user(void *to, const void *from, unsigned long n);
#define __do_copy_from_user(to,from,n)				\
	(n) = __arch_copy_from_user(to,from,n)

extern unsigned long __arch_copy_to_user(void *to, const void *from, unsigned long n);
#define __do_copy_to_user(to,from,n)				\
	(n) = __arch_copy_to_user(to,from,n)

extern unsigned long __arch_clear_user(void *addr, unsigned long n);
#define __do_clear_user(addr,sz)				\
	(sz) = __arch_clear_user(addr,sz)

extern unsigned long __arch_strncpy_from_user(char *to, const char *from, unsigned long count);
#define __do_strncpy_from_user(dst,src,count,res)		\
	(res) = __arch_strncpy_from_user(dst,src,count)

extern unsigned long __arch_strnlen_user(const char *s, long n);
#define __do_strnlen_user(s,n,res)					\
	(res) = __arch_strnlen_user(s,n)

#endif /*  __ASM_PROC_UACCESS_H__ */
