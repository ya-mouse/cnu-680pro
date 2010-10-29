all: .installed

clean install:

.installed:
	sudo rsync -avP arm-elf bin lib /usr/local/
	@touch .installed

.PHONY: all clean install

