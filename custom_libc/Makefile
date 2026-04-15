CC = x86_64-elf-gcc

# where UAPI source is
ABI_DIR := ../abi
# where the libc header source is
INCLUDE_DIR := ./include

OUTPUT_DIR := ../stuos_libc
# where the installed headers go
OUTPUT_INCLUDE_DIR = $(OUTPUT_DIR)/include

CFLAGS = -ffreestanding -m64 -nostdlib -O0 -Wall -Wextra -fno-pie -mno-red-zone -march=x86-64 -g -fno-omit-frame-pointer -fno-builtin \
	-I include/ -I ../malloc_my_os_3/ -I $(ABI_DIR)

C_SRCS = $(wildcard *.c)
ASM_SRCS = $(wildcard *.asm)
INCLUDE_SRCS = $(shell find $(INCLUDE_DIR) -name "*.h")
UAPI_SRCS = $(shell find $(ABI_DIR) -name "*.h")
MALLOC_LIBRARY := ../malloc_my_os_3/target/x86_64-unknown-none/debug/libmalloc_my_os_3.a

ARTIFACT_DIR := ./.build
ARTIFACT_OBJS := $(C_SRCS:%.c=$(ARTIFACT_DIR)/%.o) $(ASM_SRCS:%.asm=$(ARTIFACT_DIR)/%.o)

OUTPUT_BINARY = $(OUTPUT_DIR)/libc.a
OUTPUT_HEADERS := $(INCLUDE_SRCS:$(INCLUDE_DIR)/%.h=$(OUTPUT_INCLUDE_DIR)/%.h) $(UAPI_SRCS:$(ABI_DIR)/%.h=$(OUTPUT_INCLUDE_DIR)/%.h)

all: $(OUTPUT_BINARY) $(OUTPUT_HEADERS)

# need to make libc
# start by copying the .a archive for malloc, then append my object files on to it
$(OUTPUT_BINARY): $(ARTIFACT_OBJS) $(MALLOC_LIBRARY)
	@mkdir -p $(dir $@)
	cp $(MALLOC_LIBRARY) $@
	ar rs $@ $(ARTIFACT_OBJS)

# .o files require the .c or .asm source files
$(ARTIFACT_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(ARTIFACT_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@


#copy headers from uapi and include dir
$(OUTPUT_INCLUDE_DIR)/%.h: $(INCLUDE_DIR)/%.h
	@mkdir -p $(dir $@)
	cp $< $@
$(OUTPUT_INCLUDE_DIR)/%.h: $(ABI_DIR)/%.h
	@mkdir -p $(dir $@)
	cp $< $@

clean:
	rm -rf $(ARTIFACT_DIR) $(OUTPUT_DIR)
