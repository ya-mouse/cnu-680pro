# $Id: Makefile,v 1.1.1.1 2003/11/17 02:32:54 jipark Exp $
#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

# These assembly files can't be assembld with -traditional, so we
# need another build rule than the one in the toplevel Makefile.
.S.o:
	$(CC) $(AFLAGS) -c $< -o $*.o

all: kernel.o head.o

O_TARGET := kernel.o

export-objs := ksyms.o

obj-y   := process.o signal.o entry.o traps.o irq.o \
           ptrace.o setup.o time.o sys_cris.o shadows.o \
	   debugport.o semaphore.o

obj-$(CONFIG_MODULES)    += ksyms.o
obj-$(CONFIG_ETRAX_KGDB) += kgdb.o
obj-$(CONFIG_ETRAX_FAST_TIMER) += fasttimer.o

# This dependency isn't caught by mkdep.  See entry.S.
entry.o: entryoffsets.s

entryoffsets.s: entryoffsets.c
	$(CC) $(CFLAGS) -S -c $<

clean:

include $(TOPDIR)/Rules.make
