CC = x86_64-elf-gcc
LD = x86_64-elf-ld

LIBC = ../lib/libc.a
LIBC_HEADERS = ../include/

CFLAGS = -ffreestanding -nostdlib -O2 -Wall -Wextra -mno-red-zone -march=x86-64 -I $(LIBC_HEADERS) -I ../../abi/
LDFLAGS = -nostdlib

SRCS = fuzz.c $(wildcard *_test.c)
OBJS := $(SRCS:.c=.o)

all: testing.out other.out

other.out: other.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIBC) -o $@

testing.out: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBC)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) testing.out
