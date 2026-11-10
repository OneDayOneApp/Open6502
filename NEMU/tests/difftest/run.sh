#!/bin/sh
# CPU-only NEMU DiffTest: REF shared object vs DUT executable.
# This rebuilds NEMU's config.  Run it explicitly, not from the default test loop.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
TMP=${TMPDIR:-/tmp}/6502emu-difftest
mkdir -p "$TMP"
cd "$ROOT"

make distclean >/dev/null 2>&1 || true
make m6502-nesref-so_defconfig >/dev/null
make -j8 >/dev/null
test -f build/6502EMU-so
cp build/6502EMU-so "$TMP/6502EMU-ref.so"

make distclean >/dev/null 2>&1 || true
make m6502-nes-dut_defconfig >/dev/null
make -j8 >/dev/null
test -x build/6502EMU

out=$("$ROOT/build/6502EMU" -b --diff="$TMP/6502EMU-ref.so" \
      --log="$TMP/diff.log" "$ROOT/programs/hello.bin" 2>&1)
printf '%s\n' "$out" | grep -q "Hello, 6502EMU!"
printf '%s\n' "$out" | grep -q "HIT GOOD TRAP"
# The REF must not print the guest's host I/O a second time.
test "$(printf '%s\n' "$out" | grep -c "Hello, 6502EMU!")" -eq 1
echo "PASS: NEMU CPU-only DiffTest"
