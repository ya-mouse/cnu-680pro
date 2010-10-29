# $Id: Makefile,v 1.1.1.1 2003/11/17 02:33:26 jipark Exp $
# Makefile for the Sun Boot PROM interface library under
# Linux.
#

USE_STANDARD_AS_RULE := true
EXTRA_AFLAGS := -ansi

L_TARGET = promlib.a
obj-y   := bootstr.o devops.o init.o memory.o misc.o \
	   tree.o console.o printf.o p1275.o map.o

include $(TOPDIR)/Rules.make

%.o: %.c
	$(CC) $(subst -pg,,$(CFLAGS)) -c $<
