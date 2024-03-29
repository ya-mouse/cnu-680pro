/*
 * include/asm-v850/posix_types.h -- Kernel versions of standard types
 *
 *  Copyright (C) 2001,02  NEC Corporation
 *  Copyright (C) 2001,02  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by Miles Bader <miles@gnu.org>
 */

#ifndef __V850_POSIX_TYPES_H__
#define __V850_POSIX_TYPES_H__

typedef unsigned int	__kernel_dev_t;
typedef unsigned long	__kernel_ino_t;
typedef unsigned long long __kernel_ino64_t;
typedef unsigned int	__kernel_mode_t;
typedef unsigned int	__kernel_nlink_t;
typedef long		__kernel_off_t;
typedef long long	__kernel_loff_t;
typedef int		__kernel_pid_t;
typedef unsigned short	__kernel_ipc_pid_t;
typedef unsigned int	__kernel_uid_t;
typedef unsigned int	__kernel_gid_t;
typedef unsigned int	__kernel_size_t;
typedef int		__kernel_ssize_t;
typedef int		__kernel_ptrdiff_t;
typedef long		__kernel_time_t;
typedef long		__kernel_suseconds_t;
typedef long		__kernel_clock_t;
typedef int		__kernel_daddr_t;
typedef char *		__kernel_caddr_t;
typedef unsigned short	__kernel_uid16_t;
typedef unsigned short	__kernel_gid16_t;
typedef unsigned int	__kernel_uid32_t;
typedef unsigned int	__kernel_gid32_t;

/* Some bogus code depends on this; we don't care.  */
typedef __kernel_uid_t __kernel_old_uid_t;


typedef struct {
#if defined(__KERNEL__) || defined(__USE_ALL)
	int	val[2];
#else /* !defined(__KERNEL__) && !defined(__USE_ALL) */
	int	__val[2];
#endif /* !defined(__KERNEL__) && !defined(__USE_ALL) */
} __kernel_fsid_t;


#if defined(__KERNEL__) || !defined(__GLIBC__) || (__GLIBC__ < 2)

#include <asm/bitops.h>

#undef	__FD_SET
#define __FD_SET(fd, fd_set) \
  __set_bit (fd, (void *)&((__kernel_fd_set *)fd_set)->fds_bits)
#undef __FD_CLR
#define __FD_CLR(fd, fd_set) \
  __clear_bit (fd, (void *)&((__kernel_fd_set *)fd_set)->fds_bits)
#undef	__FD_ISSET
#define __FD_ISSET(fd, fd_set) \
  __test_bit (fd, (void *)&((__kernel_fd_set *)fd_set)->fds_bits)
#undef	__FD_ZERO
#define __FD_ZERO(fd_set) \
  memset (fd_set, 0, sizeof (*(fd_set *)fd_set))

#endif /* defined(__KERNEL__) || !defined(__GLIBC__) || (__GLIBC__ < 2) */

#endif /* __V850_POSIX_TYPES_H__ */
