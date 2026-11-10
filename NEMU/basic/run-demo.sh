#!/bin/sh
# Run the non-interactive BASIC demo and print the guest console output.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
EMU=${EMU:-"$ROOT/build/6502EMU"}
LOG=${TMPDIR:-/tmp}/6502emu-basic-demo.log

exec "$EMU" -b --load-addr=0x2000 --pc=0x4058 --max-inst=50000000 \
  --log="$LOG" "$HERE/msbasic.bin" < "$HERE/demo.in"
