#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TMP=${TMPDIR:-/tmp}/6502emu-tests
mkdir -p "$TMP"
make -C "$HERE" clean >/dev/null
make -C "$HERE"

cd "$HERE"

if [ "${RUN_KLAUS:-0}" = "1" ]; then
  KLAUS=${KLAUS_BIN:-$TMP/6502_functional_test.bin}
  if [ ! -f "$KLAUS" ]; then
    echo "Downloading Klaus 6502 functional test for REF..."
    curl -L --fail -o "$KLAUS" \
      https://raw.githubusercontent.com/Klaus2m5/6502_65C02_functional_tests/master/bin_files/6502_functional_test.bin
  fi
  NESREF_KLAUS="$KLAUS" "$HERE/test_nes_ref"
else
  "$HERE/test_nes_ref"
fi
