SUBDIRS=toolchain uClibc apps

all: $(SUBDIRS)

clean: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGLOBALS)

.PHONY: all clean $(SUBDIRS)

