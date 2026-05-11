CC = knc
AS = as
LD = ld

BUILD = out

# EFI Configuration
EFIINC = /usr/include/efi
EFIARCH = /usr/include/efi/ia32

EFILIB = /usr/lib32
CRT0 = /usr/lib32/crt0-efi-ia32.o

OVMF = /usr/share/qemu/OVMF.fd

# Bootloader Compiler Flags
CFLAGS = \
	-I. \
	-Iinclude \
	-I$(EFIINC) \
	-I$(EFIARCH) \
	-fshort-wchar \
	-m32 \
	-fpic

# Kernel-specific flags
KERNEL_CFLAGS = \
	-I. \
	-Iinclude \
	-Ikernel \
	-Iconsole/outputs \
	-m32

ASFLAGS = -m32

# EFI Linker Flags
EFI_LDFLAGS = \
	-nostdlib \
	-znocombreloc \
	-T $(EFILIB)/elf_ia32_efi.lds \
	-shared \
	-Bsymbolic \
	-L $(EFILIB) \
	-lgnuefi \
	-lefi

# Kernel Linker Flags
KERNEL_LDFLAGS = \
	-nostdlib \
	-T kernel/linker32.lds \
	-m elf_i386

# Source files
BOOTLOADER_SRC = boot/nytb3.c
BOOTLOADER_OBJ = $(BUILD)/nytb3.o
BOOTLOADER = $(BUILD)/BOOTIA32.EFI

KERNEL_SRC = \
	kernel/kernel.c \
	kernel/paging/paging.c \
	include/memory.c \
	console/outputs/printk.c \
	console/outputs/outputs_base.c

KERNEL_ASRC = kernel/start.S

KERNEL_OBJ = $(KERNEL_SRC:.c=.o) $(KERNEL_ASRC:.S=.o)

KERNEL = kernel.elf

# Targets
all: $(BOOTLOADER) $(KERNEL)

$(BUILD):
	mkdir -p $(BUILD)

# Bootloader build
$(BOOTLOADER_OBJ): $(BOOTLOADER_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BOOTLOADER): $(BOOTLOADER_OBJ)
	$(LD) $(EFI_LDFLAGS) $(CRT0) $< -o $@

# Kernel assembly
kernel/start.o: kernel/start.S
	$(AS) $(ASFLAGS) $< -o $@

# Kernel C compilation
kernel/%.o: kernel/%.c
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

console/%.o: console/%.c
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

include/%.o: include/%.c
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

# Kernel linking
$(KERNEL): $(KERNEL_OBJ)
	$(LD) $(KERNEL_LDFLAGS) -o $@ $^

# Run in QEMU
run: $(BOOTLOADER) $(KERNEL)
	dd if=/dev/zero of=esp.img bs=1M count=64
	mkfs.vfat esp.img

	mmd -i esp.img EFI
	mmd -i esp.img EFI/BOOT

	mcopy -i esp.img $(BOOTLOADER) ::EFI/BOOT/BOOTIA32.EFI
	mcopy -i esp.img $(KERNEL) ::kernel.elf

	qemu-system-i386 \
	-bios $(OVMF) \
	-drive file=esp.img,format=raw,if=none,id=esp \
	-device virtio-blk-pci,drive=esp \
	-nographic

clean:
	rm -f $(KERNEL_OBJ)
	rm -f $(BOOTLOADER_OBJ)
	rm -f $(BOOTLOADER)
	rm -f $(KERNEL)
	rm -f esp.img
	rm -rf $(BUILD)

.PHONY: all run clean