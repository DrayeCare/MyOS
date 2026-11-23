CC = gcc

CFLAGS = -ffreestanding -fno-stack-protector \
         -fno-pic -fno-pie -mno-red-zone \
         -mcmodel=kernel -Wall -Wextra \
         -Iinclude

LDFLAGS = -nostdlib -static -no-pie \
          -Wl,-T,linker.ld \
          -Wl,--build-id=none

BUILD = build
ISO_ROOT = iso_root
ISO = $(BUILD)/MyOS.iso

HEADERS = include/limine.h include/font8x8_basic.h \
          include/framebuffer.h include/console.h \
          include/interrupts.h include/keyboard.h include/input.h \
          include/shell.h include/version.h include/pmm.h

KERNEL_OBJS = $(BUILD)/main.o $(BUILD)/framebuffer.o $(BUILD)/console.o \
              $(BUILD)/interrupts.o $(BUILD)/keyboard.o $(BUILD)/input.o \
              $(BUILD)/shell.o $(BUILD)/pmm.o $(BUILD)/isr.o

OVMF_CODE ?= /usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS = $(BUILD)/OVMF_VARS.fd

# Host-side tests use the real console/framebuffer sources with a mock Limine
# header.  -Itests must precede -Iinclude so the mock wins for <limine.h>.
TEST_DIR = tests
TEST_BUILD = $(BUILD)/tests
TEST_BIN = $(TEST_BUILD)/test_console
TEST_SOURCES = $(TEST_DIR)/test_console.c \
               kernel/console.c kernel/framebuffer.c
TEST_KEYBOARD_SOURCES = $(TEST_DIR)/test_keyboard.c kernel/keyboard.c
TEST_INPUT_SOURCES = $(TEST_DIR)/test_input.c \
                     kernel/input.c kernel/console.c kernel/framebuffer.c
TEST_SHELL_SOURCES = $(TEST_DIR)/test_shell.c kernel/shell.c kernel/input.c \
                     kernel/console.c kernel/framebuffer.c kernel/pmm.c
TEST_PMM_SOURCES = $(TEST_DIR)/test_pmm.c kernel/pmm.c
TEST_BINS = $(TEST_BUILD)/test_console \
            $(TEST_BUILD)/test_keyboard \
            $(TEST_BUILD)/test_input \
            $(TEST_BUILD)/test_shell \
            $(TEST_BUILD)/test_pmm
HOST_CC = gcc
HOST_CFLAGS = -std=gnu11 -Wall -Wextra -I$(TEST_DIR) -Iinclude

all: $(BUILD)/kernel

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: kernel/%.c $(HEADERS) linker.ld | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: kernel/%.S linker.ld | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Keep the ISO staging copy in lockstep with the linked kernel.  The ISO
# itself is only rebuilt by the `iso` target.
$(BUILD)/kernel: $(KERNEL_OBJS) linker.ld | $(BUILD) $(ISO_ROOT)/boot
	$(CC) $(LDFLAGS) $(KERNEL_OBJS) -o $@
	cp $@ $(ISO_ROOT)/boot/kernel

$(ISO_ROOT)/boot:
	mkdir -p $(ISO_ROOT)/boot

# Limine BIOS El Torito image + Limine UEFI boot image are both preserved.
$(ISO): $(BUILD)/kernel $(ISO_ROOT)/limine.conf \
        $(ISO_ROOT)/boot/limine-bios-cd.bin \
        $(ISO_ROOT)/boot/limine-bios.sys \
        $(ISO_ROOT)/boot/limine-uefi-cd.bin \
        $(ISO_ROOT)/EFI/BOOT/BOOTX64.EFI
	xorriso -as mkisofs -R -r -J \
    -b boot/limine-bios-cd.bin \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    --efi-boot boot/limine-uefi-cd.bin \
    -efi-boot-part --efi-boot-image --protective-msdos-label \
    $(ISO_ROOT) -o $@

iso: $(ISO)

# Create the variable store only when absent; never overwrite an existing one.
$(OVMF_VARS): | $(BUILD)
	cp /usr/share/OVMF/OVMF_VARS_4M.fd $@

run: iso $(OVMF_VARS)
	qemu-system-x86_64 -M q35 -m 256M \
	    -drive if=pflash,format=raw,unit=0,file=$(OVMF_CODE),readonly=on \
	    -drive if=pflash,format=raw,unit=1,file=$(OVMF_VARS) \
	    -cdrom $(ISO) -serial stdio

run-bios: iso
	qemu-system-x86_64 -M q35 -m 256M -cdrom $(ISO) -serial stdio

test: $(TEST_BINS)
	$(TEST_BUILD)/test_console
	$(TEST_BUILD)/test_keyboard
	$(TEST_BUILD)/test_input
	$(TEST_BUILD)/test_shell
	$(TEST_BUILD)/test_pmm

$(TEST_BUILD):
	mkdir -p $(TEST_BUILD)

$(TEST_BIN): $(TEST_SOURCES) $(HEADERS) $(TEST_DIR)/limine.h | $(TEST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(TEST_SOURCES) -o $@

$(TEST_BUILD)/test_keyboard: $(TEST_KEYBOARD_SOURCES) \
        include/keyboard.h include/interrupts.h | $(TEST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(TEST_KEYBOARD_SOURCES) -o $@

$(TEST_BUILD)/test_input: $(TEST_INPUT_SOURCES) $(HEADERS) \
        $(TEST_DIR)/limine.h | $(TEST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(TEST_INPUT_SOURCES) -o $@

$(TEST_BUILD)/test_shell: $(TEST_SHELL_SOURCES) $(HEADERS) \
        $(TEST_DIR)/limine.h | $(TEST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(TEST_SHELL_SOURCES) -o $@

$(TEST_BUILD)/test_pmm: $(TEST_PMM_SOURCES) $(HEADERS) include/pmm.h | $(TEST_BUILD)
	$(HOST_CC) $(HOST_CFLAGS) $(TEST_PMM_SOURCES) -o $@

# Removes generated objects/kernel/ISO only.  OVMF_VARS.fd is intentionally
# preserved: rebuilding must not discard the firmware variable store.
clean:
	rm -f $(KERNEL_OBJS) $(BUILD)/kernel $(ISO)
	rm -rf $(TEST_BUILD)

.PHONY: all iso run run-bios test clean
