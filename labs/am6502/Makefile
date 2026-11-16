# am6502 — an Abstract-Machine-shaped build for 6502/NES programs.
#
#   make ARCH=mos6502-nemu APP=hello          build build/hello-mos6502-nemu.bin
#   make ARCH=... APP=... run [DUT=nemu]      build + run
#   make test                                 every app on every supported DUT
#   make clean
#
# ARCH comes from am/arch/<ARCH>.mk (the platform), the program from
# apps/<APP>/app.mk.  Nothing here touches NEMU/, GEM6502/ or rtl/.
#
# The design this implements: docs/design.md.  M1: mos6502-nemu (flat image).
# M2: mos6502-nes (iNES cartridge) on NEMU, GEM6502 and the RTL bench.

AM6502_HOME := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
OPEN6502_ROOT ?= $(abspath $(AM6502_HOME)/../..)

ARCH ?= mos6502-nemu
APP  ?= hello
DUT  ?= nemu

# External tests are opt-in (they download), mirroring NEMU/tests/run.sh.
RUN_KLAUS ?= 0
# mainargs is not wired up yet (M2).  Refuse it instead of ignoring it.
mainargs ?=
ifneq ($(mainargs),)
$(error am6502: mainargs= is not implemented yet (M2); see labs/am6502/README.md)
endif

AS   := ca65
LD   := ld65
PYTHON ?= python3
NEMU ?= $(OPEN6502_ROOT)/NEMU/build/6502EMU
GEM6502 ?= $(OPEN6502_ROOT)/GEM6502/gem6502
IVERILOG ?= iverilog
VVP ?= vvp

BUILD   := $(AM6502_HOME)/build/$(APP)-$(ARCH)
APP_DIR := $(AM6502_HOME)/apps/$(APP)

include $(AM6502_HOME)/am/arch/$(ARCH).mk
include $(AM6502_HOME)/apps/$(APP)/app.mk

# Which apps make sense on this platform (Klaus is a flat 64 KiB image).
APPS ?= $(AM_APPS)

.SUFFIXES:
.DELETE_ON_ERROR:
.DEFAULT_GOAL := all

IMAGE   := $(BUILD)/$(APP)-$(ARCH).$(AM_IMAGE_EXT)
SEGMENT := $(BUILD)/$(APP)-$(ARCH).seg
MAP     := $(BUILD)/$(APP)-$(ARCH).map

ASFLAGS := --cpu 6502 -I $(AM6502_HOME)/am -I $(AM6502_HOME)/am/include $(AM_ASFLAGS)

vpath %.s $(APP_DIR) $(AM_SRC_DIR)

all: $(IMAGE)

$(BUILD):
	@mkdir -p $@

$(BUILD)/%.o: %.s | $(BUILD)
	$(AS) $(ASFLAGS) -l $(BUILD)/$*.lst -o $@ $<

ifeq ($(APP_KIND),external)
OBJS :=

$(APP_ASSET):
	@mkdir -p $(dir $@)
	@echo "fetching $(APP_URL)"
	@curl -fsSL -o $@ $(APP_URL) || { rm -f $@; echo "am6502: download failed; set KLAUS_BIN=/path/to/6502_functional_test.bin" >&2; exit 1; }

$(IMAGE): $(APP_ASSET)
	$(PYTHON) $(AM6502_HOME)/tools/mkimage.py \
	    --input $(APP_ASSET) --set-vector $(APP_VECTOR) \
	    --out $@ --hex $(BUILD)/$(APP)-$(ARCH).hex --check
else
OBJS := $(addprefix $(BUILD)/,$(addsuffix .o,$(notdir $(basename $(APP_SRCS) $(AM_SRCS)))))

$(SEGMENT): $(OBJS) $(AM_CFG) | $(BUILD)
	$(LD) -C $(AM_CFG) -m $(MAP) -o $@ $(OBJS)

ifeq ($(AM_IMAGE_KIND),cartridge)
$(IMAGE): $(SEGMENT)
	$(PYTHON) $(AM6502_HOME)/tools/mkimage.py \
	    --prg $(SEGMENT) --chr-size $(AM_CHR_SIZE) --mapper $(AM_MAPPER) \
	    --mirroring $(AM_MIRRORING) --out $@ \
	    --hex-dir $(BUILD) --hex-prefix $(APP)-$(ARCH) --check
else
$(IMAGE): $(SEGMENT)
	$(PYTHON) $(AM6502_HOME)/tools/mkimage.py \
	    --segment $(SEGMENT) --load-addr $(AM_LOAD_ADDR) \
	    --out $@ --hex $(BUILD)/$(APP)-$(ARCH).hex --check
endif
endif

run: $(IMAGE)
	$(PYTHON) $(AM6502_HOME)/tools/run.py \
	    --arch $(ARCH) --app $(APP) --dut $(DUT) \
	    --image $(IMAGE) --check '$(APP_CHECK)' --max-inst $(APP_MAX_INST) \
	    --nemu $(NEMU) --gem $(GEM6502) --root $(OPEN6502_ROOT) \
	    --iverilog $(IVERILOG) --vvp $(VVP)

# Per-instruction lockstep comparison of the same image on NEMU and RTL.
difftest: $(IMAGE)
	$(PYTHON) $(AM6502_HOME)/tools/difftest.py \
	    --arch $(ARCH) --app $(APP) --image $(IMAGE) --check '$(APP_CHECK)' \
	    --max-inst $(APP_MAX_INST) --nemu $(NEMU) --root $(OPEN6502_ROOT) \
	    --iverilog $(IVERILOG) --vvp $(VVP)

test:
	@set -e; for app in $(APPS); do \
	  if [ "$$app" = klaus ] && [ "$(RUN_KLAUS)" != 1 ]; then \
	    echo "SKIP klaus [$(ARCH)]  (set RUN_KLAUS=1 to download and run)"; \
	    continue; \
	  fi; \
	  $(MAKE) --no-print-directory -f $(AM6502_HOME)/Makefile \
	      APP=$$app ARCH=$(ARCH) DUT=$(DUT) run; \
	done

clean:
	rm -rf $(AM6502_HOME)/build

help:
	@echo "am6502: make [ARCH=<arch>] [APP=<app>] [DUT=<dut>] <target>"
	@echo
	@echo "  ARCH  mos6502-nemu        flat 64 KiB image (M1)"
	@echo "        mos6502-nes         iNES cartridge (M2)"
	@echo "  APP   hello klaus         program, from apps/<APP>/app.mk"
	@echo "  DUT   nemu gem rtl        execution backend (gem/rtl for *.nes)"
	@echo
	@echo "targets: all (default), run, test, difftest, clean, help"
	@echo "vars:    RUN_KLAUS=1 enables the external Klaus test"
	@echo "         KLAUS_BIN=<path> uses a local copy instead of downloading"

.PHONY: all run test difftest clean help
