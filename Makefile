ARCH ?= x86_64

CC = clang
CXX = clang++
LD = ld.lld

BUILD_DIR = $(shell pwd)/build
PUB_INCLUDE_DIR = $(shell pwd)/include
LIMINE_DIR = /usr/local/share/limine
CONFIG_DIR = $(BUILD_DIR)/config

COMMON_FLAGS = -ffreestanding -O3
ifeq ($(ARCH),x86_64)
    TARGET = x86_64-none-elf
    ARCH_FLAGS = -march=x86-64 -mcmodel=kernel -mgeneral-regs-only -mno-red-zone
else ifeq ($(ARCH), aarch64)
    TARGET = aarch64-unknown-none
    ARCH_FLAGS = -mgeneral-regs-only
endif

CFLAGS = $(COMMON_FLAGS) $(ARCH_FLAGS) --target=$(TARGET)
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti

export ARCH CC CXX LD BUILD_DIR CFLAGS CXXFLAGS PUB_INCLUDE_DIR

KERNEL_NAME = kafka-kernel-$(ARCH)
KERNEL_BIN = $(BUILD_DIR)/$(KERNEL_NAME)

$(shell mkdir -p $(BUILD_DIR))
$(shell mkdir -p $(BUILD_DIR)/obj/kernel)
$(shell mkdir -p $(BUILD_DIR)/obj/drivers)
$(shell mkdir -p $(CONFIG_DIR))

all: configure arch driver kernel

driver: configure
	@echo "Building drivers for $(ARCH) architecture..."
	@$(MAKE) -C drivers
	@echo "Drivers build complete."

arch:
	@echo "Building architecture-specific library..."
	@$(MAKE) -C arch
	@echo "Architecture build complete."

kernel: configure arch driver
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
ifeq ($(ARCH),x86_64)
	@qemu-system-$(ARCH) -M q35 -m 2G $(BUILD_DIR)/kafka-$(ARCH).iso
else ifeq ($(ARCH),aarch64)
	@qemu-system-$(ARCH) -M virt -cpu cortex-a72 -m 2G -device ramfb -device qemu-xhci -device usb-kbd -device usb-mouse -cdrom $(BUILD_DIR)/kafka-$(ARCH).iso
endif

configure:
	@echo "Generating compile_commands.json..."
	@cd $(shell pwd) && compiledb -n make arch driver kernel
	@mv compile_commands.json $(BUILD_DIR)/compile_commands.json

build: kernel iso

clean:
	@echo "Cleaning build files..."
	@$(MAKE) -C drivers clean
	@$(MAKE) -C kernel clean
	@rm -rf $(BUILD_DIR) iso_root
	@echo "Clean complete."

.PHONY: all arch driver kernel iso run configure build clean