/* $Id: itlb_base.S,v 1.1.1.1 2003/11/17 02:33:24 jipark Exp $
 * itlb_base.S:	Front end to ITLB miss replacement strategy.
 *              This is included directly into the trap table.
 *
 * Copyright (C) 1996,1998 David S. Miller (davem@redhat.com)
 * Copyright (C) 1997,1998 Jakub Jelinek   (jj@ultra.linux.cz)
 */

#if PAGE_SHIFT == 13
/*
 * To compute vpte offset, we need to do ((addr >> 13) << 3),
 * which can be optimized to (addr >> 10) if bits 10/11/12 can
 * be guaranteed to be 0 ... mmu_context.h does guarantee this
 * by only using 10 bits in the hwcontext value.
 */
#define CREATE_VPTE_OFFSET1(r1, r2) \
				srax	r1, 10, r2
#define CREATE_VPTE_OFFSET2(r1, r2)
#define CREATE_VPTE_NOP		nop
#else /* PAGE_SHIFT */
#define CREATE_VPTE_OFFSET1(r1, r2) \
				srax	r1, PAGE_SHIFT, r2
#define CREATE_VPTE_OFFSET2(r1, r2) \
				sllx	r2, 3, r2
#define CREATE_VPTE_NOP
#endif /* PAGE_SHIFT */


/* Ways we can get here:
 *
 * 1) Nucleus instruction misses from module code.
 * 2) All user instruction misses.
 *
 * All real page faults merge their code paths to the
 * sparc64_realfault_common label below.
 */

/* ITLB ** ICACHE line 1: Quick user TLB misses		*/
	ldxa		[%g1 + %g1] ASI_IMMU, %g4	! Get TAG_ACCESS
	CREATE_VPTE_OFFSET1(%g4, %g6)			! Create VPTE offset
	CREATE_VPTE_OFFSET2(%g4, %g6)			! Create VPTE offset
	ldxa		[%g3 + %g6] ASI_P, %g5		! Load VPTE
1:	brgez,pn	%g5, 3f				! Not valid, branch out
	 nop						! Delay-slot
2:	stxa		%g5, [%g0] ASI_ITLB_DATA_IN	! Load PTE into TLB
	retry						! Trap return
3:	rdpr		%pstate, %g4			! Move into alternate globals

/* ITLB ** ICACHE line 2: Real faults			*/
	wrpr		%g4, PSTATE_AG|PSTATE_MG, %pstate
	rdpr		%tpc, %g5			! And load faulting VA
	mov		FAULT_CODE_ITLB, %g4		! It was read from ITLB
sparc64_realfault_common:				! Called by TL0 dtlb_miss too
	stb		%g4, [%g6 + AOFF_task_thread + AOFF_thread_fault_code]
	stx		%g5, [%g6 + AOFF_task_thread + AOFF_thread_fault_address]
	ba,pt		%xcc, etrap			! Save state
1:	 rd		%pc, %g7			! ...
	nop

/* ITLB ** ICACHE line 3: Finish faults + window fixups	*/
	call		do_sparc64_fault		! Call fault handler
	 add		%sp, STACK_BIAS + REGWIN_SZ, %o0! Compute pt_regs arg
	ba,pt		%xcc, rtrap_clr_l6		! Restore cpu state
	 nop
winfix_trampoline:
	rdpr		%tpc, %g3			! Prepare winfixup TNPC
	or		%g3, 0x7c, %g3			! Compute offset to branch
	wrpr		%g3, %tnpc			! Write it into TNPC
	done						! Do it to it

/* ITLB ** ICACHE line 4: Unused...	*/
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	CREATE_VPTE_NOP

#undef CREATE_VPTE_OFFSET1
#undef CREATE_VPTE_OFFSET2
#undef CREATE_VPTE_NOP
