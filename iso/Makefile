BUILD     := ../build
LIMINE    := ../limine
KERNEL    := $(BUILD)/nox.elf
ISO_ROOT  := $(BUILD)/iso_root
ISO       := $(BUILD)/nox.iso

.PHONY: all clean

all: $(ISO)

$(ISO): $(KERNEL)
	@mkdir -p $(ISO_ROOT)/boot/limine
	cp $(KERNEL)                    $(ISO_ROOT)/boot/
	cp limine.conf                  $(ISO_ROOT)/boot/limine/
	cp $(LIMINE)/limine-bios.sys    $(ISO_ROOT)/boot/limine/
	cp $(LIMINE)/limine-bios-cd.bin $(ISO_ROOT)/boot/limine/
	cp $(LIMINE)/limine-uefi-cd.bin $(ISO_ROOT)/boot/limine/
	cp $(LIMINE)/BOOTX64.EFI        $(ISO_ROOT)/boot/limine/
	xorriso -as mkisofs                                           \
	    -b boot/limine/limine-bios-cd.bin                        \
	    -no-emul-boot -boot-load-size 4 -boot-info-table         \
	    --efi-boot boot/limine/limine-uefi-cd.bin                \
	    -efi-boot-part --efi-boot-image --protective-msdos-label \
	    $(ISO_ROOT) -o $(ISO)
	$(LIMINE)/limine bios-install $(ISO)

clean:
	rm -rf $(ISO_ROOT) $(ISO)
