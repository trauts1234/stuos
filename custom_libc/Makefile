CC = x86_64-elf-gcc
LD = x86_64-elf-ld

#the sysroot-esque results directory
OUTPUT_DIR := ../stuos_libc
#where build artifacts are
ARTIFACT_DIR := ./.build
#compiler flags for building libc
CFLAGS = -ffreestanding -nostdlib -O2 -Wall -Wextra -fPIC -march=x86-64 -g -fno-omit-frame-pointer \
	-I include/ -I ../malloc_my_os_3/ -I $(ABI_DIR)


### Building the CRT* files

# where CRT* source is
CRT_DIR := ./crt
CRT_SRCS = $(wildcard $(CRT_DIR)/crt*.asm)
OUTPUT_CRTS = $(CRT_SRCS:$(CRT_DIR)/%.asm=$(OUTPUT_DIR)/%.o)


### Moving the header files

# where UAPI source is
ABI_DIR := ../abi
# where the libc header source is
INCLUDE_DIR := ./include
INCLUDE_SRCS = $(shell find $(INCLUDE_DIR) -name "*.h")
UAPI_SRCS = $(shell find $(ABI_DIR) -name "*.h")

# where the installed headers go
OUTPUT_INCLUDE_DIR = $(OUTPUT_DIR)/include
OUTPUT_HEADERS := $(INCLUDE_SRCS:$(INCLUDE_DIR)/%.h=$(OUTPUT_INCLUDE_DIR)/%.h) $(UAPI_SRCS:$(ABI_DIR)/%.h=$(OUTPUT_INCLUDE_DIR)/%.h)


### Building the libc.* files

#malloc and libm dependencies
MALLOC_SRC_DIR = ../malloc_my_os_3
MALLOC_SRCS = $(wildcard $(MALLOC_SRC_DIR)/*.c)
LIBM_OBJS := $(wildcard /home/stuart/Programming/C/openlibm/objectfiles/*)
#source
C_SRCS = $(wildcard *.c)
ASM_SRCS = $(wildcard *.asm)
#where the .o files go before being linked
LIBC_ARTIFACTS := $(C_SRCS:%.c=$(ARTIFACT_DIR)/%.o) $(ASM_SRCS:%.asm=$(ARTIFACT_DIR)/%.o) $(MALLOC_SRCS:$(MALLOC_SRC_DIR)/%.c=$(ARTIFACT_DIR)/%.o)
#output files
OUTPUT_A = $(OUTPUT_DIR)/libc_stuos.a
OUTPUT_SO = $(OUTPUT_DIR)/libc_stuos.so

all: $(OUTPUT_A) $(OUTPUT_SO) $(OUTPUT_CRTS) $(OUTPUT_HEADERS)

$(OUTPUT_DIR)/crt%.o: $(CRT_DIR)/crt%.asm
	@mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@

#copy headers from uapi and include dir
$(OUTPUT_INCLUDE_DIR)/%.h: $(INCLUDE_DIR)/%.h
	@mkdir -p $(dir $@)
	cp $< $@
$(OUTPUT_INCLUDE_DIR)/%.h: $(ABI_DIR)/%.h
	@mkdir -p $(dir $@)
	cp $< $@

# start by copying the .a archive for malloc, then append my object files on to it
$(OUTPUT_A): $(LIBC_ARTIFACTS) $(LIBM_OBJS)
	@mkdir -p $(dir $@)
	ar rcs $@ $(LIBC_ARTIFACTS) $(LIBM_OBJS)
# Create a dynamic library from the static one
$(OUTPUT_SO): $(OUTPUT_A)
	$(LD) -shared --whole-archive -o $@ $<

# .o files require the .c or .asm source files
$(ARTIFACT_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(ARTIFACT_DIR)/%.o: $(MALLOC_SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DBGFLAGS) -c $< -o $@
$(ARTIFACT_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@

clean:
	rm -rf $(ARTIFACT_DIR) $(OUTPUT_DIR)
