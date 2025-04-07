# OUTPUT

LYKOS_ISO = lykos.iso

# QEMU

QEMU_FLAGS := -m 2G					\
			  -smp 4				\
			  -no-shutdown 			\
			  -no-reboot 			\
			  -cdrom $(LYKOS_ISO) 	\
			  -boot d 				\
			  -d int				\
			  -D qemu/log.txt		\
			  -serial stdio

OVMF := qemu/ovmf-$(ARCH)

run: build $(OVMF)
ifeq ($(ARCH), x86_64)
	qemu-system-x86_64 -M q35 -bios $(OVMF) $(QEMU_FLAGS) 
else ifeq ($(ARCH), aarch64)
	qemu-system-aarch64 -M virt -cpu cortex-a72 -device ramfb -device qemu-xhci -device usb-kbd -device usb-mouse -bios $(OVMF) $(QEMU_FLAGS) 
endif

$(OVMF):
ifeq ($(ARCH), x86_64)
	mkdir -p $(@D)
	curl -Lo $@ https://retrage.github.io/edk2-nightly/bin/RELEASEX64_OVMF.fd
else ifeq ($(ARCH), aarch64)
	mkdir -p $(@D)
	curl -Lo $@ https://retrage.github.io/edk2-nightly/bin/RELEASEAARCH64_QEMU_EFI.fd
endif

# BUILDING

build: limine tools/ksym
	make -C kernel ARCH=$(ARCH)

	mkdir -p iso_root/EFI/BOOT

	llvm-nm kernel/bin/kernel.elf -n > kernel_symbols.txt
	./tools/ksym kernel_symbols.txt iso_root/kernel_symbols.bin
	tar -cvf iso_root/initrd.tar --format=ustar initrd/

	cp kernel/bin/kernel.elf limine.conf limine/limine-uefi-cd.bin \
		./iso_root/

ifeq ($(ARCH), aarch64)
	cp limine/BOOTAA64.EFI \
		./iso_root/EFI/BOOT/

	xorriso -as mkisofs \
			--efi-boot limine-uefi-cd.bin \
			-efi-boot-part --efi-boot-image --protective-msdos-label \
			iso_root -o $(LYKOS_ISO)
else ifeq ($(ARCH), x86_64)
	cp -v limine/BOOTX64.EFI \
		./iso_root/EFI/BOOT/

	xorriso -as mkisofs \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(LYKOS_ISO)
endif

limine:
	git clone https://github.com/limine-bootloader/limine.git --branch=v8.x-binary --depth=1

tools/ksym:
	cc tools/ksym.c -O2 -o tools/ksym

# CLEAN

clean:
	rm -rf iso_root
	rm -rf $(LYKOS_ISO)
	rm -f  qemu/log.txt
	rm -rf initrd.tar

	make -C kernel clean

distclean: clean
	rm -rf limine
	rm -rf qemu
	rm -f  tools/ksym

	make -C kernel distclean
	