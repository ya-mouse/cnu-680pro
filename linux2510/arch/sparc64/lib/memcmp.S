/* $Id: memcmp.S,v 1.1.1.1 2003/11/17 02:33:24 jipark Exp $
 * Sparc64 optimized memcmp code.
 *
 * Copyright (C) 1997 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 * Copyright (C) 2000 David S. Miller (davem@redhat.com)
 */

	.text
	.align	32
	.globl	__memcmp, memcmp
__memcmp:
memcmp:
	cmp	%o2, 0		! IEU1	Group
loop:	be,pn	%icc, ret_0	! CTI
	 nop			! IEU0
	ldub	[%o0], %g5	! LSU	Group
	ldub	[%o1], %g3	! LSU	Group
	sub	%o2, 1, %o2	! IEU0
	add	%o0, 1, %o0	! IEU1
	add	%o1, 1, %o1	! IEU0	Group
	subcc	%g5, %g3, %g3	! IEU1	Group
	be,pt	%icc, loop	! CTI
	 cmp	%o2, 0		! IEU1	Group

ret_n0:	retl
	 mov	%g3, %o0
ret_0:	retl
	 mov	0, %o0
