SUBDIRS=gpio sredird

all: $(SUBDIRS)

clean: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGLOBALS)

.PHONY: all clean $(SUBDIRS)
