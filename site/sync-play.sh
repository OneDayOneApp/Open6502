#!/usr/bin/env bash
# Rebuild the GEM6502 WebAssembly core and refresh the in-page demo assets.
#
# The canonical player lives in GEM6502/web/. The site keeps a copy so GitHub
# Pages can serve it without a build step; run this after changing the core,
# the player or the bundled ROMs.
set -euo pipefail

cd "$(dirname "$0")"
ROOT=$(cd .. && pwd)

"$ROOT/GEM6502/web/build.sh"

mkdir -p assets/play/roms
cp "$ROOT/GEM6502/web/player.js" assets/play/player.js
cp "$ROOT/GEM6502/web/gem6502.wasm" assets/play/gem6502.wasm
cp "$ROOT"/roms/*.nes assets/play/roms/
chmod 644 assets/play/player.js assets/play/gem6502.wasm assets/play/roms/*.nes

echo "synced site/assets/play:"
ls -lh assets/play/player.js assets/play/gem6502.wasm
