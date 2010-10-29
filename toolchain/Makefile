all: .installed

clean:

.installed:
	sudo rsync -avP arm-elf bin lib /usr/local/
	@touch .installed

.PHONY: all clean

