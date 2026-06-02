CC = /home/stuart/Programming/C/stuos_core/toolchain_output/x86_64-tcc
LD = x86_64-elf-ld

SRC_DIR := ./src
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

all: $(OS_BINARY)

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
	rm -rf $(BUILD_DIR)
