ARCH ?= x86_64

CC = clang
CXX = clang++
LD = ld.lld

BUILD_DIR = $(shell pwd)/build
LIMINE_DIR = /usr/local/share/limine
CONFIG_DIR = $(BUILD_DIR)/config

COMMON_FLAGS = -ffreestanding -O3
ifeq ($(ARCH),x86_64)
    TARGET = x86_64-none-elf
    ARCH_FLAGS = -march=x86-64-v3 
endif

CFLAGS = $(COMMON_FLAGS) $(ARCH_FLAGS) --target=$(TARGET) -mcmodel=kernel -mno-red-zone -mgeneral-regs-only
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti

export ARCH CC CXX LD BUILD_DIR CFLAGS CXXFLAGS

KERNEL_NAME = kafka-kernel-$(ARCH)
KERNEL_BIN = $(BUILD_DIR)/$(KERNEL_NAME)

$(shell mkdir -p $(BUILD_DIR))
$(shell mkdir -p $(BUILD_DIR)/obj/kernel)
$(shell mkdir -p $(CONFIG_DIR))

all: configure kernel

kernel: configure
	@echo "Building kernel for $(ARCH) architecture..."
	@$(MAKE) -C kernel
	@echo "Kernel build complete."

iso: kernel
	@echo "Creating bootable ISO..."
	@mkdir -p $(BUILD_DIR)/iso_root/boot/limine
	@cp $(KERNEL_BIN) $(BUILD_DIR)/iso_root/boot/kernel
	@cp boot/limine.conf $(BUILD_DIR)/iso_root/boot
	@cp boot/wallpaper.png $(BUILD_DIR)/iso_root/boot
	@cp $(LIMINE_DIR)/limine-bios-cd.bin $(BUILD_DIR)/iso_root/boot/limine/
	@cp $(LIMINE_DIR)/limine-uefi-cd.bin $(BUILD_DIR)/iso_root/boot/limine/
	@cp $(LIMINE_DIR)/limine-bios.sys $(BUILD_DIR)/iso_root/boot/limine/
	@mkdir -p $(BUILD_DIR)/iso_root/EFI/BOOT
	@cp $(LIMINE_DIR)/BOOTX64.EFI $(BUILD_DIR)/iso_root/EFI/BOOT/
	@xorriso -as mkisofs -R -r -J \
		-b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(BUILD_DIR)/iso_root -o $(BUILD_DIR)/kafka-$(ARCH).iso
	@limine bios-install $(BUILD_DIR)/kafka-$(ARCH).iso
	@echo "ISO creation complete: $(BUILD_DIR)/kafka-$(ARCH).iso"

run: iso
	@echo "Running kernel in QEMU..."
	@qemu-system-$(ARCH) -m 2G $(BUILD_DIR)/kafka-$(ARCH).iso

configure:
	@echo "Generating compile_commands.json..."
	@compiledb -o $(BUILD_DIR)/compile_commands.json -n make -C kernel

build: kernel iso

clean:
	@echo "Cleaning build files..."
	@$(MAKE) -C kernel clean
	@rm -rf $(BUILD_DIR) iso_root
	@echo "Clean complete."

.PHONY: all kernel iso run configure vscode_config build clean