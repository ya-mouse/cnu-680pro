# $Id: Makefile,v 1.1.1.1 2003/11/17 02:33:24 jipark Exp $
# Makefile for Sparc64 library files..
#

USE_STANDARD_AS_RULE := true
EXTRA_AFLAGS := -ansi

L_TARGET = lib.a
obj-y := PeeCeeI.o blockops.o debuglocks.o strlen.o strncmp.o \
	 memscan.o strncpy_from_user.o strlen_user.o memcmp.o checksum.o \
	 VIScopy.o VISbzero.o VISmemset.o VIScsum.o VIScsumcopy.o \
	 VIScsumcopyusr.o VISsave.o atomic.o rwlock.o bitops.o \
	 dec_and_lock.o U3memcpy.o U3copy_from_user.o U3copy_to_user.o \
	 U3copy_in_user.o mcount.o

include $(TOPDIR)/Rules.make
