CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror
ROOT := ../..
CPPFLAGS += -I$(ROOT)/include -I$(ROOT)/src -I$(ROOT)/src/nes

SRCS := test_ppu.c stubs.c $(ROOT)/src/nes/ppu.c

test_ppu: $(SRCS) $(ROOT)/src/nes/nes.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SRCS)

clean:
	rm -f test_ppu
.PHONY: clean
