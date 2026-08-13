CC = x86_64-elf-gcc
LD = x86_64-elf-ld

#TODO sysroot
LIBC = /home/stuart/Programming/stuos/sysroot/lib/tcc/libc.a /home/stuart/Programming/stuos/sysroot/lib/tcc/crt0.o
LIBC_HEADERS = /home/stuart/Programming/stuos/sysroot/lib/tcc/include

CFLAGS = -ffreestanding -nostdlib -O2 -Wall -Wextra -mno-red-zone -march=x86-64 -I $(LIBC_HEADERS)
LDFLAGS = -nostdlib

SRCS = fuzz.c $(wildcard *_test.c)
OBJS := $(SRCS:.c=.o)

#TODO make the root makefile copy this
OUTPUT_DIR = ../../sysroot
TESTING_OUT = $(OUTPUT_DIR)/testing.out
OTHER_OUT = $(OUTPUT_DIR)/other.out

all: $(TESTING_OUT) $(OTHER_OUT)

$(OTHER_OUT): other.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIBC) -o $@

$(TESTING_OUT): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBC)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) $(TESTING_OUT) $(OTHER_OUT)
