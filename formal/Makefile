# Open6502 formal model.
#
# `make` type-checks the whole Lean development and kernel-checks every proof;
# `make check-rtl` additionally proves that the emitted decode table agrees with
# the hand-written dispatch in rtl/cpu/open6502_cpu.sv.

LAKE ?= lake
PYTHON ?= python3

.PHONY: all build extract check-rtl check-cosim clean

all: build check-rtl check-cosim

build:
	$(LAKE) build

extract: build
	$(LAKE) exe open6502-extract build

check-rtl: extract
	$(PYTHON) scripts/check_rtl_sync.py \
	  --rtl ../rtl/cpu/open6502_cpu.sv --json build/decode.json

# Co-simulate the Lean ISA model against the SystemVerilog RTL (needs iverilog).
check-cosim: build
	$(PYTHON) scripts/cosim.py

clean:
	rm -rf .lake build
