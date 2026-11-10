#!/bin/sh
# Start Microsoft BASIC 1.1 interactively.  Type BASIC lines (uppercase),
# then RUN.  Press Ctrl-D to leave BASIC and stop 6502EMU.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
EMU=${EMU:-"$ROOT/build/6502EMU"}
LOG=${TMPDIR:-/tmp}/6502emu-basic.log

test -x "$EMU" || { echo "build 6502EMU first: make -C $ROOT" >&2; exit 1; }
test -f "$HERE/msbasic.bin" || { echo "missing $HERE/msbasic.bin; run basic/build.sh" >&2; exit 1; }

exec "$EMU" -b --load-addr=0x2000 --pc=0x4058 --log="$LOG" "$HERE/msbasic.bin"
