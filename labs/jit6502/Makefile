# 6502 -> AArch64 JIT lab.
#
# Everything here is self-contained: no NEMU, no RTL, no SDL.  Run `make`,
# then `./bin/jit6502 klaus /tmp/klaus.bin` (or `make test`).

CC ?= clang
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Wno-unused-parameter
CFLAGS += -Isrc -Ines

# Optional SDL2 window (the headless lab works without it).
SDL2_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL2_LIBS := $(shell sdl2-config --libs 2>/dev/null)
ifneq ($(strip $(SDL2_LIBS)),)
CFLAGS += -DHAVE_SDL2 $(SDL2_CFLAGS)
LDLIBS += $(SDL2_LIBS)
endif
LDFLAGS ?=
LDLIBS ?=

BIN := bin/jit6502
SRCS := $(wildcard src/*.c) $(wildcard nes/*.c)
HDRS := $(wildcard src/*.h) $(wildcard nes/*.h)

.PHONY: all clean test klaus bench

all: $(BIN)

$(BIN): $(SRCS) $(HDRS)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS) $(LDLIBS)

KLAUS ?= /tmp/klaus.bin

.PHONY: klaus bench test selfcheck enccheck difftest

klaus: $(BIN)
	./$(BIN) klaus $(KLAUS) --engine=jit --chain --stats

# Generate the benchmark images (Python, no dependencies).
bench: $(BIN)
	python3 tools/make_bench.py bench

test: $(BIN) bench
	KLAUS=$(KLAUS) ./scripts/compare_engines.sh

difftest: $(BIN)
	./scripts/difftest_gem6502.sh

selfcheck: $(BIN)
	./$(BIN) selfcheck 20000

# Verify the AArch64 encoder against llvm-mc's reference encodings.
enccheck:
	cc -O1 -I src -o /tmp/enccheck tools/enccheck.c && /tmp/enccheck

clean:
	rm -rf bin
