#ifndef __H8300_UACCESS_H
#define __H8300_UACCESS_H

/*
 * User space memory access functions
 */
#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/segment.h>

#define VERIFY_READ	0
#define VERIFY_WRITE	1

/* We let the MMU do all checking */
extern inline int access_ok(int type, const void * addr, unsigned long size)
{
#define	RANGE_CHECK_OK(addr, size, lower, upper) \
	(((addr) >= (lower)) && (((addr) + (size)) < (upper)))

	extern unsigned long _ramend;
	return(RANGE_CHECK_OK((unsigned long) addr, size, 0L, (unsigned long)&_ramend));
}

extern inline int verify_area(int type, const void * addr, unsigned long size)
{
	return access_ok(type,addr,size)?0:-EFAULT;
}

/*
 * The exception table consists of pairs of addresses: the first is the
 * address of an instruction that is allowed to fault, and the second is
 * the address at which the program should continue.  No registers are
 * modified, so it is entirely up to the continuation code to figure out
 * what to do.
 *
 * All the routines below use bits of fixup code that are out of line
 * with the main instruction path.  This means when everything is well,
 * we don't even have to jump over them.  Further, they do not intrude
 * on our cache or tlb entries.
 */

struct exception_table_entry
{
	unsigned long insn, fixup;
};

/* Returns 0 if exception not found and fixup otherwise.  */
extern unsigned long search_exception_table(unsigned long);


/*
 * These are the main single-value transfer routines.  They automatically
 * use the right size if we just have the right pointer type.
 */

#define put_user(x, ptr)				\
({							\
    int __pu_err = 0;					\
    typeof(*(ptr)) __pu_val = (x);			\
    switch (sizeof (*(ptr))) {				\
    case 1:						\
	__put_user_asm( __pu_val, ptr, char);	        \
	break;						\
    case 2:						\
	__put_user_asm(__pu_val, ptr, short);      	\
	break;						\
    case 4:						\
	__put_user_asm(__pu_val, ptr, long);   	        \
	break;						\
    case 8:						\
	memcpy(ptr, &__pu_val, sizeof (*(ptr)));        \
	break;						\
    default:						\
	__pu_err = __put_user_bad();			\
	break;						\
    }							\
    __pu_err;						\
})
#define __put_user(x, ptr) put_user(x, ptr)

extern int __put_user_bad(void);

/*
 * Tell gcc we read from memory instead of writing: this is because
 * we do not write to any memory gcc knows about, so there are no
 * aliasing issues.
 */

#define __ptr(x) ((unsigned long *)(x))

#define __put_user_asm(x,ptr,size) *(unsigned size *)(ptr) = x

/*
 * Tell gcc we read from memory instead of writing: this is because
 * we do not write to any memory gcc knows about, so there are no
 * aliasing issues.
 */

#define get_user(x, ptr)					\
({								\
    int __gu_err = 0;						\
    typeof(*(ptr)) __gu_val = 0;				\
    switch (sizeof(*(ptr))) {					\
    case 1:							\
	__get_user_asm(__gu_val, ptr, char);	                \
	break;							\
    case 2:							\
	__get_user_asm(__gu_val, ptr, short);	                \
	break;							\
    case 4:							\
	__get_user_asm(__gu_val, ptr, long);   	                \
	break;							\
    case 8:							\
	memcpy(&__gu_val, ptr, sizeof (*(ptr))); \
	break;							\
    default:							\
	__gu_val = 0;						\
	__gu_err = __get_user_bad();				\
	break;							\
    }								\
    (x) = __gu_val;						\
    __gu_err;							\
})
#define __get_user(x, ptr) get_user(x, ptr)

extern int __get_user_bad(void);

#define __get_user_asm(x,ptr,size) x = *(unsigned size *)(ptr)

extern int __get_user_bad(void);

#define copy_from_user(to, from, n)		(memcpy(to, from, n), 0)
#define copy_to_user(to, from, n)		(memcpy(to, from, n), 0)

#define __copy_from_user(to, from, n) copy_from_user(to, from, n)
#define __copy_to_user(to, from, n) copy_to_user(to, from, n)

#define copy_to_user_ret(to,from,n,retval) ({ if (copy_to_user(to,from,n)) return retval; })

#define copy_from_user_ret(to,from,n,retval) ({ if (copy_from_user(to,from,n)) return retval; })

/*
 * Copy a null terminated string from userspace.
 */

static inline long
strncpy_from_user(char *dst, const char *src, long count)
{
	char *tmp;
	strncpy(dst, src, count);
	for (tmp = dst; *tmp && count > 0; tmp++, count--)
		;
	return(tmp - dst); /* DAVIDM should we count a NUL ?  check getname */
}

/*
 * Return the size of a string (including the ending 0)
 *
 * Return 0 on exception, a value greater than N if too long
 */
static inline long strnlen_user(const char *src, long n)
{
	return(strlen(src) + 1); /* DAVIDM make safer */
}

#define strlen_user(str) strnlen_user(str, 32767)

/*
 * Zero Userspace
 */

static inline unsigned long
clear_user(void *to, unsigned long n)
{
	memset(to, 0, n);
    return(0);
}

#endif /* _H8300_UACCESS_H */
