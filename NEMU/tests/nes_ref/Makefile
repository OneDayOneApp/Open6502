CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror
ROOT := ../..
CPPFLAGS += -I$(ROOT)/src

SRCS := test_nes_ref.c \
        $(ROOT)/src/nes/ref/clock.c \
        $(ROOT)/src/nes/ref/rom.c \
        $(ROOT)/src/nes/ref/state.c \
        $(ROOT)/src/nes/ref/machine.c \
        $(ROOT)/src/nes/ref/cpu.c \
        $(ROOT)/src/nes/ref/m6502_table.c

test_nes_ref: $(SRCS) $(ROOT)/src/nes/ref/nes_ref.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SRCS)

clean:
	rm -f test_nes_ref

.PHONY: clean
