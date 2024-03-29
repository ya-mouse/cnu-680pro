#
# arch/v850/kernel/Makefile
#
#  Copyright (C) 2001,02  NEC Corporation
#  Copyright (C) 2001,02  Miles Bader <miles@gnu.org>
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#

all: head.o kernel.o

USE_STANDARD_AS_RULE	:= true

O_TARGET := kernel.o
obj-y := intv.o entry.o process.o syscalls.o time.o semaphore.o setup.o \
	 signal.o irq.o mach.o ptrace.o bug.o
export-objs := v850_ksyms.o rte_mb_a_pci.o

obj-$(CONFIG_MODULES)		+= v850_ksyms.o
# chip-specific code
obj-$(CONFIG_V850E_MA1)		+= ma.o	nb85e_utils.o nb85e_timer_d.o
obj-$(CONFIG_V850E_NB85E)	+= nb85e_intc.o
obj-$(CONFIG_V850E2_ANNA)	+= anna.o nb85e_intc.o nb85e_utils.o nb85e_timer_d.o
obj-$(CONFIG_V850E_AS85EP1)	+= as85ep1.o nb85e_intc.o nb85e_utils.o nb85e_timer_d.o
# platform-specific code
obj-$(CONFIG_V850E_SIM)		+= sim.o simcons.o
obj-$(CONFIG_V850E2_SIM85E2C)	+= sim85e2c.o nb85e_intc.o memcons.o
obj-$(CONFIG_V850E2_FPGA85E2C)	+= fpga85e2c.o nb85e_intc.o memcons.o
obj-$(CONFIG_RTE_CB)		+= rte_cb.o rte_cb_leds.o
obj-$(CONFIG_RTE_CB_MA1)	+= rte_ma1_cb.o
obj-$(CONFIG_RTE_CB_NB85E)	+= rte_nb85e_cb.o
obj-$(CONFIG_RTE_CB_MULTI)	+= rte_cb_multi.o
obj-$(CONFIG_RTE_MB_A_PCI)	+= rte_mb_a_pci.o
obj-$(CONFIG_RTE_GBUS_INT)	+= gbus_int.o
# feature-specific code
obj-$(CONFIG_V850E_MA1_HIGHRES_TIMER)	+= highres_timer.o
obj-$(CONFIG_PROC_FS)		+= procfs.o

head.o: head.S v850_defs.h
entry.o: entry.S v850_defs.h

v850_defs.h: v850_defs.c v850_defs.head
	rm -f v850_defs.d
	SUNPRO_DEPENDENCIES="v850_defs.d v850_defs.h" \
	$(CC) $(filter-out -MD,$(CFLAGS)) -S v850_defs.c
	cp v850_defs.head v850_defs.h
	grep '^#define' v850_defs.s >> v850_defs.h
	rm v850_defs.s
-include v850_defs.d

include $(TOPDIR)/Rules.make
