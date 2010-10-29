#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= 2510REF.o

# Object file lists.

#coreyliu 20040202 define the obj we want one by one to 
#                  remove the config_reset and factory_default
#obj-y			:= $(patsubst %.c, %.o, $(wildcard *.c))
obj-y			:= arch.o config.o dma.o io.o irq.o smdk2510-pci.o time.o
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:= 

include $(TOPDIR)/Rules.make
