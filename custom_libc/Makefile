CC = x86_64-elf-gcc
LD = x86_64-elf-ld

#where build artifacts are
ARTIFACT_DIR := ./.build
#compiler flags for building libc
CFLAGS = -ffreestanding -nostdlib -O2 -Wall -Wextra -fPIC -march=x86-64 -g -fno-omit-frame-pointer \
	-I include/ -I ../malloc_my_os_3/ -I ../abi


### Building the CRT* files

# where CRT* source is
CRT_DIR := ./crt
CRT_SRCS = $(wildcard $(CRT_DIR)/crt*.asm)
OUTPUT_CRTS = $(CRT_SRCS:$(CRT_DIR)/%.asm=$(ARTIFACT_DIR)/%.o)

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
OUTPUT_A = $(ARTIFACT_DIR)/libc_stuos.a
OUTPUT_SO = $(ARTIFACT_DIR)/libc_stuos.so

all: $(OUTPUT_A) $(OUTPUT_SO) $(OUTPUT_CRTS)

$(ARTIFACT_DIR)/crt%.o: $(CRT_DIR)/crt%.asm
	@mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@

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
	rm -rf $(ARTIFACT_DIR)
