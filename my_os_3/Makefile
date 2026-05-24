CC = /home/stuart/Programming/C/stuos_core/tcc/tcc
LD = x86_64-elf-ld

SRC_DIR := ./src
LIMINE_CONFIG := $(SRC_DIR)/limine.conf
C_SRCS = $(wildcard $(SRC_DIR)/*.c)
ASM_SRCS = $(wildcard $(SRC_DIR)/*.asm)

CFLAGS = -ffreestanding -m64 -nostdlib -O0 -Wall -Wextra -mcmodel=kernel -fno-pie -mno-red-zone -march=x86-64 -I../abi -I ../malloc_my_os_3/
DBGFLAGS = -g -fno-omit-frame-pointer
LDFLAGS = -T $(SRC_DIR)/linker.lds -nostdlib

# location of build artifacts
BUILD_DIR := ./.build
OS_BINARY := $(BUILD_DIR)/myos
OBJ_DIR := $(BUILD_DIR)/obj
OBJS := $(C_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)  $(ASM_SRCS:$(SRC_DIR)/%.asm=$(OBJ_DIR)/%.o)
MALLOC_ARCHIVE := ../malloc_my_os_3/target/x86_64-unknown-none/debug/libmalloc_my_os_3.a

LIMINE_FOLDER = ./limine
OUTPUT_DISK = ../filesystem
OUTPUT_DIR = ../filesystem_mnt

all: $(OUTPUT_DISK)
# all: $(OS_BINARY)

$(OUTPUT_DISK): $(BUILD_DIR)/built.stamp
	@ mountpoint -q $(OUTPUT_DIR)

	$(LIMINE_FOLDER)/limine bios-install $(OUTPUT_DISK)

# making the filesystem requires the project to have been compiled and limine to be installed
#use a .stamp file as directory timestamps are useless in linux
$(BUILD_DIR)/built.stamp: $(OS_BINARY) 
	mkdir -p $(OUTPUT_DIR)/boot/limine $(OUTPUT_DIR)/EFI/BOOT $(OUTPUT_DIR)/dev
	cp $(LIMINE_CONFIG) $(LIMINE_FOLDER)/limine-bios.sys $(LIMINE_FOLDER)/limine-bios-cd.bin $(LIMINE_FOLDER)/limine-uefi-cd.bin $(OUTPUT_DIR)/boot/limine/
	cp $(LIMINE_FOLDER)/BOOTX64.EFI $(LIMINE_FOLDER)/BOOTIA32.EFI $(OUTPUT_DIR)/EFI/BOOT
	cp $< $(OUTPUT_DIR)/boot
	touch $(BUILD_DIR)/built.stamp

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
	rm -rf $(BUILD_DIR) disasm.asm
