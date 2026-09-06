CC = x86_64-elf-gcc
LD = x86_64-elf-ld

#where build artifacts are
ARTIFACT_DIR := ./.build
#where to install to
SYSROOT = ../sysroot

#compiler flags for building libc
CFLAGS = -ffreestanding -nostdlib -O2 -Wall -Wextra -fPIC -march=x86-64 -g -fno-omit-frame-pointer \
	-I include/ -I ../custom_malloc/ -I ../abi

MALLOC_SRC_DIR = ../custom_malloc
HEADER_SRC_DIR = ./include
CRT_DIR := ./crt

#source
#for libc.*
C_SRCS = $(wildcard *.c)
ASM_SRCS = $(wildcard *.asm)
MALLOC_SRCS = $(wildcard $(MALLOC_SRC_DIR)/*.c)
#for the headers
HEADER_SRCS = $(find $(HEADER_SRC_DIR) -type f)
#for the crt*.o files
CRT_SRCS = $(wildcard $(CRT_DIR)/crt*.asm)

#TODO copy this into the repo and compile normally :(
LIBM_OBJS := $(wildcard /home/stuart/Programming/C/openlibm/objectfiles/*)
#where the .o files go before being linked
LIBC_ARTIFACTS := $(C_SRCS:%.c=$(ARTIFACT_DIR)/%.o) $(ASM_SRCS:%.asm=$(ARTIFACT_DIR)/%.o) $(MALLOC_SRCS:$(MALLOC_SRC_DIR)/%.c=$(ARTIFACT_DIR)/%.o)

#output files
OUTPUT_A = $(SYSROOT)/usr/lib/libc.a
OUTPUT_SO = $(SYSROOT)/usr/lib/libc.so
OUTPUT_CRTS = $(CRT_SRCS:$(CRT_DIR)/%.asm=$(SYSROOT)/usr/lib/%.o)
OUTPUT_HEADERS = $(HEADER_SRCS:$(HEADER_SRC_DIR)/%=$(SYSROOT)/usr/include/%)

all: $(OUTPUT_A) $(OUTPUT_SO) $(OUTPUT_CRTS) $(OUTPUT_HEADERS)

$(OUTPUT_A): $(LIBC_ARTIFACTS) $(LIBM_OBJS)
	ar rcs $@ $(LIBC_ARTIFACTS) $(LIBM_OBJS)
# Create a dynamic library from the static one
$(OUTPUT_SO): $(OUTPUT_A)
	$(LD) -shared --whole-archive -o $@ $<

$(SYSROOT)/usr/include/%: $(HEADER_SRC_DIR)/%
	cp $< $@

$(SYSROOT)/usr/lib/%.o: $(CRT_DIR)/%.asm
	nasm -f elf64 $< -o $@

# .o files require the .c or .asm source files
$(ARTIFACT_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(ARTIFACT_DIR)/%.o: $(MALLOC_SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(ARTIFACT_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@

clean:
	rm -rf $(ARTIFACT_DIR)
