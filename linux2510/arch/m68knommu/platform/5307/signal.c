/*
 *  linux/arch/m68k/kernel/signal.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

/*
 * Linux/m68k support by Hamish Macdonald
 *
 * 68060 fixes by Jesper Skov
 *
 * 1997-12-01  Modified for POSIX.1b signals by Andreas Schwab
 *
 * mathemu support by Roman Zippel
 *  (Note: fpstate in the signal context is completly ignored for the emulator
 *         and the internal floating point format is put on stack)
 */

/*
 * ++roman (07/09/96): implemented signal stacks (specially for tosemu on
 * Atari :-) Current limitation: Only one sigstack can be active at one time.
 * If a second signal with SA_ONSTACK set arrives while working on a sigstack,
 * SA_ONSTACK is ignored. This behaviour avoids lots of trouble with nested
 * signal handlers!
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/ptrace.h>
#include <linux/unistd.h>
#include <linux/stddef.h>
#include <linux/highuid.h>
#include <linux/personality.h>

#include <asm/setup.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/traps.h>
#include <asm/ucontext.h>

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

asmlinkage long sys_wait4(pid_t pid, unsigned int * stat_addr, int options,
			struct rusage * ru);

asmlinkage int do_signal(sigset_t *oldset, struct pt_regs *regs);

#ifndef CONFIG_COLDFIRE
const int frame_extra_sizes[16] = {
  0,
  -1, /* sizeof(((struct frame *)0)->un.fmt1), */
  sizeof(((struct frame *)0)->un.fmt2),
  sizeof(((struct frame *)0)->un.fmt3),
  sizeof(((struct frame *)0)->un.fmt4),
  -1, /* sizeof(((struct frame *)0)->un.fmt5), */
  -1, /* sizeof(((struct frame *)0)->un.fmt6), */
  sizeof(((struct frame *)0)->un.fmt7),
  -1, /* sizeof(((struct frame *)0)->un.fmt8), */
  sizeof(((struct frame *)0)->un.fmt9),
  sizeof(((struct frame *)0)->un.fmta),
  sizeof(((struct frame *)0)->un.fmtb),
  -1, /* sizeof(((struct frame *)0)->un.fmtc), */
  -1, /* sizeof(((struct frame *)0)->un.fmtd), */
  -1, /* sizeof(((struct frame *)0)->un.fmte), */
  -1, /* sizeof(((struct frame *)0)->un.fmtf), */
};
#endif

/*
 * Atomically swap in the new signal mask, and wait for a signal.
 */
asmlinkage int do_sigsuspend(struct pt_regs *regs)
{
	old_sigset_t mask = regs->d3;
	sigset_t saveset;

	mask &= _BLOCKABLE;
	spin_lock_irq(&current->sigmask_lock);
	saveset = current->blocked;
	siginitset(&current->blocked, mask);
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	regs->d0 = -EINTR;
	while (1) {
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		if (do_signal(&saveset, regs))
			return -EINTR;
	}
}

asmlinkage int
do_rt_sigsuspend(struct pt_regs *regs)
{
	sigset_t *unewset = (sigset_t *)regs->d1;
	size_t sigsetsize = (size_t)regs->d2;
	sigset_t saveset, newset;

	/* XXX: Don't preclude handling different sized sigset_t's.  */
	if (sigsetsize != sizeof(sigset_t))
		return -EINVAL;

	if (copy_from_user(&newset, unewset, sizeof(newset)))
		return -EFAULT;
	sigdelsetmask(&newset, ~_BLOCKABLE);

	spin_lock_irq(&current->sigmask_lock);
	saveset = current->blocked;
	current->blocked = newset;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	regs->d0 = -EINTR;
	while (1) {
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		if (do_signal(&saveset, regs))
			return -EINTR;
	}
}

asmlinkage int 
sys_sigaction(int sig, const struct old_sigaction *act,
	      struct old_sigaction *oact)
{
	struct k_sigaction new_ka, old_ka;
	int ret;

	if (act) {
		old_sigset_t mask;
		if (verify_area(VERIFY_READ, act, sizeof(*act)) ||
		    __get_user(new_ka.sa.sa_handler, &act->sa_handler) ||
		    __get_user(new_ka.sa.sa_restorer, &act->sa_restorer))
			return -EFAULT;
		__get_user(new_ka.sa.sa_flags, &act->sa_flags);
		__get_user(mask, &act->sa_mask);
		siginitset(&new_ka.sa.sa_mask, mask);
	}

	ret = do_sigaction(sig, act ? &new_ka : NULL, oact ? &old_ka : NULL);

	if (!ret && oact) {
		if (verify_area(VERIFY_WRITE, oact, sizeof(*oact)) ||
		    __put_user(old_ka.sa.sa_handler, &oact->sa_handler) ||
		    __put_user(old_ka.sa.sa_restorer, &oact->sa_restorer))
			return -EFAULT;
		__put_user(old_ka.sa.sa_flags, &oact->sa_flags);
		__put_user(old_ka.sa.sa_mask.sig[0], &oact->sa_mask);
	}

	return ret;
}

asmlinkage int
sys_sigaltstack(const stack_t *uss, stack_t *uoss)
{
	return do_sigaltstack(uss, uoss, rdusp());
}


/*
 * Do a signal return; undo the signal stack.
 *
 * Keep the return code on the stack quadword aligned!
 * That makes the cache flush below easier.
 */

struct sigframe
{
	char *pretcode;
	int sig;
	int code;
	struct sigcontext *psc;
	char retcode[12];
	unsigned long extramask[_NSIG_WORDS-1];
	struct sigcontext sc;
};

struct rt_sigframe
{
	char *pretcode;
	int sig;
	struct siginfo *pinfo;
	void *puc;
#ifndef CONFIG_COLDFIRE
	char retcode[12];
#else
	char retcode[16];		/* extended since notb doesn't exist on ColdFire */
#endif
	struct siginfo info;
	struct ucontext uc;
};


int copy_siginfo_to_user(siginfo_t *to, siginfo_t *from)
{
	if (!access_ok (VERIFY_WRITE, to, sizeof(siginfo_t)))
		return -EFAULT;
	if (from->si_code < 0)
		return __copy_to_user(to, from, sizeof(siginfo_t));
	else {
		int err;

		/* If you change siginfo_t structure, please be sure
		   this code is fixed accordingly.
		   It should never copy any pad contained in the structure
		   to avoid security leaks, but must copy the generic
		   3 ints plus the relevant union member.  */
		err = __put_user(from->si_signo, &to->si_signo);
		err |= __put_user(from->si_errno, &to->si_errno);
		err |= __put_user((short)from->si_code, &to->si_code);
		/* First 32bits of unions are always present.  */
		err |= __put_user(from->si_pid, &to->si_pid);
		switch (from->si_code >> 16) {
		case __SI_FAULT >> 16:
			break;
		case __SI_CHLD >> 16:
			err |= __put_user(from->si_utime, &to->si_utime);
			err |= __put_user(from->si_stime, &to->si_stime);
			err |= __put_user(from->si_status, &to->si_status);
		default:
			err |= __put_user(from->si_uid, &to->si_uid);
			break;
		/* case __SI_RT: This is not generated by the kernel as of now.  */
		}
		return err;
	}
}

#ifndef NO_FPU

static unsigned char fpu_version = 0;	/* version number of fpu, set by setup_frame */

static inline int restore_fpu_state(struct sigcontext *sc)
{
	int err = 1;

	if (FPU_IS_EMU) {
	    /* restore registers */
	    memcpy(current->thread.fpcntl, sc->sc_fpcntl, 12);
	    memcpy(current->thread.fp, sc->sc_fpregs, 24);
	    return 0;
	}

	if (CPU_IS_060 ? sc->sc_fpstate[2] : sc->sc_fpstate[0]) {
	    /* Verify the frame format.  */
	    if (!CPU_IS_060 && (sc->sc_fpstate[0] != fpu_version))
		goto out;
	    if (CPU_IS_020_OR_030) {
		if (m68k_fputype & FPU_68881 &&
		    !(sc->sc_fpstate[1] == 0x18 || sc->sc_fpstate[1] == 0xb4))
		    goto out;
		if (m68k_fputype & FPU_68882 &&
		    !(sc->sc_fpstate[1] == 0x38 || sc->sc_fpstate[1] == 0xd4))
		    goto out;
	    } else if (CPU_IS_040) {
		if (!(sc->sc_fpstate[1] == 0x00 ||
                      sc->sc_fpstate[1] == 0x28 ||
                      sc->sc_fpstate[1] == 0x60))
		    goto out;
	    } else if (CPU_IS_060) {
		if (!(sc->sc_fpstate[3] == 0x00 ||
                      sc->sc_fpstate[3] == 0x60 ||
		      sc->sc_fpstate[3] == 0xe0))
		    goto out;
	    } else
		goto out;

	    __asm__ volatile (".chip 68k/68881\n\t"
			      "fmovemx %0,%/fp0-%/fp1\n\t"
			      "fmoveml %1,%/fpcr/%/fpsr/%/fpiar\n\t"
			      ".chip 68k"
			      : /* no outputs */
			      : "m" (*sc->sc_fpregs), "m" (*sc->sc_fpcntl));
	}
	__asm__ volatile (".chip 68k/68881\n\t"
			  "frestore %0\n\t"
			  ".chip 68k" : : "m" (*sc->sc_fpstate));
	err = 0;

out:
	return err;
}

#define FPCONTEXT_SIZE	216
#define uc_fpstate	uc_filler[0]
#define uc_formatvec	uc_filler[FPCONTEXT_SIZE/4]
#define uc_extra	uc_filler[FPCONTEXT_SIZE/4+1]

static inline int rt_restore_fpu_state(struct ucontext *uc)
{
	unsigned char fpstate[FPCONTEXT_SIZE];
	int context_size = CPU_IS_060 ? 8 : 0;
	fpregset_t fpregs;
	int err = 1;

	if (FPU_IS_EMU) {
		/* restore fpu control register */
		if (__copy_from_user(current->thread.fpcntl,
				&uc->uc_mcontext.fpregs.f_pcr, 12))
			goto out;
		/* restore all other fpu register */
		if (__copy_from_user(current->thread.fp,
				uc->uc_mcontext.fpregs.f_fpregs, 96))
			goto out;
		return 0;
	}

	if (__get_user(*(long *)fpstate, (long *)&uc->uc_fpstate))
		goto out;
	if (CPU_IS_060 ? fpstate[2] : fpstate[0]) {
		if (!CPU_IS_060)
			context_size = fpstate[1];
		/* Verify the frame format.  */
		if (!CPU_IS_060 && (fpstate[0] != fpu_version))
			goto out;
		if (CPU_IS_020_OR_030) {
			if (m68k_fputype & FPU_68881 &&
			    !(context_size == 0x18 || context_size == 0xb4))
				goto out;
			if (m68k_fputype & FPU_68882 &&
			    !(context_size == 0x38 || context_size == 0xd4))
				goto out;
		} else if (CPU_IS_040) {
			if (!(context_size == 0x00 ||
			      context_size == 0x28 ||
			      context_size == 0x60))
				goto out;
		} else if (CPU_IS_060) {
			if (!(fpstate[3] == 0x00 ||
			      fpstate[3] == 0x60 ||
			      fpstate[3] == 0xe0))
				goto out;
		} else
			goto out;
		if (__copy_from_user(&fpregs, &uc->uc_mcontext.fpregs,
				     sizeof(fpregs)))
			goto out;
		__asm__ volatile (".chip 68k/68881\n\t"
				  "fmovemx %0,%/fp0-%/fp7\n\t"
				  "fmoveml %1,%/fpcr/%/fpsr/%/fpiar\n\t"
				  ".chip 68k"
				  : /* no outputs */
				  : "m" (*fpregs.f_fpregs),
				    "m" (fpregs.f_pcr));
	}
	if (context_size &&
	    __copy_from_user(fpstate + 4, (long *)&uc->uc_fpstate + 1,
			     context_size))
		goto out;
	__asm__ volatile (".chip 68k/68881\n\t"
			  "frestore %0\n\t"
			  ".chip 68k" : : "m" (*fpstate));
	err = 0;

out:
	return err;
}

#endif

static inline int
restore_sigcontext(struct pt_regs *regs, struct sigcontext *usc, void *fp,
		   int *pd0)
{
#ifndef CONFIG_COLDFIRE
	int fsize;
#endif
	int formatvec;
	struct sigcontext context;
	int err = 0;

	/* get previous context */
	if (copy_from_user(&context, usc, sizeof(context)))
		goto badframe;
	
	/* restore passed registers */
	regs->d1 = context.sc_d1;
	regs->a0 = context.sc_a0;
	regs->a1 = context.sc_a1;
	regs->sr = (regs->sr & 0xff00) | (context.sc_sr & 0xff);
	regs->pc = context.sc_pc;
	regs->orig_d0 = -1;		/* disable syscall checks */
	wrusp(context.sc_usp);
	formatvec = context.sc_formatvec;
	regs->format = formatvec >> 12;
	regs->vector = formatvec & 0xfff;

#ifndef NO_FPU
	err = restore_fpu_state(&context);
#endif

#ifndef CONFIG_COLDFIRE /* has no extra crap */
	fsize = frame_extra_sizes[regs->format];
	if (fsize < 0) {
		/*
		 * user process trying to return with weird frame format
		 */
#if DEBUG
		printk("user process returning with weird frame format\n");
#endif
		goto badframe;
	}

	/* OK.	Make room on the supervisor stack for the extra junk,
	 * if necessary.
	 */

	if (fsize) {
		struct switch_stack *sw = (struct switch_stack *)regs - 1;
		regs->d0 = context.sc_d0;
#define frame_offset (sizeof(struct pt_regs)+sizeof(struct switch_stack))
		__asm__ __volatile__
			("   movel %0,%/a0\n\t"
			 "   subl %1,%/a0\n\t"     /* make room on stack */
			 "   movel %/a0,%/sp\n\t"  /* set stack pointer */
			 /* move switch_stack and pt_regs */
			 "1: movel %0@+,%/a0@+\n\t"
			 "   dbra %2,1b\n\t"
			 "   lea %/sp@(%c3),%/a0\n\t" /* add offset of fmt */
			 "   lsrl  #2,%1\n\t"
			 "   subql #1,%1\n\t"
			 "2: movesl %4@+,%2\n\t"
			 "3: movel %2,%/a0@+\n\t"
			 "   dbra %1,2b\n\t"
			 "   bral " SYMBOL_NAME_STR(ret_from_signal) "\n"
			 "4:\n"
			 ".section __ex_table,\"a\"\n"
			 "   .align 4\n"
			 "   .long 2b,4b\n"
			 "   .long 3b,4b\n"
			 ".previous"
			 : /* no outputs, it doesn't ever return */
			 : "a" (sw), "d" (fsize), "d" (frame_offset/4-1),
			   "n" (frame_offset), "a" (fp)
			 : "a0");
#undef frame_offset
		/*
		 * If we ever get here an exception occurred while
		 * building the above stack-frame.
		 */
		goto badframe;
	}
#endif /* CONFIG_COLDFIRE */

	*pd0 = context.sc_d0;
	return err;

badframe:
	return 1;
}

static inline int
rt_restore_ucontext(struct pt_regs *regs, struct switch_stack *sw,
		    struct ucontext *uc, int *pd0)
{
#ifndef CONFIG_COLDFIRE
	int fsize;
#endif
	int temp;
	greg_t *gregs = uc->uc_mcontext.gregs;
	unsigned long usp;
	int err;

	err = __get_user(temp, &uc->uc_mcontext.version);
	if (temp != MCONTEXT_VERSION)
		goto badframe;
	/* restore passed registers */
	err |= __get_user(regs->d0, &gregs[0]);
	err |= __get_user(regs->d1, &gregs[1]);
	err |= __get_user(regs->d2, &gregs[2]);
	err |= __get_user(regs->d3, &gregs[3]);
	err |= __get_user(regs->d4, &gregs[4]);
	err |= __get_user(regs->d5, &gregs[5]);
	err |= __get_user(sw->d6, &gregs[6]);
	err |= __get_user(sw->d7, &gregs[7]);
	err |= __get_user(regs->a0, &gregs[8]);
	err |= __get_user(regs->a1, &gregs[9]);
	err |= __get_user(regs->a2, &gregs[10]);
	err |= __get_user(sw->a3, &gregs[11]);
	err |= __get_user(sw->a4, &gregs[12]);
	err |= __get_user(sw->a5, &gregs[13]);
	err |= __get_user(sw->a6, &gregs[14]);
	err |= __get_user(usp, &gregs[15]);
	wrusp(usp);
	err |= __get_user(regs->pc, &gregs[16]);
	err |= __get_user(temp, &gregs[17]);
	regs->sr = (regs->sr & 0xff00) | (temp & 0xff);
	regs->orig_d0 = -1;		/* disable syscall checks */
#ifdef CONFIG_COLDFIRE
	err |= __get_user(temp, &uc->uc_formatvec);
#endif
	regs->format = temp >> 12;
	regs->vector = temp & 0xfff;

#if DAVIDM /* NO_FPU */
	err |= rt_restore_fpu_state(uc);
#endif

	if (do_sigaltstack(&uc->uc_stack, NULL, usp) == -EFAULT)
		goto badframe;

#ifndef CONFIG_COLDFIRE
	fsize = frame_extra_sizes[regs->format];
	if (fsize < 0) {
		/*
		 * user process trying to return with weird frame format
		 */
#if DEBUG
		printk("user process returning with weird frame format\n");
#endif
		goto badframe;
	}

	/* OK.	Make room on the supervisor stack for the extra junk,
	 * if necessary.
	 */

	if (fsize) {
#define frame_offset (sizeof(struct pt_regs)+sizeof(struct switch_stack))
		__asm__ __volatile__
			("   movel %0,%/a0\n\t"
			 "   subl %1,%/a0\n\t"     /* make room on stack */
			 "   movel %/a0,%/sp\n\t"  /* set stack pointer */
			 /* move switch_stack and pt_regs */
			 "1: movel %0@+,%/a0@+\n\t"
			 "   dbra %2,1b\n\t"
			 "   lea %/sp@(%c3),%/a0\n\t" /* add offset of fmt */
			 "   lsrl  #2,%1\n\t"
			 "   subql #1,%1\n\t"
			 "2: movesl %4@+,%2\n\t"
			 "3: movel %2,%/a0@+\n\t"
			 "   dbra %1,2b\n\t"
			 "   bral " SYMBOL_NAME_STR(ret_from_signal) "\n"
			 "4:\n"
			 ".section __ex_table,\"a\"\n"
			 "   .align 4\n"
			 "   .long 2b,4b\n"
			 "   .long 3b,4b\n"
			 ".previous"
			 : /* no outputs, it doesn't ever return */
			 : "a" (sw), "d" (fsize), "d" (frame_offset/4-1),
			   "n" (frame_offset), "a" (&uc->uc_extra)
			 : "a0");
#undef frame_offset
		/*
		 * If we ever get here an exception occurred while
		 * building the above stack-frame.
		 */
		goto badframe;
	}
#endif /* CONFIG_COLDFIRE */

	*pd0 = regs->d0;
	return err;

badframe:
	return 1;
}

asmlinkage int do_sigreturn(unsigned long __unused)
{
	struct switch_stack *sw = (struct switch_stack *) &__unused;
	struct pt_regs *regs = (struct pt_regs *) (sw + 1);
	unsigned long usp = rdusp();
	struct sigframe *frame = (struct sigframe *)(usp - 4);
	sigset_t set;
	int d0;

	if (verify_area(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__get_user(set.sig[0], &frame->sc.sc_mask) ||
	    (_NSIG_WORDS > 1 &&
	     __copy_from_user(&set.sig[1], &frame->extramask,
			      sizeof(frame->extramask))))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sigmask_lock);
	current->blocked = set;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);
	
	if (restore_sigcontext(regs, &frame->sc, frame + 1, &d0))
		goto badframe;
	return d0;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

asmlinkage int do_rt_sigreturn(unsigned long __unused)
{
	struct switch_stack *sw = (struct switch_stack *) &__unused;
	struct pt_regs *regs = (struct pt_regs *) (sw + 1);
	unsigned long usp = rdusp();
	struct rt_sigframe *frame = (struct rt_sigframe *)(usp - 4);
	sigset_t set;
	int d0;

	if (verify_area(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sigmask_lock);
	current->blocked = set;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);
	
	if (rt_restore_ucontext(regs, sw, &frame->uc, &d0))
		goto badframe;
	return d0;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

#ifndef NO_FPU
/*
 * Set up a signal frame.
 */

static inline void save_fpu_state(struct sigcontext *sc, struct pt_regs *regs)
{
	if (FPU_IS_EMU) {
		/* save registers */
		memcpy(sc->sc_fpcntl, current->thread.fpcntl, 12);
		memcpy(sc->sc_fpregs, current->thread.fp, 24);
		return;
	}

	__asm__ volatile (".chip 68k/68881\n\t"
			  "fsave %0\n\t"
			  ".chip 68k"
			  : : "m" (*sc->sc_fpstate) : "memory");

	if (CPU_IS_060 ? sc->sc_fpstate[2] : sc->sc_fpstate[0]) {
		fpu_version = sc->sc_fpstate[0];
		if (CPU_IS_020_OR_030 &&
		    regs->vector >= (VEC_FPBRUC * 4) &&
		    regs->vector <= (VEC_FPNAN * 4)) {
			/* Clear pending exception in 68882 idle frame */
			if (*(unsigned short *) sc->sc_fpstate == 0x1f38)
				sc->sc_fpstate[0x38] |= 1 << 3;
		}
		__asm__ volatile (".chip 68k/68881\n\t"
				  "fmovemx %/fp0-%/fp1,%0\n\t"
				  "fmoveml %/fpcr/%/fpsr/%/fpiar,%1\n\t"
				  ".chip 68k"
				  : /* no outputs */
				  : "m" (*sc->sc_fpregs),
				    "m" (*sc->sc_fpcntl)
				  : "memory");
	}
}

static inline int rt_save_fpu_state(struct ucontext *uc, struct pt_regs *regs)
{
	unsigned char fpstate[FPCONTEXT_SIZE];
	int context_size = CPU_IS_060 ? 8 : 0;
	int err = 0;

	if (FPU_IS_EMU) {
		/* save fpu control register */
		err |= copy_to_user(&uc->uc_mcontext.fpregs.f_pcr,
				current->thread.fpcntl, 12);
		/* save all other fpu register */
		err |= copy_to_user(uc->uc_mcontext.fpregs.f_fpregs,
				current->thread.fp, 96);
		return err;
	}

	__asm__ volatile (".chip 68k/68881\n\t"
			  "fsave %0\n\t"
			  ".chip 68k"
			  : : "m" (*fpstate) : "memory");

	err |= __put_user(*(long *)fpstate, (long *)&uc->uc_fpstate);
	if (CPU_IS_060 ? fpstate[2] : fpstate[0]) {
		fpregset_t fpregs;
		if (!CPU_IS_060)
			context_size = fpstate[1];
		fpu_version = fpstate[0];
		if (CPU_IS_020_OR_030 &&
		    regs->vector >= (VEC_FPBRUC * 4) &&
		    regs->vector <= (VEC_FPNAN * 4)) {
			/* Clear pending exception in 68882 idle frame */
			if (*(unsigned short *) fpstate == 0x1f38)
				fpstate[0x38] |= 1 << 3;
		}
		__asm__ volatile (".chip 68k/68881\n\t"
				  "fmovemx %/fp0-%/fp7,%0\n\t"
				  "fmoveml %/fpcr/%/fpsr/%/fpiar,%1\n\t"
				  ".chip 68k"
				  : /* no outputs */
				  : "m" (*fpregs.f_fpregs),
				    "m" (fpregs.f_pcr)
				  : "memory");
		err |= copy_to_user(&uc->uc_mcontext.fpregs, &fpregs,
				    sizeof(fpregs));
	}
	if (context_size)
		err |= copy_to_user((long *)&uc->uc_fpstate + 1, fpstate + 4,
				    context_size);
	return err;
}

#endif

static void setup_sigcontext(struct sigcontext *sc, struct pt_regs *regs,
			     unsigned long mask)
{
	sc->sc_mask = mask;
	sc->sc_usp = rdusp();
	sc->sc_d0 = regs->d0;
	sc->sc_d1 = regs->d1;
	sc->sc_a0 = regs->a0;
	sc->sc_a1 = regs->a1;
	sc->sc_sr = regs->sr;
	sc->sc_pc = regs->pc;
	sc->sc_formatvec = regs->format << 12 | regs->vector;
#ifndef NO_FPU
	save_fpu_state(sc, regs);
#endif
}

static inline int rt_setup_ucontext(struct ucontext *uc, struct pt_regs *regs)
{
	struct switch_stack *sw = (struct switch_stack *)regs - 1;
	greg_t *gregs = uc->uc_mcontext.gregs;
	int err = 0;

	err |= __put_user(MCONTEXT_VERSION, &uc->uc_mcontext.version);
	err |= __put_user(regs->d0, &gregs[0]);
	err |= __put_user(regs->d1, &gregs[1]);
	err |= __put_user(regs->d2, &gregs[2]);
	err |= __put_user(regs->d3, &gregs[3]);
	err |= __put_user(regs->d4, &gregs[4]);
	err |= __put_user(regs->d5, &gregs[5]);
	err |= __put_user(sw->d6, &gregs[6]);
	err |= __put_user(sw->d7, &gregs[7]);
	err |= __put_user(regs->a0, &gregs[8]);
	err |= __put_user(regs->a1, &gregs[9]);
	err |= __put_user(regs->a2, &gregs[10]);
	err |= __put_user(sw->a3, &gregs[11]);
	err |= __put_user(sw->a4, &gregs[12]);
	err |= __put_user(sw->a5, &gregs[13]);
	err |= __put_user(sw->a6, &gregs[14]);
	err |= __put_user(rdusp(), &gregs[15]);
	err |= __put_user(regs->pc, &gregs[16]);
	err |= __put_user(regs->sr, &gregs[17]);
#ifdef CONFIG_COLDFIRE
	err |= __put_user((regs->format << 12) | regs->vector, &uc->uc_formatvec);
#endif
#ifndef NO_FPU
	err |= rt_save_fpu_state(uc, regs);
#endif
	return err;
}

static inline void push_cache (unsigned long vaddr)
{
#if 0 /* DAVIDM - implement this for 5407, maybe */
	/*
	 * Using the old cache_push_v() was really a big waste.
	 *
	 * What we are trying to do is to flush 8 bytes to ram.
	 * Flushing 2 cache lines of 16 bytes is much cheaper than
	 * flushing 1 or 2 pages, as previously done in
	 * cache_push_v().
	 *                                                     Jes
	 */
	if (CPU_IS_040) {
		unsigned long temp;

		__asm__ __volatile__ (".chip 68040\n\t"
				      "nop\n\t"
				      "ptestr (%1)\n\t"
				      "movec %%mmusr,%0\n\t"
				      ".chip 68k"
				      : "=r" (temp)
				      : "a" (vaddr));

		temp &= PAGE_MASK;
		temp |= vaddr & ~PAGE_MASK;

		__asm__ __volatile__ (".chip 68040\n\t"
				      "nop\n\t"
				      "cpushl %%bc,(%0)\n\t"
				      ".chip 68k"
				      : : "a" (temp));
	}
	else if (CPU_IS_060) {
		unsigned long temp;
		__asm__ __volatile__ (".chip 68060\n\t"
				      "plpar (%0)\n\t"
				      ".chip 68k"
				      : "=a" (temp)
				      : "0" (vaddr));
		__asm__ __volatile__ (".chip 68060\n\t"
				      "cpushl %%bc,(%0)\n\t"
				      ".chip 68k"
				      : : "a" (temp));
	}
	else {
		/*
		 * 68030/68020 have no writeback cache;
		 * still need to clear icache.
		 * Note that vaddr is guaranteed to be long word aligned.
		 */
		unsigned long temp;
		asm volatile ("movec %%cacr,%0" : "=r" (temp));
		temp += 4;
		asm volatile ("movec %0,%%caar\n\t"
			      "movec %1,%%cacr"
			      : : "r" (vaddr), "r" (temp));
		asm volatile ("movec %0,%%caar\n\t"
			      "movec %1,%%cacr"
			      : : "r" (vaddr + 4), "r" (temp));
	}
#endif
}

static inline void *
get_sigframe(struct k_sigaction *ka, struct pt_regs *regs, size_t frame_size)
{
	unsigned long usp;

	/* Default to using normal stack.  */
	usp = rdusp();

	/* This is the X/Open sanctioned signal stack switching.  */
	if (ka->sa.sa_flags & SA_ONSTACK) {
		if (!on_sig_stack(usp))
			usp = current->sas_ss_sp + current->sas_ss_size;
	}
	return (void *)((usp - frame_size) & -8UL);
}

static void setup_frame (int sig, struct k_sigaction *ka,
			 sigset_t *set, struct pt_regs *regs)
{
	struct sigframe *frame;
#ifndef CONFIG_COLDFIRE
	int fsize = frame_extra_sizes[regs->format];
#endif
	struct sigcontext context;
	unsigned long a5;
	int err = 0;

#ifndef CONFIG_COLDFIRE
	if (fsize < 0) {
#ifdef DEBUG
		printk ("setup_frame: Unknown frame format %#x\n",
			regs->format);
#endif
		goto give_sigsegv;
	}

	frame = get_sigframe(ka, regs, sizeof(*frame) + fsize);

	if (fsize) {
		err |= copy_to_user (frame + 1, regs + 1, fsize);
		regs->stkadj = fsize;
	}
#else /* ! CONFIG_COLDFIRE */
	frame = get_sigframe(ka, regs, sizeof(*frame));
#endif /* ! CONFIG_COLDFIRE */

	err |= __put_user((current->exec_domain
			   && current->exec_domain->signal_invmap
			   && sig < 32
			   ? current->exec_domain->signal_invmap[sig]
			   : sig),
			  &frame->sig);

	err |= __put_user(regs->vector, &frame->code);
	err |= __put_user(&frame->sc, &frame->psc);

	if (_NSIG_WORDS > 1)
		err |= copy_to_user(frame->extramask, &set->sig[1],
				    sizeof(frame->extramask));

	setup_sigcontext(&context, regs, set->sig[0]);
	err |= copy_to_user (&frame->sc, &context, sizeof(context));

	/* Set up to return from userspace.  */
	err |= __put_user(frame->retcode, &frame->pretcode);

	/* movea.l #,a5; moveq #,d0; trap #0 */
	a5 = ((struct switch_stack *)regs - 1)->a5;
	err |= __put_user(0x2a7c0000 + ((a5 >> 16) & 0xffff),
			(long *)(frame->retcode + 0));
	err |= __put_user(0x00007000 + ((a5 & 0xffff) << 16) + __NR_sigreturn,
			(long *)(frame->retcode + 4));
	err |= __put_user(0x4e40, (short *)(frame->retcode + 8));

	if (err)
		goto give_sigsegv;

	push_cache ((unsigned long) &frame->retcode);

	/* Set up registers for signal handler */
	wrusp ((unsigned long) frame);
	regs->pc = (unsigned long) ka->sa.sa_handler;

adjust_stack:
	/* Prepare to skip over the extra stuff in the exception frame.  */
	if (regs->stkadj) {
		struct pt_regs *tregs =
			(struct pt_regs *)((ulong)regs + regs->stkadj);
#ifdef DEBUG
		printk("Performing stackadjust=%04x\n", regs->stkadj);
#endif
		/* This must be copied with decreasing addresses to
                   handle overlaps.  */
		tregs->vector = 0;
		tregs->format = 0;
		tregs->pc = regs->pc;
		tregs->sr = regs->sr;
	}
	return;

give_sigsegv:
	if (sig == SIGSEGV)
		ka->sa.sa_handler = SIG_DFL;
	force_sig(SIGSEGV, current);
	goto adjust_stack;
}

static void setup_rt_frame (int sig, struct k_sigaction *ka, siginfo_t *info,
			    sigset_t *set, struct pt_regs *regs)
{
	struct rt_sigframe *frame;
#ifndef CONFIG_COLDFIRE
	int fsize = frame_extra_sizes[regs->format];
#endif
	unsigned long a5;
	int err = 0;

#ifndef CONFIG_COLDFIRE
	if (fsize < 0) {
#ifdef DEBUG
		printk ("setup_rt_frame: Unknown frame format %#x\n",
			regs->format);
#endif
		goto give_sigsegv;
	}
#endif

	frame = get_sigframe(ka, regs, sizeof(*frame));

#ifndef CONFIG_COLDFIRE
	if (fsize) {
		err |= copy_to_user (&frame->uc.uc_extra, regs + 1, fsize);
		regs->stkadj = fsize;
	}
#endif

	err |= __put_user((current->exec_domain
			   && current->exec_domain->signal_invmap
			   && sig < 32
			   ? current->exec_domain->signal_invmap[sig]
			   : sig),
			  &frame->sig);
	err |= __put_user(&frame->info, &frame->pinfo);
	err |= __put_user(&frame->uc, &frame->puc);
	err |= copy_siginfo_to_user(&frame->info, info);

	/* Create the ucontext.  */
	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(0, &frame->uc.uc_link);
	err |= __put_user((void *)current->sas_ss_sp,
			  &frame->uc.uc_stack.ss_sp);
	err |= __put_user(sas_ss_flags(rdusp()),
			  &frame->uc.uc_stack.ss_flags);
	err |= __put_user(current->sas_ss_size, &frame->uc.uc_stack.ss_size);
	err |= rt_setup_ucontext(&frame->uc, regs);
	err |= copy_to_user (&frame->uc.uc_sigmask, set, sizeof(*set));

	/* Set up to return from userspace.  */
	err |= __put_user(frame->retcode, &frame->pretcode);

#ifndef CONFIG_COLDFIRE /* notb is no Coldfire opcode */

	/* moveq #,d0; notb d0; movea.l #,a5; trap #0 */
	a5 = ((struct switch_stack *)regs - 1)->a5;
	err |= __put_user(0x70004600 + ((__NR_rt_sigreturn ^ 0xff) << 16),
			  (long *)(frame->retcode + 0));
	err != __put_user(0x2a7c0000 + ((a5 >> 16) & 0xffff),
			(long *)(frame->retcode + 4));
	err != __put_user(0x00004e40 + ((a5 & 0xffff) << 16),
			(long *)(frame->retcode + 8));

#else
	/* moveq #,d0; andi.l #,D0 ;movea.l #,a5; trap #0 */
	a5 = ((struct switch_stack *)regs - 1)->a5;
	err |= __put_user(0x70AD0280,
			  (long *)(frame->retcode + 0));
	err |= __put_user(0x000000ff,(long *)(frame->retcode + 4));
	err |= __put_user(0x2a7c0000 + ((a5 >> 16) & 0xffff),
			(long *)(frame->retcode + 8));
	err |= __put_user(0x00004e40 + ((a5 & 0xffff) << 16),
			(long *)(frame->retcode + 12));
#endif

	if (err)
		goto give_sigsegv;

	push_cache ((unsigned long) &frame->retcode);

	/* Set up registers for signal handler */
	wrusp ((unsigned long) frame);
	regs->pc = (unsigned long) ka->sa.sa_handler;

adjust_stack:
	/* Prepare to skip over the extra stuff in the exception frame.  */
	if (regs->stkadj) {
		struct pt_regs *tregs =
			(struct pt_regs *)((ulong)regs + regs->stkadj);
#ifdef DEBUG
		printk("Performing stackadjust=%04x\n", regs->stkadj);
#endif
		/* This must be copied with decreasing addresses to
                   handle overlaps.  */
		tregs->vector = 0;
		tregs->format = 0;
		tregs->pc = regs->pc;
		tregs->sr = regs->sr;
	}
	return;

give_sigsegv:
	if (sig == SIGSEGV)
		ka->sa.sa_handler = SIG_DFL;
	force_sig(SIGSEGV, current);
	goto adjust_stack;
}

static inline void
handle_restart(struct pt_regs *regs, struct k_sigaction *ka, int has_handler)
{
	switch (regs->d0) {
	case -ERESTARTNOHAND:
		if (!has_handler)
			goto do_restart;
		regs->d0 = -EINTR;
		break;

	case -ERESTARTSYS:
		if (has_handler && !(ka->sa.sa_flags & SA_RESTART)) {
			regs->d0 = -EINTR;
			break;
		}
	/* fallthrough */
	case -ERESTARTNOINTR:
	do_restart:
		regs->d0 = regs->orig_d0;
		regs->pc -= 2;
		break;
	}
}

/*
 * OK, we're invoking a handler
 */
static void
handle_signal(int sig, struct k_sigaction *ka, siginfo_t *info,
	      sigset_t *oldset, struct pt_regs *regs)
{
	/* are we from a system call? */
	if (regs->orig_d0 >= 0)
		/* If so, check system call restarting.. */
		handle_restart(regs, ka, 1);

	/* set up the stack frame */
	if (ka->sa.sa_flags & SA_SIGINFO)
		setup_rt_frame(sig, ka, info, oldset, regs);
	else
		setup_frame(sig, ka, oldset, regs);

	if (ka->sa.sa_flags & SA_ONESHOT)
		ka->sa.sa_handler = SIG_DFL;

	if (!(ka->sa.sa_flags & SA_NODEFER)) {
		spin_lock_irq(&current->sigmask_lock);
		sigorsets(&current->blocked,&current->blocked,&ka->sa.sa_mask);
		sigaddset(&current->blocked,sig);
		recalc_sigpending(current);
		spin_lock_irq(&current->sigmask_lock);
	}
}

/*
 * Note that 'init' is a special process: it doesn't get signals it doesn't
 * want to handle. Thus you cannot kill init even with a SIGKILL even by
 * mistake.
 *
 * Note that we go through the signals twice: once to check the signals
 * that the kernel can handle, and then we build all the user-level signal
 * handling stack-frames in one go after that.
 */
asmlinkage int do_signal(sigset_t *oldset, struct pt_regs *regs)
{
	siginfo_t info;
	struct k_sigaction *ka;

	current->thread.esp0 = (unsigned long) regs;

	if (!oldset)
		oldset = &current->blocked;

	for (;;) {
		int signr;

		spin_lock_irq(&current->sigmask_lock);
		signr = dequeue_signal(&current->blocked, &info);
		spin_unlock_irq(&current->sigmask_lock);

		if (!signr)
			break;

		if ((current->ptrace & PT_PTRACED) && signr != SIGKILL) {
			current->exit_code = signr;
			current->state = TASK_STOPPED;
			regs->sr &= ~PS_T;

			/* Did we come from a system call? */
			if (regs->orig_d0 >= 0) {
				/* Restart the system call the same way as
				   if the process were not traced.  */
				struct k_sigaction *ka =
					&current->sig->action[signr-1];
				int has_handler =
					(ka->sa.sa_handler != SIG_IGN &&
					 ka->sa.sa_handler != SIG_DFL);
				handle_restart(regs, ka, has_handler);
			}
			notify_parent(current, SIGCHLD);
			schedule();

			/* We're back.  Did the debugger cancel the sig?  */
			if (!(signr = current->exit_code)) {
			discard_frame:
#ifndef CONFIG_COLDFIRE
			    /* Make sure that a faulted bus cycle isn't
			       restarted (only needed on the 680[23]0).  */
			    if (regs->format == 10 || regs->format == 11)
				regs->stkadj = frame_extra_sizes[regs->format];
#endif
			    continue;
			}
			current->exit_code = 0;

			/* The debugger continued.  Ignore SIGSTOP.  */
			if (signr == SIGSTOP)
				goto discard_frame;

			/* Update the siginfo structure.  Is this good?  */
			if (signr != info.si_signo) {
				info.si_signo = signr;
				info.si_errno = 0;
				info.si_code = SI_USER;
				info.si_pid = current->p_pptr->pid;
				info.si_uid = current->p_pptr->uid;
			}

			/* If the (new) signal is now blocked, requeue it.  */
			if (sigismember(&current->blocked, signr)) {
				send_sig_info(signr, &info, current);
				continue;
			}
		}

		ka = &current->sig->action[signr-1];
		if (ka->sa.sa_handler == SIG_IGN) {
			if (signr != SIGCHLD)
				continue;
			/* Check for SIGCHLD: it's special.  */
			while (sys_wait4(-1, NULL, WNOHANG, NULL) > 0)
				/* nothing */;
			continue;
		}

		if (ka->sa.sa_handler == SIG_DFL) {
			int exit_code = signr;

			if (current->pid == 1)
				continue;

			switch (signr) {
			case SIGCONT: case SIGCHLD:
			case SIGWINCH: case SIGURG:
				continue;

			case SIGTSTP: case SIGTTIN: case SIGTTOU:
				if (is_orphaned_pgrp(current->pgrp))
					continue;
				/* FALLTHRU */

			case SIGSTOP: {
				struct signal_struct *sig;
				current->state = TASK_STOPPED;
				current->exit_code = signr;
                                sig = current->p_pptr->sig;
                                if (sig && !(sig->action[SIGCHLD-1].sa.sa_flags 
& SA_NOCLDSTOP))
                                        notify_parent(current, SIGCHLD);
				schedule();
				continue;
			}

			case SIGQUIT: case SIGILL: case SIGTRAP:
			case SIGIOT: case SIGFPE: case SIGSEGV:
			case SIGBUS: case SIGSYS: case SIGXCPU: case SIGXFSZ:
#if defined(CONFIG_NETtel) || defined(CONFIG_SNAPGEAR)
				printk("%s is core dumping at 0x%x\n", 
						current->comm, regs->pc);
				show_regs(regs);
#endif
				if (do_coredump(signr, regs))
					exit_code |= 0x80;
				/* FALLTHRU */

			default:
				sig_exit(signr, exit_code, &info);
				/* NOTREACHED */
			}
		}

		/* Whee!  Actually deliver the signal.  */
		handle_signal(signr, ka, &info, oldset, regs);
		return 1;
	}

	/* Did we come from a system call? */
	if (regs->orig_d0 >= 0)
		/* Restart the system call - no handlers present */
		handle_restart(regs, NULL, 0);

	/* If we are about to discard some frame stuff we must copy
	   over the remaining frame. */
	if (regs->stkadj) {
		struct pt_regs *tregs =
		  (struct pt_regs *) ((ulong) regs + regs->stkadj);

		/* This must be copied with decreasing addresses to
		   handle overlaps.  */
		tregs->vector = 0;
		tregs->format = 0;
		tregs->pc = regs->pc;
		tregs->sr = regs->sr;
	}
	return 0;
}


/*
 *	return the correct value for a5/start_data,  mainly for signal
 *	handlers to ensure they get the correct data segment.
 *
 * Special notes for the shared library case:
 *	The debug code here that checks the integrity of the pre-data segment
 * pointers to other data segments can falsely warn about broken a5 registers.
 * It is quite legitimate for user written assembly to use a5 for anything.
 * it is also quite legitimate for compiled programs to use a5 too.  E.g. a
 * function that doesn't need a5 for data references can use it as a temporary
 * register.  Plus, absolute code can also be run in a shared library
 * environment.  All these mean that the current a5 may not point to the start
 * of a data segment.
 *
 *	Secondly, we could return the beginning of any data segment however it
 * is simplist to return the data segment start since we avoid checking if what
 * a5 points to is really a data segment.  The signal handler will have to ensure
 * that a5 gets set correctly for it but the compiler takes care of that too.
 */

unsigned long
get_pic_a5(void)
{
#ifdef DEBUG
  	unsigned long a5;

	__asm__ __volatile__("movel %/a5,%0"
			     : "=a" (a5));

	if (current->mm->start_data != a5)
#ifdef CONFIG_BINFMT_SHARED_FLAT
	{	int i;

		/* It simply isn't sufficient to check for a5 having changed, we
		 * must verify that the GOT pointers are equal for both segments.
		 */
		for (i=0; i<MAX_SHARED_LIBS; i++) {
			if (*(unsigned long *)(current->mm->start_data - 4*i - 4) !=
					*(unsigned long *)(a5 - 4*i - 4)) {
				printk("%s: possible broken a5 (%d) 0x%x (0x%x)\n",
						current->comm,
						i, a5,
						current->mm->start_data);
				break;
			}
		}
	}
#else
		printk("broken a5 0x%x != 0x%x\n", current->mm->start_data, a5);
#endif
#endif
	return current->mm->start_data;
}

