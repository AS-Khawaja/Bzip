# Makefile – BZip2 Phase 1
CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -std=c11 -O2 -g
TARGET  = bzip2_phase2
SRCS    = main.c config.c block.c rle1.c bwt.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c bzip2.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Quick smoke-test
test: $(TARGET)
	python3 -c "import random,string; d='AAAA'*20+'BBBBBB'*10+'CD'*50+''.join(random.choices(string.ascii_letters,k=200)); open('test_input.txt','w').write(d); print('Generated',len(d),'bytes')"
	./$(TARGET) test_input.txt

clean:
	rm -f $(OBJS) $(TARGET) test_input.txt test_input.txt_phase2.bwt
