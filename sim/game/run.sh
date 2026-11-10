#!/bin/sh
# Build the Verilator SoC model and run a mapper-0 NROM game.
# Usage: ./sim/game/run.sh [game.nes] [output.ppm]
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)
ROM=${1:-"$ROOT/roms/falling.nes"}
OUT=${2:-/tmp/open6502-game.ppm}
ROMDIR=${ROMDIR:-/tmp/open6502-game-rom}
OBJ=${OBJ:-/tmp/open6502-game-obj}
LIBCXX_DIR=${LIBCXX_DIR:-/opt/homebrew/Cellar/llvm/21.1.8/lib/c++}
SDK=${SDK:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}

rm -rf "$ROMDIR" "$OBJ"
mkdir -p "$ROMDIR"
python3 "$HERE/extract_ines.py" "$ROM" "$ROMDIR" game

verilator --cc --exe -Wno-fatal --top-module open6502_top -Mdir "$OBJ" \
  "$ROOT/rtl/open6502_top.sv" "$ROOT/rtl/open6502_pkg.sv" \
  "$ROOT/rtl/cpu/open6502_cpu.sv" "$ROOT/rtl/bus/open6502_bus.sv" \
  "$ROOT/rtl/bus/open6502_wram.sv" "$ROOT/rtl/ppu/open6502_ppu.sv" \
  "$ROOT/rtl/apu/open6502_apu.sv" "$ROOT/rtl/apu/open6502_dmc.sv" \
  "$ROOT/rtl/cart/open6502_mapper.sv" "$ROOT/rtl/clocks/open6502_clocks.sv" \
  "$ROOT/rtl/video/open6502_video_timing.sv" \
  "$ROOT/rtl/input/open6502_controller.sv" \
  "$ROOT/rtl/dma/open6502_oam_dma.sv" \
  "$ROOT/rtl/dbg/open6502_difftest_bridge.sv" \
  "$HERE/main.cpp" >/tmp/open6502-game-verilator.log 2>&1

make -C "$OBJ" -f Vopen6502_top.mk CXX=clang++ LINK=clang++ \
  CXXFLAGS="-isysroot $SDK" \
  LDFLAGS="-isysroot $SDK -L$LIBCXX_DIR" \
  LDLIBS='-lc++' -j2

"$OBJ/Vopen6502_top" "$ROMDIR/game_prg.bin" "$ROMDIR/game_chr.bin" "$OUT"
echo "frame: $OUT"
