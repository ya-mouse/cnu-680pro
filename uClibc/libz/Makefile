#################################################
# Makefile for libz.a ( use uClibc )
# by jkglee
# 2002.09.27
#################################################

TOPDIR=../
include $(TOPDIR)Rules.mak

LIBGIGAROUTER=libz.a
LIBGIGAROUTER_SHARED=libz.so
LIBGIGAROUTER_SHARED_FULLNAME=libz-$(MAJOR_VERSION).$(MINOR_VERSION).$(SUBLEVEL).so

CSRC = adler32.c compress.c crc32.c gzio.c uncompr.c deflate.o trees.c \
       zutil.c inflate.c infblock.c inftrees.c infcodes.c infutil.c inffast.c
OBJS=$(patsubst %.c,%.o, $(CSRC))

all: $(OBJS) $(LIBGIGAROUTER)

$(LIBGIGAROUTER): ar-target

ar-target: $(OBJS)
	$(AR) $(ARFLAGS) $(LIBGIGAROUTER) $(OBJS)
	install -d $(TOPDIR)lib
	rm -f $(TOPDIR)lib/$(LIBGIGAROUTER)
	install -m 644 $(LIBGIGAROUTER) $(TOPDIR)lib
	cp zlib.h ../include
	cp zconf.h ../include

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@
	$(STRIPTOOL) -x -R .note -R .comment $*.o

$(OBJ): Makefile

shared: all
	$(LD) $(LDFLAGS) -soname=$(LIBGIGAROUTER_SHARED).$(MAJOR_VERSION) \
		-o $(LIBGIGAROUTER_SHARED_FULLNAME) --whole-archive $(LIBGIGAROUTER) \
		--no-whole-archive $(TOPDIR)/libc/misc/internals/interp.o \
		-L$(TOPDIR)/lib -lc;
	install -d $(TOPDIR)lib
	rm -f $(TOPDIR)lib/$(LIBGIGAROUTER_SHARED_FULLNAME) $(TOPDIR)lib/$(LIBGIGAROUTER_SHARED).$(MAJOR_VERSION)
	install -m 644 $(LIBGIGAROUTER_SHARED_FULLNAME) $(TOPDIR)lib;
	(cd $(TOPDIR)lib && ln -sf $(LIBGIGAROUTER_SHARED_FULLNAME) $(LIBGIGAROUTER_SHARED)); 
	(cd $(TOPDIR)lib && ln -sf $(LIBGIGAROUTER_SHARED_FULLNAME) $(LIBGIGAROUTER_SHARED).$(MAJOR_VERSION)); 


clean: 
	rm -f *.[oa] *~ core

