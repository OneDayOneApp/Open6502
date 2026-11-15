# am6502 arch: mos6502-nes — NROM cartridge, 2 KiB WRAM, PPU/APU present.
#
# The image is an iNES file (mapper 0) whose reset vector is part of the PRG,
# so it boots unmodified on 6502EMU, gem6502 and the RTL SoC.
# NEMU:   6502EMU --headless --frames=N --screenshot=... game.nes
# GEM:    gem6502 game game.nes N out.ppm --start
# RTL:    iverilog/verilator bench, PRG/CHR loaded from the derived .hex files

AM_PLATFORM   := nes
AM_IMAGE_KIND := cartridge
AM_IMAGE_EXT  := nes
AM_LOAD_ADDR  := 0x8000
AM_SRC_DIR    := $(AM6502_HOME)/am/src/nes
AM_SRCS       := crt0.s trm.s
AM_CFG        := $(AM_SRC_DIR)/nes.cfg
AM_ASFLAGS    := -D AM_PLATFORM_NES=1

# Cartridge shape (NROM-256: 32 KiB PRG, 8 KiB CHR-ROM, mapper 0).
AM_PRG_KB     := 32
AM_CHR_SIZE   := 8192
AM_MAPPER     := 0
AM_MIRRORING  := horizontal

# Klaus is a flat 64 KiB image; only the cartridge apps run here.
AM_APPS       := hello

# Host I/O ports this platform implements (design §6.4).
AM_HOST_PUTC  := $4018
AM_HOST_EXIT  := $4019
