/* $Id: lshrdi3.S,v 1.1.1.1 2003/11/17 02:33:23 jipark Exp $ */

#include <asm/cprefix.h>

	.globl	C_LABEL(__lshrdi3)
C_LABEL(__lshrdi3):
	cmp	%o2, 0
	be	3f
	 mov	0x20, %g2

	sub	%g2, %o2, %g2
	cmp	%g2, 0
	bg	1f
	 srl	%o0, %o2, %o4

	clr	%o4
	neg	%g2
	b	2f
	 srl	%o0, %g2, %o5
1:
	sll  %o0, %g2, %g3
	srl  %o1, %o2, %g2
	or  %g2, %g3, %o5
2:
	mov  %o4, %o0
	mov  %o5, %o1
3:
	retl 
	 nop 
