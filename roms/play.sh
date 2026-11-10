#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
EMU=${EMU:-"$ROOT/NEMU/build/6502EMU"}
name=${1:-falling}
case "$name" in
  *.nes) rom=$name ;;
  *) rom="$HERE/$name.nes" ;;
esac
if [ ! -x "$EMU" ]; then
  echo "build the emulator first: make -C $ROOT/NEMU m6502_defconfig && make -C $ROOT/NEMU -j8" >&2
  exit 1
fi
if [ ! -f "$rom" ]; then
  echo "unknown ROM: $name" >&2
  exit 1
fi
exec "$EMU" "$rom"
