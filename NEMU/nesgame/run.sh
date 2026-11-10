#!/bin/sh
# Run the bundled NROM demo game.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
EMU=${EMU:-"$ROOT/build/6502EMU"}
test -x "$EMU" || { echo "build 6502EMU first: make -C $ROOT" >&2; exit 1; }
test -f "$HERE/game.nes" || { echo "missing game.nes; run nesgame/build.sh" >&2; exit 1; }
exec "$EMU" --scale=3 "$HERE/game.nes"
