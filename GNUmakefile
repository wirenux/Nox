# top-level dispatcher

.PHONY: all run debug clean

QEMU  := qemu-system-x86_64
BUILD := build

all:
	$(MAKE) -C kernel
	$(MAKE) -C iso

run: all
	$(QEMU)              \
	    -cdrom $(BUILD)/nox.iso \
	    -m 256M          \
	    -serial stdio    \
		-vga std		 \
	    -no-reboot       \
	    -no-shutdown	\
		-display cocoa,zoom-to-fit=on

debug: all
	$(QEMU)              \
	    -cdrom $(BUILD)/nox.iso \
	    -m 256M          \
	    -serial stdio    \
	    -no-reboot       \
	    -no-shutdown     \
		-display cocoa,zoom-to-fit=on \
	    -s -S

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C iso    clean
