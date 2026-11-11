CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Iinclude
LDFLAGS ?=

CORE := src/gem6502.c
MAIN := src/main.c
TEST := tests/test_gem6502.c

all: gem6502

gem6502: $(CORE) $(MAIN) include/gem6502.h
	$(CC) $(CFLAGS) -o $@ $(CORE) $(MAIN) $(LDFLAGS)

test: tests/test_gem6502
	./tests/test_gem6502

tests/test_gem6502: $(CORE) $(TEST) include/gem6502.h
	$(CC) $(CFLAGS) -o $@ $(CORE) $(TEST) $(LDFLAGS)

clean:
	rm -f gem6502 tests/test_gem6502

.PHONY: all test clean
