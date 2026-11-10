#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
make -C "$HERE" clean >/dev/null
make -C "$HERE"
"$HERE/test_ppu"
