# $Id: Makefile,v 1.1.1.1 2003/11/17 02:33:22 jipark Exp $
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1999  Kaz Kojima
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies. Remember to do have actions
# for "archclean" and "archdep" for cleaning up and making dependencies for
# this architecture
#

ifdef CONFIG_CPU_LITTLE_ENDIAN
CFLAGS		+= -ml
AFLAGS		+= -ml
# LINKFLAGS	+= -EL
LDFLAGS		:= -EL
else
CFLAGS		+= -mb
AFLAGS		+= -mb
# LINKFLAGS	+= -EB
LDFLAGS		:= -EB
endif

ifdef CONFIG_SH_DSP
AFLAGS += -Wa,-dsp
endif

ifndef CROSS_COMPILE
tool_prefix	= sh-linux-gnu-
# ifdef CONFIG_CROSSCOMPILE
CROSS_COMPILE  = $(tool_prefix)
# endif
endif

LD	=$(CROSS_COMPILE)ld $(LDFLAGS)
OBJCOPY=$(CROSS_COMPILE)objcopy -O binary -R .note -R .comment -R .stab -R .stabstr -S

MODFLAGS	+=

#
#

ifdef CONFIG_CPU_SH3
CFLAGS		+= -m3
AFLAGS		+= -m3
endif
ifdef CONFIG_CPU_SH4
CFLAGS		+= -m4 -m4-nofpu # -mno-implicit-fp
AFLAGS		+= -m4 -m4-nofpu # -mno-implicit-fp
endif

#
# Choosing incompatible machines durings configuration will result in
# error messages during linking.  Select a default linkscript if
# none has been choosen above.
#
LINKSCRIPT    = arch/sh/vmlinux.lds
LINKFLAGS     += -T $(word 1,$(LINKSCRIPT)) -e _stext

ifdef LOADADDR
LINKFLAGS     += -Ttext $(word 1,$(LOADADDR))
endif

#
CFLAGS		+= -pipe

HEAD := arch/sh/kernel/head.o arch/sh/kernel/init_task.o

SUBDIRS := $(SUBDIRS) $(addprefix arch/sh/, kernel mm lib)
CORE_FILES := arch/sh/kernel/kernel.o arch/sh/mm/mm.o $(CORE_FILES)
LIBGCC := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
LIBS := $(TOPDIR)/arch/sh/lib/lib.a $(LIBS) $(TOPDIR)/arch/sh/lib/lib.a \
	$(LIBGCC)

#
# If you want the kernel build to build modules outside of the tree
# then define this and pass it to the main linux makefile
#
ifdef EXTRA_MODULE_DIRS
SUBDIRS += $(EXTRA_MODULE_DIRS)
endif

MAKEBOOT = $(MAKE) -C arch/$(ARCH)/boot

ifneq ($(CONFIG_SH_GENERIC)$(CONFIG_SH_STB1_HARP)$(CONFIG_SH_STB1_OVERDRIVE),)
SUBDIRS += arch/sh/stboards
CORE_FILES += arch/sh/stboards/stboards.o
endif

vmlinux: arch/sh/vmlinux.lds

arch/sh/vmlinux.lds: arch/sh/vmlinux.lds.S FORCE
	$(CPP) -traditional -C -P -I$(HPATH) -Ush arch/sh/vmlinux.lds.S >arch/sh/vmlinux.lds

FORCE: ;

zImage: vmlinux
	@$(MAKEBOOT) zImage

compressed: zImage

zdisk: vmlinux
	@$(MAKEBOOT) zdisk

archclean:
	@$(MAKEBOOT) clean
	$(MAKE) -C arch/$(ARCH)/kernel clean
	$(MAKE) -C arch/$(ARCH)/stboards clean
#	$(MAKE) -C arch/$(ARCH)/tools clean

archmrproper:
	rm -f arch/sh/vmlinux.lds

archdep:
	@$(MAKEBOOT) dep

