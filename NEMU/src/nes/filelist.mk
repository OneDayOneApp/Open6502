INC_PATH += $(NEMU_HOME)/src
INC_PATH += $(NEMU_HOME)/src/nes
DIRS-$(CONFIG_NES) += src/nes
ifdef CONFIG_NES
CFLAGS += $(shell sdl2-config --cflags)
LIBS += $(shell sdl2-config --libs)
endif
