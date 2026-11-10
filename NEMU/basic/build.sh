#!/bin/sh
# Rebuild the bundled Microsoft BASIC 1.1 image for 6502EMU.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$HERE/msbasic"

command -v ca65 >/dev/null 2>&1 || { echo "ca65 not found (install cc65)" >&2; exit 1; }
command -v ld65 >/dev/null 2>&1 || { echo "ld65 not found (install cc65)" >&2; exit 1; }

ca65 -D kb9 msbasic.s -o msbasic.o
ld65 -C kb9.cfg msbasic.o -o "$HERE/msbasic.bin" -Ln msbasic.lbl
rm -f msbasic.o msbasic.lbl
echo "built $HERE/msbasic.bin"
