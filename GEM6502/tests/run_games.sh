#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)
GEM="$HERE/../gem6502"
OUT=${TMPDIR:-/tmp}/gem6502-games
mkdir -p "$OUT"
for rom in falling galaxypatrol hanoi table_tennis; do
  "$GEM" game "$ROOT/roms/$rom.nes" 60 "$OUT/$rom.ppm" >/dev/null
  python3 - "$OUT/$rom.ppm" "$rom" <<'PY'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
colors = im.getcolors(1 << 24) or []
black = sum(n for n, c in colors if c == (0, 0, 0))
print(f"{sys.argv[2]:14s} colors={len(colors):2d} black={black:5d}")
if black == 256 * 240:
    raise SystemExit("blank frame")
PY
done
echo "PASS: GEM6502 NROM game smoke"
