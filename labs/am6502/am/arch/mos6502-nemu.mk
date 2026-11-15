# am6502 arch: mos6502-nemu — flat 64 KiB, host I/O at $F001-$F003.
#
# The image is a 64 KiB memory image whose entry point is the reset vector at
# $FFFC.  Load it with:  6502EMU -b -a 0 -r image.bin
# (6502EMU defaults to $8000, which would put a 64 KiB image at the wrong
# address; the runner always passes -a 0.)

AM_PLATFORM   := nemu
AM_IMAGE_EXT  := bin
AM_LOAD_ADDR  := 0x0400
AM_SRC_DIR    := $(AM6502_HOME)/am/src/nemu
AM_SRCS       := crt0.s trm.s
AM_CFG        := $(AM_SRC_DIR)/flat.cfg

# Host I/O ports this platform implements (see docs/design.md §6.4).
AM_HOST_PUTC  := $F001
AM_HOST_EXIT  := $F002
