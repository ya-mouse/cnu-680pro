# $Id: Makefile,v 1.1.1.1 2003/11/17 02:33:24 jipark Exp $
# Makefile for the Sun Boot PROM interface library under
# Linux.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

L_TARGET = promlib.a

obj-y := bootstr.o devmap.o devops.o init.o memory.o misc.o mp.o \
	 palloc.o ranges.o segment.o console.o printf.o tree.o

obj-$(CONFIG_SUN4) += sun4prom.o

include $(TOPDIR)/Rules.make
