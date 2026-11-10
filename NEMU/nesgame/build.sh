#!/bin/sh
# Build the bundled NROM game.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$HERE"
command -v ca65 >/dev/null 2>&1 || { echo "ca65 not found (install cc65)" >&2; exit 1; }
command -v ld65 >/dev/null 2>&1 || { echo "ld65 not found (install cc65)" >&2; exit 1; }
ca65 game.s -o game.o
ld65 -C game.cfg game.o -o game.prg
python3 chrgen.py game.chr
python3 combine.py game.prg game.chr game.nes
rm -f game.o game.prg game.chr
echo "built $HERE/game.nes"
