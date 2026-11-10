CA65 ?= ca65
LD65 ?= ld65
EMU  ?= ../build/6502EMU

SRCS := $(wildcard *.s)
BINS := $(SRCS:.s=.bin)

.PHONY: all clean test
all: $(BINS)

%.o: %.s
	$(CA65) $< -o $@

%.bin: %.o 6502.cfg
	$(LD65) -C 6502.cfg -o $@ $<

test: hello.bin
	$(EMU) -b --log=/tmp/6502emu-hello.log hello.bin

clean:
	rm -f *.o *.bin
