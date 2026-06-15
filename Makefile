# Makefile for m1keOS - multi-file kernel build
# Uses i686-elf cross-compiler if present, else host gcc -m32.

NASM = nasm

ifeq ($(shell which i686-elf-gcc 2>/dev/null),)
  CC = gcc
  LD = gcc
  CROSS_CFLAGS  = -m32
  CROSS_LDFLAGS = -m32 -no-pie
else
  CC = i686-elf-gcc
  LD = i686-elf-gcc
  CROSS_CFLAGS  =
  CROSS_LDFLAGS =
endif

# QEMU detection
QEMU = $(shell which qemu-system-i386 2>/dev/null || which qemu-system-x86_64 2>/dev/null || true)
QEMU_DISPLAY ?= none
ifeq ($(strip $(QEMU)),)
$(error qemu-system-i386 / qemu-system-x86_64 not found; install qemu)
endif

INCLUDES = -Ikernel -Ikernel/include
CFLAGS = $(CROSS_CFLAGS) -std=gnu11 -ffreestanding -O2 -Wall -Wextra \
         -fno-pic -fno-pie -fno-stack-protector -fno-omit-frame-pointer \
         -fno-builtin -nostdlib -mno-sse -mno-sse2 -mno-mmx -mgeneral-regs-only \
         -mincoming-stack-boundary=2 $(INCLUDES)
LDFLAGS = $(CROSS_LDFLAGS) -T linker.ld -nostdlib -Wl,-z,noexecstack
LIBS = -lgcc

OBJ_DIR = obj
BIN_DIR = bin
ISO_DIR = iso
ISO     = m1keos.iso
KERNEL_BIN = $(BIN_DIR)/kernel.bin

# Sources
ASM_SRCS := boot/boot.asm kernel/arch/lowlevel.asm
C_SRCS   := $(shell find kernel -name '*.c' | sort)

ASM_OBJS := $(patsubst %.asm,$(OBJ_DIR)/%.o,$(ASM_SRCS))
C_OBJS   := $(patsubst %.c,$(OBJ_DIR)/%.o,$(C_SRCS))
OBJS     := $(ASM_OBJS) $(C_OBJS)

all: $(ISO)

$(OBJ_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(NASM) -f elf32 $< -o $@

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(OBJS) linker.ld
	@mkdir -p $(BIN_DIR)
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

# GRUB config + ISO
$(ISO): $(KERNEL_BIN)
	@mkdir -p $(ISO_DIR)/boot/grub
	@printf 'set timeout=0\nset default=0\n\nmenuentry "m1keOS" {\n    multiboot /boot/kernel.bin\n    boot\n}\n' > $(ISO_DIR)/boot/grub/grub.cfg
	@cp $(KERNEL_BIN) $(ISO_DIR)/boot/
	grub-mkrescue -o $(ISO) $(ISO_DIR) 2>/dev/null

run: $(ISO)
	$(QEMU) -cdrom $(ISO) -m 256M -serial stdio -display $(QEMU_DISPLAY)

# Interactive graphical window. grab-on-hover lets the PS/2 mouse work without
# clicking to grab; release the pointer with Ctrl+Alt+G.
gui: $(ISO)
	$(QEMU) -cdrom $(ISO) -m 256M -vga std -serial stdio -display gtk,grab-on-hover=on

# Headless test: feed commands over serial, capture output, auto-quit
test: $(ISO)
	@echo '--- m1keOS headless boot test ---'
	printf '\nuname\nm1kectl list\nm1kectl inspect kernel\nm1kectl scheduler set round-robin\nm1kectl desktop theme set blue\nm1kectl module list\nm1kectl module unload mouse\nm1kectl service restart network\nm1pkg install neofetch\nm1pkg list\ndmesg\ncat /etc/m1ke/system.conf\n' | \
	  timeout 10 $(QEMU) -cdrom $(ISO) -m 256M -serial stdio -display none -no-reboot || true

debug: $(ISO)
	$(QEMU) -cdrom $(ISO) -m 256M -s -S -serial stdio -display $(QEMU_DISPLAY)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(ISO_DIR) $(ISO)

.PHONY: all run gui test debug clean
