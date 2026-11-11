CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Iinclude
LDFLAGS ?=

CORE := src/gem6502.c src/ines.c src/render.c src/cartridge.c src/audio.c
MAIN := src/main.c src/frontend.c
SDL_CONFIG ?= sdl2-config
SDL_CFLAGS := $(shell $(SDL_CONFIG) --cflags 2>/dev/null)
SDL_LIBS := $(shell $(SDL_CONFIG) --libs 2>/dev/null)
ifneq ($(strip $(SDL_LIBS)),)
UI_FLAGS := -DGEM_HAVE_SDL $(SDL_CFLAGS)
endif
TEST := tests/test_gem6502.c
INES_TEST := tests/test_ines.c
RENDER_TEST := tests/test_render.c

all: gem6502

gem6502: $(CORE) $(MAIN) include/gem6502.h include/audio.h include/frontend.h
	$(CC) $(CFLAGS) $(UI_FLAGS) -o $@ $(CORE) $(MAIN) $(LDFLAGS) $(SDL_LIBS)

test: tests/test_gem6502 tests/test_ines tests/test_render tests/test_audio
	./tests/test_gem6502
	./tests/test_ines
	./tests/test_render
	./tests/test_audio

difftest: gem6502
	./tests/difftest.sh

mmc3: gem6502
	python3 -u tests/mmc3_regression.py

games: gem6502
	./tests/run_games.sh

tests/test_gem6502: $(CORE) $(TEST) include/gem6502.h include/audio.h
	$(CC) $(CFLAGS) -o $@ $(CORE) $(TEST) $(LDFLAGS)

tests/test_ines: src/ines.c $(INES_TEST) include/ines.h
	$(CC) $(CFLAGS) -o $@ src/ines.c $(INES_TEST) $(LDFLAGS)

tests/test_render: src/render.c src/gem6502.c src/audio.c $(RENDER_TEST) include/render.h
	$(CC) $(CFLAGS) -o $@ src/render.c src/gem6502.c src/audio.c $(RENDER_TEST) $(LDFLAGS)

tests/test_audio: $(CORE) tests/test_audio.c include/audio.h include/gem6502.h
	$(CC) $(CFLAGS) -o $@ $(CORE) tests/test_audio.c $(LDFLAGS)

clean:
	rm -f gem6502 tests/test_gem6502 tests/test_ines tests/test_render tests/test_frontend tests/test_audio tests/test_kernel

.PHONY: all test difftest games mmc3 clean

ifneq ($(strip $(SDL_LIBS)),)
test-ui: tests/test_frontend
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./tests/test_frontend

tests/test_frontend: $(CORE) src/frontend.c tests/test_frontend.c include/frontend.h include/gem6502.h include/audio.h
	$(CC) $(CFLAGS) $(UI_FLAGS) -o $@ $(CORE) tests/test_frontend.c $(LDFLAGS) $(SDL_LIBS)
else
test-ui:
	@echo "SDL2 is required for test-ui" >&2; exit 1
endif
.PHONY: test-ui

# Experimental timing kernel; independent of the legacy execution path.
test-kernel: tests/test_kernel
	./tests/test_kernel

tests/test_kernel: src/sim/kernel.c tests/test_kernel.c include/gem/kernel.h
	$(CC) $(CFLAGS) -Werror -o $@ src/sim/kernel.c tests/test_kernel.c $(LDFLAGS)

.PHONY: test-kernel
