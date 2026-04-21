CC = x86_64-elf-gcc
LD = x86_64-elf-ld

SRC_DIR := ./src
LIMINE_CONFIG := $(SRC_DIR)/limine.conf
C_SRCS = $(wildcard $(SRC_DIR)/*.c)
ASM_SRCS = $(wildcard $(SRC_DIR)/*.asm)

CFLAGS = -ffreestanding -m64 -nostdlib -O2 -Wall -Wextra -mcmodel=kernel -fno-pie -mno-red-zone -march=x86-64 -I../abi -I ../malloc_my_os_3/
DBGFLAGS = -g -fno-omit-frame-pointer
LDFLAGS = -T $(SRC_DIR)/linker.lds -nostdlib

# location of build artifacts
BUILD_DIR := ./.build
OS_BINARY := $(BUILD_DIR)/myos
OS_ROOTDIR := $(BUILD_DIR)/iso_root
OBJ_DIR := $(BUILD_DIR)/obj
OBJS := $(C_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)  $(ASM_SRCS:$(SRC_DIR)/%.asm=$(OBJ_DIR)/%.o)
MALLOC_ARCHIVE := ../malloc_my_os_3/target/x86_64-unknown-none/debug/libmalloc_my_os_3.a

INCLUDED_BINARIES_PATH := ./included_binaries
INCL_BINS := $(shell find $(INCLUDED_BINARIES_PATH) -type f)#find all binaries to be bundled with the OS

LIMINE_FOLDER = ./limine
OUTPUT = ../stuos_image.iso

all: $(OUTPUT)

disasm: disasm.asm

disasm.asm: $(OS_BINARY)
	objdump -d -M intel -S $(OS_BINARY) > disasm.asm

# making the ISO requires the filesystem to be set up in iso_root/
$(OUTPUT): $(OS_ROOTDIR)/built.stamp
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(OS_ROOTDIR) -o $(OUTPUT)
	$(LIMINE_FOLDER)/limine bios-install $(OUTPUT)

# making the filesystem requires the project to have been compiled and limine to be installed
#use a .stamp file as directory timestamps are useless in linux
$(OS_ROOTDIR)/built.stamp: $(OS_BINARY) $(OS_ROOTDIR)/boot/binaries.tar
	touch $(OS_ROOTDIR)/built.stamp
	mkdir -p $(OS_ROOTDIR)/boot/limine $(OS_ROOTDIR)/EFI/BOOT
	cp $(LIMINE_CONFIG) $(LIMINE_FOLDER)/limine-bios.sys $(LIMINE_FOLDER)/limine-bios-cd.bin $(LIMINE_FOLDER)/limine-uefi-cd.bin $(OS_ROOTDIR)/boot/limine/
	cp $(LIMINE_FOLDER)/BOOTX64.EFI $(LIMINE_FOLDER)/BOOTIA32.EFI $(OS_ROOTDIR)/EFI/BOOT
	cp $< $(OS_ROOTDIR)/boot

$(OS_ROOTDIR)/boot/binaries.tar: $(INCL_BINS)
	@mkdir -p $(dir $@)
	tar --format=ustar --owner=0 --group=0 -C $(INCLUDED_BINARIES_PATH) -cf $@ $(patsubst $(INCLUDED_BINARIES_PATH)/%,%,$(INCL_BINS))

# linking the os requires the .o files and the malloc library
$(OS_BINARY): $(OBJS) $(MALLOC_ARCHIVE)
	@mkdir -p $(dir $@)
	$(LD) $(OBJS) $(MALLOC_ARCHIVE) $(LDFLAGS) -o $@

# .o files require the .c source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DBGFLAGS) -c $< -o $@

# .o files could also require the corresponding .asm assembly file
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(OUTPUT) disasm.asm
