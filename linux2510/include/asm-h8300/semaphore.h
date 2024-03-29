#ifndef _H8300_SEMAPHORE_H
#define _H8300_SEMAPHORE_H

#define RW_LOCK_BIAS		 0x01000000

#ifndef __ASSEMBLY__

#include <linux/linkage.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/rwsem.h>

#include <asm/system.h>
#include <asm/atomic.h>

/*
 * Interrupt-safe semaphores..
 *
 * (C) Copyright 1996 Linus Torvalds
 *
 * H8/300 version by Yoshinori Sato
 */


struct semaphore {
	atomic_t count;
	int sleepers;
	wait_queue_head_t wait;
#if WAITQUEUE_DEBUG
	long __magic;
#endif
};

#if WAITQUEUE_DEBUG
# define __SEM_DEBUG_INIT(name) \
		, (long)&(name).__magic
#else
# define __SEM_DEBUG_INIT(name)
#endif

#define __SEMAPHORE_INITIALIZER(name,count) \
{ ATOMIC_INIT(count), 0, __WAIT_QUEUE_HEAD_INITIALIZER((name).wait) \
	__SEM_DEBUG_INIT(name) }

#define __MUTEX_INITIALIZER(name) \
	__SEMAPHORE_INITIALIZER(name,1)

#define __DECLARE_SEMAPHORE_GENERIC(name,count) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name,count)

#define DECLARE_MUTEX(name) __DECLARE_SEMAPHORE_GENERIC(name,1)
#define DECLARE_MUTEX_LOCKED(name) __DECLARE_SEMAPHORE_GENERIC(name,0)

extern inline void sema_init (struct semaphore *sem, int val)
{
	*sem = (struct semaphore)__SEMAPHORE_INITIALIZER(*sem, val);
}

static inline void init_MUTEX (struct semaphore *sem)
{
	sema_init(sem, 1);
}

static inline void init_MUTEX_LOCKED (struct semaphore *sem)
{
	sema_init(sem, 0);
}

asmlinkage void __down_failed(void /* special register calling convention */);
asmlinkage int  __down_failed_interruptible(void  /* params in registers */);
asmlinkage int  __down_failed_trylock(void  /* params in registers */);
asmlinkage void __up_wakeup(void /* special register calling convention */);

asmlinkage void __down(struct semaphore * sem);
asmlinkage int  __down_interruptible(struct semaphore * sem);
asmlinkage int  __down_trylock(struct semaphore * sem);
asmlinkage void __up(struct semaphore * sem);

extern spinlock_t semaphore_wake_lock;

/*
 * This is ugly, but we want the default case to fall through.
 * "down_failed" is a special asm handler that calls the C
 * routine that actually waits. See arch/m68k/lib/semaphore.S
 */
extern inline void down(struct semaphore * sem)
{
#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif

	__asm__ __volatile__(
		"stc ccr,r4l\n\t"
		"orc #0x80,ccr\n\t"
		"mov.l @%0, er0\n\t"
		"dec.l #1,er0\n\t"
		"mov.l er0,@%0\n\t"
		"bpl 1f\n\t"
		"ldc r4l,ccr\n\t"
		"mov.l %0,er0\n\t"
		"jsr @_" SYMBOL_NAME_STR(__down) "\n"
		"1:\n\t"
		"ldc r4l,ccr"
		: /* no outputs */
		: "r" (&(sem->count))
		: "cc", "er0", "er1", "er2", "er3", "er4", "memory");
}

extern inline int down_interruptible(struct semaphore * sem)
{
	register int ret __asm__("er0");

#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif

	__asm__ __volatile__(
		"stc ccr,r1l\n\t"
		"orc #0x80,ccr\n\t"
		"mov.l @%1, er2\n\t"
		"dec.l #1,er2\n\t"
		"mov.l er2,@%1\n\t"
		"bpl 1f\n\t"
		"ldc r1l,ccr\n\t"
		"mov.l %1,er0\n\t"
		"jsr @_" SYMBOL_NAME_STR(__down_interruptible) "\n\t"
		"bra 2f\n"
		"1:\n\t"
		"ldc r1l,ccr\n\t"
		"sub.l %0,%0\n\t"
		"2:\n\t"
		: "=r" (ret)
		: "r" (&(sem->count))
		: "cc", "er1", "er2", "er3", "memory");
	return ret;
}

extern inline int down_trylock(struct semaphore * sem)
{
	register int result;

#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif

	__asm__ __volatile__(
		"stc ccr,r4l\n\t"
		"orc #0x80,ccr\n\t"
		"mov.l @%1,er0\n\t"
		"dec.l #1,er0\n\t"
		"mov.l er0,@%1\n\t"
		"bpl 1f\n\t"
		"ldc r4l,ccr\n\t"
		"jmp @3f\n"
		"1:\n\t"
		"ldc r4l,ccr\n\t"
		"sub.l %0,%0\n"
		"2:\n"
		".section .text.lock,\"ax\"\n"
		".align 2\n"
		"3:\n\t"
		"mov.l %1,er0\n\t"
		"jsr @_" SYMBOL_NAME_STR(__down_trylock) "\n"
		"mov.l er0,%0\n\t"
		"jmp @2b\n\t"
		".previous"
		: "=r" (result)
		: "r" (&(sem->count))
		: "cc", "er0","er4", "memory");
	return result;
}

/*
 * Note! This is subtle. We jump to wake people up only if
 * the semaphore was negative (== somebody was waiting on it).
 * The default case (no contention) will result in NO
 * jumps for both down() and up().
 */
extern inline void up(struct semaphore * sem)
{
#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif

	__asm__ __volatile__(
		"stc ccr,r4l\n\t"
		"orc #0x80,ccr\n\t"
		"mov.l @%0,er0\n\t"
		"inc.l #1,er0\n\t"
		"mov.l er0,@%0\n\t"
		"bmi 1f\n\t"
		"bne 2f\n\t"
		"1:\n\t"
		"ldc r4l,ccr\n\t"
		"mov.l %0,er0\n\t"
		"jsr @_" SYMBOL_NAME_STR(__up) "\n"
		"2:\n\t"
                "ldc r4l,ccr"
		: /* no outputs */
		: "r" (&(sem->count))
		: "cc", "er0", "er4", "memory");
}

static inline int sem_getcount(struct semaphore *sem)
{
	return atomic_read(&sem->count);
}

#endif /* __ASSEMBLY__ */

#endif
