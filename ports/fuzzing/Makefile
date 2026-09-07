CC = ../../x86_64-tcc
LD = $(CC)

FLAGS = -O2 -static

SRCS = fuzz.c $(wildcard *_test.c)
OBJS := $(SRCS:.c=.o)

SYSROOT = ../../sysroot
TESTING_OUT = $(SYSROOT)/testing.out
OTHER_OUT = $(SYSROOT)/other.out

all: $(TESTING_OUT) $(OTHER_OUT)

$(OTHER_OUT): other.c
	$(CC) $(FLAGS) $< -o $@

$(TESTING_OUT): $(OBJS)
	$(LD) $(FLAGS) $^ -o $@

%.o: %.c
	$(CC) $(FLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) $(TESTING_OUT) $(OTHER_OUT)
