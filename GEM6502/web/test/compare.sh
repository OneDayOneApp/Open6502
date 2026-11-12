#!/usr/bin/env bash
# Compare the WebAssembly renderer against the native build, byte for byte.
#
#   ./test/compare.sh [rom] [frames]
#
# Defaults to ../roms/falling.nes after 60 frames.  Both runs use the same
# core sources and the same gem_render_frame() path, so any difference means
# the wasm build, the memory loader or the bridge changed behaviour.
set -euo pipefail

cd "$(dirname "$0")/../.."   # GEM6502/

ROM=${1:-../roms/falling.nes}
FRAMES=${2:-60}
OUT_WASM=$(mktemp -t gem6502-wasm).ppm
OUT_NATIVE=$(mktemp -t gem6502-native).ppm
trap 'rm -f "$OUT_WASM" "$OUT_NATIVE"' EXIT

make -s gem6502
node web/test/run-node.mjs "$ROM" "$FRAMES" "$OUT_WASM" >/dev/null
./gem6502 game "$ROM" "$FRAMES" "$OUT_NATIVE" >/dev/null

if cmp -s "$OUT_WASM" "$OUT_NATIVE"; then
  echo "PASS: $ROM frame $FRAMES is byte-identical (wasm == native)"
else
  echo "FAIL: $ROM frame $FRAMES differs" >&2
  cmp "$OUT_WASM" "$OUT_NATIVE" >&2 || true
  exit 1
fi
