CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Iinclude
LDFLAGS ?=

CORE := src/gem6502.c src/ines.c src/render.c
MAIN := src/main.c
TEST := tests/test_gem6502.c
INES_TEST := tests/test_ines.c
RENDER_TEST := tests/test_render.c

all: gem6502

gem6502: $(CORE) $(MAIN) include/gem6502.h
	$(CC) $(CFLAGS) -o $@ $(CORE) $(MAIN) $(LDFLAGS)

test: tests/test_gem6502 tests/test_ines tests/test_render
	./tests/test_gem6502
	./tests/test_ines
	./tests/test_render

difftest: gem6502
	./tests/difftest.sh

tests/test_gem6502: $(CORE) $(TEST) include/gem6502.h
	$(CC) $(CFLAGS) -o $@ $(CORE) $(TEST) $(LDFLAGS)

tests/test_ines: src/ines.c $(INES_TEST) include/ines.h
	$(CC) $(CFLAGS) -o $@ src/ines.c $(INES_TEST) $(LDFLAGS)

tests/test_render: src/render.c src/gem6502.c $(RENDER_TEST) include/render.h
	$(CC) $(CFLAGS) -o $@ src/render.c src/gem6502.c $(RENDER_TEST) $(LDFLAGS)

clean:
	rm -f gem6502 tests/test_gem6502 tests/test_ines tests/test_render

.PHONY: all test difftest clean
