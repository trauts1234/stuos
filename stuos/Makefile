CC = x86_64-elf-gcc
# /home/stuart/Programming/C/stuos_core/toolchain_output/x86_64-tcc
LD = x86_64-elf-ld


SRC_DIR := ./src
C_SRCS = $(wildcard $(SRC_DIR)/*.c)
ASM_SRCS = $(wildcard $(SRC_DIR)/*.asm)
MALLOC_SRC_DIR = ../custom_malloc
MALLOC_SRCS = $(wildcard $(MALLOC_SRC_DIR)/*.c)

#frame pointer is needed for HCF backtraces
#-Wno-packed-bitfield-compat gcc fixed a packing issue with chars in 4.4
CFLAGS = -ffreestanding -m64 -nostdlib -O0 -fno-omit-frame-pointer -Wall -Wextra -Wunused -Werror -Wno-packed-bitfield-compat -mcmodel=kernel -fno-pie -mno-red-zone -march=x86-64 -I../abi -I ../custom_malloc/
DBGFLAGS = -g
LDFLAGS = -T $(SRC_DIR)/linker.lds -nostdlib

# location of build artifacts
BUILD_DIR := ./.build
OBJ_DIR := $(BUILD_DIR)/obj
OS_BINARY := $(BUILD_DIR)/stuos
OBJS := $(C_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)  $(ASM_SRCS:$(SRC_DIR)/%.asm=$(OBJ_DIR)/%.o) $(MALLOC_SRCS:$(MALLOC_SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

all: $(OS_BINARY)

# linking the os requires the .o files and the malloc library
$(OS_BINARY): $(OBJS)
	$(LD) $^ $(LDFLAGS) -o $@

# .o files require the .c source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DBGFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(MALLOC_SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DBGFLAGS) -c $< -o $@

# .o files could also require the corresponding .asm assembly file
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@

clean:
	rm -rf $(BUILD_DIR)
