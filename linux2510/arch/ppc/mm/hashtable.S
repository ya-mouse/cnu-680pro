/*
 * BK Id: %F% %I% %G% %U% %#%
 */
/*
 *  arch/ppc/kernel/hashtable.S
 *
 *  $Id: hashtable.S,v 1.1.1.1 2003/11/17 02:33:18 jipark Exp $
 *
 *  PowerPC version 
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *  Rewritten by Cort Dougan (cort@cs.nmt.edu) for PReP
 *    Copyright (C) 1996 Cort Dougan <cort@cs.nmt.edu>
 *  Adapted for Power Macintosh by Paul Mackerras.
 *  Low-level exception handlers and MMU support
 *  rewritten by Paul Mackerras.
 *    Copyright (C) 1996 Paul Mackerras.
 *
 *  This file contains low-level assembler routines for managing
 *  the PowerPC MMU hash table.  (PPC 8xx processors don't use a
 *  hash table, so this file is not used on them.)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *	
 */

#include <linux/config.h>
#include <asm/ppc_asm.h>
#include <asm/processor.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/cputable.h>
#include <asm/ppc_asm.h>
#include <kernel/ppc_defs.h>

#ifdef CONFIG_SMP
	.comm	hash_table_lock,4
#endif /* CONFIG_SMP */

/*
 * Load a PTE into the hash table, if possible.
 * The address is in r4, and r3 contains an access flag:
 * _PAGE_RW (0x400) if a write.
 * r23 contains the SRR1 value, from which we use the MSR_PR bit.
 * SPRG3 contains the physical address of the current task's thread.
 *
 * Returns to the caller if the access is illegal or there is no
 * mapping for the address.  Otherwise it places an appropriate PTE
 * in the hash table and returns from the exception.
 * Uses r0, r2 - r7, ctr, lr.
 */
	.text
	.globl	hash_page
hash_page:
#ifdef CONFIG_PPC64BRIDGE
	mfmsr	r0
	clrldi	r0,r0,1		/* make sure it's in 32-bit mode */
	MTMSRD(r0)
	isync
#endif
	tophys(r7,0)			/* gets -KERNELBASE into r7 */
#ifdef CONFIG_SMP
	addis	r2,r7,hash_table_lock@h
	ori	r2,r2,hash_table_lock@l
	mfspr	r5,SPRG3
	lwz	r0,PROCESSOR-THREAD(r5)
	oris	r0,r0,0x0fff
	b	10f
11:	lwz	r6,0(r2)
	cmpwi	0,r6,0
	bne	11b
10:	lwarx	r6,0,r2
	cmpwi	0,r6,0
	bne-	11b
	stwcx.	r0,0,r2
	bne-	10b
	isync
#endif
	/* Get PTE (linux-style) and check access */
	lis	r0,KERNELBASE@h		/* check if kernel address */
	cmplw	0,r4,r0
	mfspr	r2,SPRG3		/* current task's THREAD (phys) */
	ori	r3,r3,_PAGE_USER|_PAGE_PRESENT /* test low addresses as user */
	lwz	r5,PGDIR(r2)		/* virt page-table root */
	blt+	112f			/* assume user more likely */
	lis	r5,swapper_pg_dir@ha	/* if kernel address, use */
	addi	r5,r5,swapper_pg_dir@l	/* kernel page table */
	rlwimi	r3,r23,32-12,29,29	/* MSR_PR -> _PAGE_USER */
112:	add	r5,r5,r7		/* convert to phys addr */
	rlwimi	r5,r4,12,20,29		/* insert top 10 bits of address */
	lwz	r5,0(r5)		/* get pmd entry */
	rlwinm.	r5,r5,0,0,19		/* extract address of pte page */
#ifdef CONFIG_SMP
	beq-	hash_page_out		/* return if no mapping */
#else
	/* XXX it seems like the 601 will give a machine fault on the
	   rfi if its alignment is wrong (bottom 4 bits of address are
	   8 or 0xc) and we have had a not-taken conditional branch
	   to the address following the rfi. */
	beqlr-
#endif
	add	r2,r5,r7		/* convert to phys addr */
	rlwimi	r2,r4,22,20,29		/* insert next 10 bits of address */
	rlwinm	r0,r3,32-3,24,24	/* _PAGE_RW access -> _PAGE_DIRTY */
	ori	r0,r0,_PAGE_ACCESSED|_PAGE_HASHPTE

	/*
	 * Update the linux PTE atomically.  We do the lwarx up-front
	 * because almost always, there won't be a permission violation
	 * and there won't already be an HPTE, and thus we will have
	 * to update the PTE to set _PAGE_HASHPTE.  -- paulus.
	 */
retry:
	lwarx	r6,0,r2			/* get linux-style pte */
	andc.	r5,r3,r6		/* check access & ~permission */
#ifdef CONFIG_SMP
	bne-	hash_page_out		/* return if access not permitted */
#else
	bnelr-
#endif
	or	r5,r0,r6		/* set accessed/dirty bits */
	stwcx.	r5,0,r2			/* attempt to update PTE */
	bne-	retry			/* retry if someone got there first */

	mfsrin	r3,r4			/* get segment reg for segment */
	mr	r2,r8			/* we have saved r2 but not r8 */
	bl	create_hpte		/* add the hash table entry */
	mr	r8,r2

/*
 * htab_reloads counts the number of times we have to fault an
 * HPTE into the hash table.  This should only happen after a
 * fork (because fork does a flush_tlb_mm) or a vmalloc or ioremap.
 * Where a page is faulted into a process's address space,
 * update_mmu_cache gets called to put the HPTE into the hash table
 * and those are counted as preloads rather than reloads.
 */
	addis	r2,r7,htab_reloads@ha
	lwz	r3,htab_reloads@l(r2)
	addi	r3,r3,1
	stw	r3,htab_reloads@l(r2)

#ifdef CONFIG_SMP
	eieio
	addis	r2,r7,hash_table_lock@ha
	li	r0,0
	stw	r0,hash_table_lock@l(r2)
#endif

	/* Return from the exception */
	lwz	r3,_CCR(r21)
	lwz	r4,_LINK(r21)
	lwz	r5,_CTR(r21)
	mtcrf	0xff,r3
	mtlr	r4
	mtctr	r5
	lwz	r0,GPR0(r21)
	lwz	r1,GPR1(r21)
	lwz	r2,GPR2(r21)
	lwz	r3,GPR3(r21)
	lwz	r4,GPR4(r21)
	lwz	r5,GPR5(r21)
	lwz	r6,GPR6(r21)
	lwz	r7,GPR7(r21)
	/* we haven't used xer */
	mtspr	SRR1,r23
	mtspr	SRR0,r22
	lwz	r20,GPR20(r21)
	lwz	r22,GPR22(r21)
	lwz	r23,GPR23(r21)
	lwz	r21,GPR21(r21)
	RFI
	
#ifdef CONFIG_SMP
hash_page_out:
	eieio
	addis	r2,r7,hash_table_lock@ha
	li	r0,0
	stw	r0,hash_table_lock@l(r2)
	blr
#endif /* CONFIG_SMP */

/*
 * Add an entry for a particular page to the hash table.
 *
 * add_hash_page(unsigned context, unsigned long va, pte_t pte)
 *
 * We assume any necessary modifications to the pte (e.g. setting
 * the accessed bit) have already been done and that there is actually
 * a hash table in use (i.e. we're not on a 603).
 */
_GLOBAL(add_hash_page)
	mflr	r0
	stw	r0,4(r1)

	/* Convert context and va to VSID */
	mulli	r3,r3,897*16		/* multiply context by context skew */
	rlwinm	r0,r4,4,28,31		/* get ESID (top 4 bits of va) */
	mulli	r0,r0,0x111		/* multiply by ESID skew */
	add	r3,r3,r0		/* note create_hpte trims to 24 bits */

	/*
	 * We disable interrupts here, even on UP, because we don't
	 * want to race with hash_page, and because we want the
	 * _PAGE_HASHPTE bit to be a reliable indication of whether
	 * the HPTE exists (or at least whether one did once).  -- paulus
	 */
	mfmsr	r10
	SYNC
	rlwinm	r0,r10,0,17,15		/* clear bit 16 (MSR_EE) */
	mtmsr	r0
	SYNC

#ifdef CONFIG_SMP
	lis	r9,hash_table_lock@h
	ori	r9,r9,hash_table_lock@l
	lwz	r8,PROCESSOR(r2)
	oris	r8,r8,10
10:	lwarx	r7,0,r9
	cmpi	0,r7,0
	bne-	11f
	stwcx.	r8,0,r9
	beq+	12f
11:	lwz	r7,0(r9)
	cmpi	0,r7,0
	beq	10b
	b	11b
12:	isync
#endif

	/*
	 * Fetch the linux pte and test and set _PAGE_HASHPTE atomically.
	 * If _PAGE_HASHPTE was already set, we don't replace the existing
	 * HPTE, so we just unlock and return.
	 */
	mr	r7,r5
1:	lwarx	r6,0,r7
	andi.	r0,r6,_PAGE_HASHPTE
	bne	9f			/* if HASHPTE already set, done */
	ori	r5,r6,_PAGE_ACCESSED|_PAGE_HASHPTE
	stwcx.	r5,0,r7
	bne-	1b

	li	r7,0			/* no address offset needed */
	bl	create_hpte

	lis	r8,htab_preloads@ha
	lwz	r3,htab_preloads@l(r8)
	addi	r3,r3,1
	stw	r3,htab_preloads@l(r8)

9:
#ifdef CONFIG_SMP
	eieio
	li	r0,0
	stw	r0,0(r9)		/* clear hash_table_lock */
#endif

	lwz	r0,4(r1)
	mtlr	r0

	/* reenable interrupts */
	mtmsr	r10
	SYNC
	blr

/*
 * This routine adds a hardware PTE to the hash table.
 * It is designed to be called with the MMU either on or off.
 * r3 contains the VSID, r4 contains the virtual address,
 * r5 contains the linux PTE, r6 contains the old value of the
 * linux PTE (before setting _PAGE_HASHPTE) and r7 contains the
 * offset to be added to addresses (0 if the MMU is on,
 * -KERNELBASE if it is off).
 * On SMP, the caller should have the hash_table_lock held.
 * We assume that the caller has (or will) set the _PAGE_HASHPTE
 * bit in the linux PTE in memory.  The value passed in r6 should
 * be the old linux PTE value; if it doesn't have _PAGE_HASHPTE set
 * this routine will skip the search for an existing HPTE.
 * This procedure modifies r0, r3 - r6, r8, cr0.
 *  -- paulus.
 *
 * For speed, 4 of the instructions get patched once the size and
 * physical address of the hash table are known.  These definitions
 * of Hash_base and Hash_bits below are just an example.
 */
Hash_base = 0xc0180000
Hash_bits = 12				/* e.g. 256kB hash table */
Hash_msk = (((1 << Hash_bits) - 1) * 64)

#ifndef CONFIG_PPC64BRIDGE
/* defines for the PTE format for 32-bit PPCs */
#define PTE_SIZE	8
#define PTEG_SIZE	64
#define LG_PTEG_SIZE	6
#define LDPTEu		lwzu
#define STPTE		stw
#define CMPPTE		cmpw
#define PTE_H		0x40
#define PTE_V		0x80000000
#define TST_V(r)	rlwinm. r,r,0,0,0
#define SET_V(r)	oris r,r,PTE_V@h
#define CLR_V(r,t)	rlwinm r,r,0,1,31

#else
/* defines for the PTE format for 64-bit PPCs */
#define PTE_SIZE	16
#define PTEG_SIZE	128
#define LG_PTEG_SIZE	7
#define LDPTEu		ldu
#define STPTE		std
#define CMPPTE		cmpd
#define PTE_H		2
#define PTE_V		1
#define TST_V(r)	andi. r,r,PTE_V
#define SET_V(r)	ori r,r,PTE_V
#define CLR_V(r,t)	li t,PTE_V; andc r,r,t
#endif /* CONFIG_PPC64BRIDGE */

#define HASH_LEFT	31-(LG_PTEG_SIZE+Hash_bits-1)
#define HASH_RIGHT	31-LG_PTEG_SIZE

_GLOBAL(create_hpte)
	/* Convert linux-style PTE (r5) to low word of PPC-style PTE (r8) */
	rlwinm	r8,r5,32-10,31,31	/* _PAGE_RW -> PP lsb */
	rlwinm	r0,r5,32-7,31,31	/* _PAGE_DIRTY -> PP lsb */
	and	r8,r8,r0		/* writable if _RW & _DIRTY */
	rlwimi	r5,r5,32-1,30,30	/* _PAGE_USER -> PP msb */
	rlwimi	r5,r5,32-2,31,31	/* _PAGE_USER -> PP lsb */
	ori	r8,r8,0xe14		/* clear out reserved bits and M */
	andc	r8,r5,r8		/* PP = user? (rw&dirty? 2: 3): 0 */
#ifdef CONFIG_SMP
	ori	r8,r8,_PAGE_COHERENT	/* set M (coherence required) */
#endif

#ifdef CONFIG_POWER4
	/*
	 * XXX hack hack hack - translate 32-bit "physical" addresses
	 * in the linux page tables to 42-bit real addresses in such
	 * a fashion that we can get at the I/O we need to access.
	 *	-- paulus
	 */
	cmpwi	r8,0
	rlwinm	r0,r8,16,16,30
	bge	57f
	cmplwi	r0,0xfe00
	li	r0,0x3fd
	bne	56f
	li	r0,0x3ff
56:	sldi	r0,r0,32
	or	r8,r8,r0
57:
#endif

	/* Construct the high word of the PPC-style PTE (r5) */
#ifndef CONFIG_PPC64BRIDGE
	rlwinm	r5,r3,7,1,24		/* put VSID in 0x7fffff80 bits */
	rlwimi	r5,r4,10,26,31		/* put in API (abbrev page index) */
#else /* CONFIG_PPC64BRIDGE */
	clrlwi	r3,r3,8			/* reduce vsid to 24 bits */
	sldi	r5,r3,12		/* shift vsid into position */
	rlwimi	r5,r4,16,20,24		/* put in API (abbrev page index) */
#endif /* CONFIG_PPC64BRIDGE */
	SET_V(r5)			/* set V (valid) bit */

	/* Get the address of the primary PTE group in the hash table (r3) */
	.globl	hash_page_patch_A
hash_page_patch_A:
	addis	r0,r7,Hash_base@h	/* base address of hash table */
	rlwimi	r0,r3,LG_PTEG_SIZE,HASH_LEFT,HASH_RIGHT    /* VSID -> hash */
	rlwinm	r3,r4,20+LG_PTEG_SIZE,HASH_LEFT,HASH_RIGHT /* PI -> hash */
	xor	r3,r3,r0		/* make primary hash */
	li	r0,8			/* PTEs/group */

	/*
	 * Test the _PAGE_HASHPTE bit in the old linux PTE, and skip the search
	 * if it is clear, meaning that the HPTE isn't there already...
	 */
	andi.	r6,r6,_PAGE_HASHPTE
	beq+	10f			/* no PTE: go look for an empty slot */
	tlbie	r4

	addis	r4,r7,htab_hash_searches@ha
	lwz	r6,htab_hash_searches@l(r4)
	addi	r6,r6,1			/* count how many searches we do */
	stw	r6,htab_hash_searches@l(r4)

	/* Search the primary PTEG for a PTE whose 1st (d)word matches r5 */
	mtctr	r0
	addi	r4,r3,-PTE_SIZE
1:	LDPTEu	r6,PTE_SIZE(r4)		/* get next PTE */
	CMPPTE	0,r6,r5
	bdnzf	2,1b			/* loop while ctr != 0 && !cr0.eq */
	beq+	found_slot

	/* Search the secondary PTEG for a matching PTE */
	ori	r5,r5,PTE_H		/* set H (secondary hash) bit */
	.globl	hash_page_patch_B
hash_page_patch_B:
	xoris	r4,r3,Hash_msk>>16	/* compute secondary hash */
	xori	r4,r4,(-PTEG_SIZE & 0xffff)
	addi	r4,r4,-PTE_SIZE
	mtctr	r0
2:	LDPTEu	r6,PTE_SIZE(r4)
	CMPPTE	0,r6,r5
	bdnzf	2,2b
	beq+	found_slot
	xori	r5,r5,PTE_H		/* clear H bit again */

	/* Search the primary PTEG for an empty slot */
10:	mtctr	r0
	addi	r4,r3,-PTE_SIZE		/* search primary PTEG */
1:	LDPTEu	r6,PTE_SIZE(r4)		/* get next PTE */
	TST_V(r6)			/* test valid bit */
	bdnzf	2,1b			/* loop while ctr != 0 && !cr0.eq */
	beq+	found_empty

	/* update counter of times that the primary PTEG is full */
	addis	r4,r7,primary_pteg_full@ha
	lwz	r6,primary_pteg_full@l(r4)
	addi	r6,r6,1
	stw	r6,primary_pteg_full@l(r4)

	/* Search the secondary PTEG for an empty slot */
	ori	r5,r5,PTE_H		/* set H (secondary hash) bit */
	.globl	hash_page_patch_C
hash_page_patch_C:
	xoris	r4,r3,Hash_msk>>16	/* compute secondary hash */
	xori	r4,r4,(-PTEG_SIZE & 0xffff)
	addi	r4,r4,-PTE_SIZE
	mtctr	r0
2:	LDPTEu	r6,PTE_SIZE(r4)
	TST_V(r6)
	bdnzf	2,2b
	beq+	found_empty
	xori	r5,r5,PTE_H		/* clear H bit again */

	/*
	 * Choose an arbitrary slot in the primary PTEG to overwrite.
	 * Since both the primary and secondary PTEGs are full, and we
	 * have no information that the PTEs in the primary PTEG are
	 * more important or useful than those in the secondary PTEG,
	 * and we know there is a definite (although small) speed
	 * advantage to putting the PTE in the primary PTEG, we always
	 * put the PTE in the primary PTEG.
	 */
	addis	r4,r7,next_slot@ha
	lwz	r6,next_slot@l(r4)
	addi	r6,r6,PTE_SIZE
	andi.	r6,r6,7*PTE_SIZE
#ifdef CONFIG_POWER4
	/*
	 * Since we don't have BATs on POWER4, we rely on always having
	 * PTEs in the hash table to map the hash table and the code
	 * that manipulates it in virtual mode, namely flush_hash_page and
	 * flush_hash_segments.  Otherwise we can get a DSI inside those
	 * routines which leads to a deadlock on the hash_table_lock on
	 * SMP machines.  We avoid this by never overwriting the first
	 * PTE of each PTEG if it is already valid.
	 *	-- paulus.
	 */
	bne	102f
	li	r6,PTE_SIZE
102:
#endif /* CONFIG_POWER4 */
	stw	r6,next_slot@l(r4)
	add	r4,r3,r6

	/* update counter of evicted pages */
	addis	r6,r7,htab_evicts@ha
	lwz	r3,htab_evicts@l(r6)
	addi	r3,r3,1
	stw	r3,htab_evicts@l(r6)

#ifndef CONFIG_SMP
	/* Store PTE in PTEG */
found_empty:
	STPTE	r5,0(r4)
found_slot:
	STPTE	r8,PTE_SIZE/2(r4)

#else /* CONFIG_SMP */
/*
 * Between the tlbie above and updating the hash table entry below,
 * another CPU could read the hash table entry and put it in its TLB.
 * There are 3 cases:
 * 1. using an empty slot
 * 2. updating an earlier entry to change permissions (i.e. enable write)
 * 3. taking over the PTE for an unrelated address
 *
 * In each case it doesn't really matter if the other CPUs have the old
 * PTE in their TLB.  So we don't need to bother with another tlbie here,
 * which is convenient as we've overwritten the register that had the
 * address. :-)  The tlbie above is mainly to make sure that this CPU comes
 * and gets the new PTE from the hash table.
 *
 * We do however have to make sure that the PTE is never in an invalid
 * state with the V bit set.
 */
found_empty:
found_slot:
	CLR_V(r5,r0)		/* clear V (valid) bit in PTE */
	STPTE	r5,0(r4)
	sync
	TLBSYNC
	STPTE	r8,PTE_SIZE/2(r4) /* put in correct RPN, WIMG, PP bits */
	sync
	SET_V(r5)
	STPTE	r5,0(r4)	/* finally set V bit in PTE */
#endif /* CONFIG_SMP */

	sync		/* make sure pte updates get to memory */
	blr

	.comm	next_slot,4
	.comm	primary_pteg_full,4
	.comm	htab_hash_searches,4

/*
 * Flush the entry for a particular page from the hash table.
 *
 * flush_hash_page(unsigned context, unsigned long va, pte_t *ptep)
 *
 * We assume that there is a hash table in use (Hash != 0).
 */
_GLOBAL(flush_hash_page)
	/* Convert context and va to VSID */
	mulli	r3,r3,897*16		/* multiply context by context skew */
	rlwinm	r0,r4,4,28,31		/* get ESID (top 4 bits of va) */
	mulli	r0,r0,0x111		/* multiply by ESID skew */
	add	r3,r3,r0		/* note code below trims to 24 bits */

	/*
	 * We disable interrupts here, even on UP, because we want
	 * the _PAGE_HASHPTE bit to be a reliable indication of
	 * whether the HPTE exists.  -- paulus
	 */
	mfmsr	r10
	rlwinm	r0,r10,0,17,15		/* clear bit 16 (MSR_EE) */
	SYNC
	mtmsr	r0
	SYNC

#ifdef CONFIG_SMP
	lis	r9,hash_table_lock@h
	ori	r9,r9,hash_table_lock@l
	lwz	r8,PROCESSOR(r2)
	oris	r8,r8,9
10:	lwarx	r7,0,r9
	cmpi	0,r7,0
	bne-	11f
	stwcx.	r8,0,r9
	beq+	12f
11:	lwz	r7,0(r9)
	cmpi	0,r7,0
	beq	10b
	b	11b
12:	isync
#endif

	/*
	 * Check the _PAGE_HASHPTE bit in the linux PTE.  If it is
	 * already clear, we're done.  If not, clear it (atomically)
	 * and proceed.  -- paulus.
	 */
1:	lwarx	r6,0,r5			/* fetch the pte */
	andi.	r0,r6,_PAGE_HASHPTE
	beq	9f			/* done if HASHPTE is already clear */
	rlwinm	r6,r6,0,31,29		/* clear HASHPTE bit */
	stwcx.	r6,0,r5			/* update the pte */
	bne-	1b

	/* Construct the high word of the PPC-style PTE (r5) */
#ifndef CONFIG_PPC64BRIDGE
	rlwinm	r5,r3,7,1,24		/* put VSID in 0x7fffff80 bits */
	rlwimi	r5,r4,10,26,31		/* put in API (abbrev page index) */
#else /* CONFIG_PPC64BRIDGE */
	clrlwi	r3,r3,8			/* reduce vsid to 24 bits */
	sldi	r5,r3,12		/* shift vsid into position */
	rlwimi	r5,r4,16,20,24		/* put in API (abbrev page index) */
#endif /* CONFIG_PPC64BRIDGE */
	SET_V(r5)			/* set V (valid) bit */

	/* Get the address of the primary PTE group in the hash table (r3) */
	.globl	flush_hash_patch_A
flush_hash_patch_A:
	lis	r8,Hash_base@h		/* base address of hash table */
	rlwimi	r8,r3,LG_PTEG_SIZE,HASH_LEFT,HASH_RIGHT    /* VSID -> hash */
	rlwinm	r3,r4,20+LG_PTEG_SIZE,HASH_LEFT,HASH_RIGHT /* PI -> hash */
	xor	r3,r3,r8		/* make primary hash */
	li	r8,8			/* PTEs/group */

	/* Search the primary PTEG for a PTE whose 1st (d)word matches r5 */
	mtctr	r8
	addi	r7,r3,-PTE_SIZE
1:	LDPTEu	r0,PTE_SIZE(r7)		/* get next PTE */
	CMPPTE	0,r0,r5
	bdnzf	2,1b			/* loop while ctr != 0 && !cr0.eq */
	beq+	3f

	/* Search the secondary PTEG for a matching PTE */
	ori	r5,r5,PTE_H		/* set H (secondary hash) bit */
	.globl	flush_hash_patch_B
flush_hash_patch_B:
	xoris	r7,r3,Hash_msk>>16	/* compute secondary hash */
	xori	r7,r7,(-PTEG_SIZE & 0xffff)
	addi	r7,r7,-PTE_SIZE
	mtctr	r8
2:	LDPTEu	r0,PTE_SIZE(r7)
	CMPPTE	0,r0,r5
	bdnzf	2,2b
	bne-	4f			/* should never fail to find it */

3:	li	r0,0
	STPTE	r0,0(r7)		/* invalidate entry */
4:	sync
	tlbie	r4			/* in hw tlb too */
	sync

#ifdef CONFIG_SMP
	TLBSYNC
9:	li	r0,0
	stw	r0,0(r9)		/* clear hash_table_lock */
#endif

9:	mtmsr	r10
	SYNC
	blr
