SUBDIRS=toolchain uClibc apps

all: $(SUBDIRS)

.stamp-out:
	@echo Checking who am I... am I root?
	@[ "$$(id -u)" = 0 ]
	@echo Yeah!! I am a root!!!odin
	mkdir -p out
	tar xfvj rootfs.tar.bz2 -C out
	# Fix admin's shell. Give me a root.
	sed -i 's,/bin/csh$$,/bin/sh,' out/etc/passwd*
	# Remove snmpd
	rm -f out/bin/snmpd
	@touch .stamp-out

install: .stamp-out $(SUBDIRS)
	mkcramfs -e 0 out 680pro.rom
	@if [ "$$(stat -c '%s' 680pro.rom)" -gt $$((0x340000)) ]; then \
		echo "Size of 680pro.rom image exceeded maximum allowed size of 0x340000 bytes."; \
		false; \
	fi
	/bin/echo -en "\x49\x54\x00\x01\x49\x23\x5F\x8C\xA3\x38\x85\x4C" > 680pro.bin
	printf "%08x" $$(stat -c '%s' 680pro.rom) | sed 's,\([0-9a-f]\{2\}\),\\\\x\1,g' | xargs /bin/echo -en >> 680pro.bin
	cat 680pro.rom >> 680pro.bin
	rm -f 680pro.rom

clean: $(SUBDIRS)
	rm -rf out .stamp-out

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: all clean install out $(SUBDIRS)

