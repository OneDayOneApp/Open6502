#!/bin/sh
# Build and run the RTL Klaus test with Verilator.
# Usage: KLAUS_BIN=/path/klaus.bin ./sim/klaus/run.sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)
KLAUS_BIN=${KLAUS_BIN:-/tmp/klaus.bin}
LIBCXX_DIR=${LIBCXX_DIR:-/opt/homebrew/Cellar/llvm/21.1.8/lib/c++}
SDK=${SDK:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
BUILD=${BUILD:-/tmp/open6502-klaus}

test -f "$KLAUS_BIN" || { echo "missing Klaus binary: $KLAUS_BIN" >&2; exit 1; }
rm -rf "$BUILD"
verilator --cc --exe -Wno-fatal --top-module open6502_cpu \
  -Mdir "$BUILD" "$ROOT/rtl/cpu/open6502_cpu.sv" "$HERE/main.cpp"
make -C "$BUILD" -f Vopen6502_cpu.mk \
  CXX=clang++ LINK=clang++ \
  CXXFLAGS="-isysroot $SDK" \
  LDFLAGS="-isysroot $SDK -L$LIBCXX_DIR" \
  LDLIBS='-lc++' -j2
"$BUILD/Vopen6502_cpu" "$KLAUS_BIN"
