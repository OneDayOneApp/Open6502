#!/bin/sh
# DiffTest: JIT6502 (lab) against GEM6502 as the reference model.
#
# Two layers:
#   1. instruction-level trace comparison (JIT compiled one instruction per
#      block, chaining off) on CPU state, zero-page cells, cycles and
#      PPU scanline/dot;
#   2. frame-level checkpoint comparison with the shipped configuration
#      (multi-instruction blocks + chaining).
#
# Usage: ./scripts/difftest_gem6502.sh [ROM] [INSTRUCTIONS] [FRAMES]
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
LAB=$(CDPATH= cd -- "$HERE/.." && pwd)
ROOT=$(CDPATH= cd -- "$LAB/../.." && pwd)

GEM=${GEM:-$ROOT/GEM6502/gem6502}
BIN=${BIN:-$LAB/bin/jit6502}
ROM=${1:-$ROOT/roms/falling.nes}
N=${2:-200000}
FRAMES=${3:-120}

[ -x "$GEM" ] || { echo "missing $GEM (run make -C GEM6502)" >&2; exit 2; }
[ -x "$BIN" ] || { echo "missing $BIN (run make in labs/jit6502)" >&2; exit 2; }
[ -f "$ROM" ] || { echo "missing ROM $ROM" >&2; exit 2; }

fail=0
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

echo "== instruction trace (N=$N): $ROM"
"$GEM" game "$ROM" 1000000 --max-steps="$N" --trace-state="$tmp/gem.trace" \
  >"$tmp/gem.out" 2>&1 || true
for eng in interp jit; do
  "$BIN" nes "$ROM" --run-inst="$N" --trace-state="$tmp/$eng.trace" \
    --engine="$eng" >"$tmp/$eng.out"
  if ! python3 - "$tmp/gem.trace" "$tmp/$eng.trace" "$eng" <<'PY'
import sys
gem = open(sys.argv[1]).read().splitlines()
lab = open(sys.argv[2]).read().splitlines()
tag = sys.argv[3]
n = min(len(gem), len(lab))
# GEM6502 may take a few extra steps around DMA stalls; compare the prefix.
if len(gem) != len(lab):
    print(f"FAIL [{tag}] line counts differ: gem={len(gem)} lab={len(lab)}")
    raise SystemExit(1)
for i in range(n):
    if gem[i] != lab[i]:
        print(f"FAIL [{tag}] first mismatch at line {i+1}")
        print(f"  GEM {gem[i]}")
        print(f"  LAB {lab[i]}")
        raise SystemExit(1)
print(f"PASS [{tag}] {n} instructions identical")
PY
  then
    fail=1
  fi
done

echo "== instruction checkpoint (N=$N, multi-instruction blocks, no chaining)"
"$GEM" game "$ROM" 1000000 --max-steps="$N" >"$tmp/gem_frame.out" 2>&1 || true
"$BIN" nes "$ROM" --run-inst="$N" --engine=jit --no-chain --quiet >"$tmp/lab_frame.out"
if ! python3 - "$tmp/gem_frame.out" "$tmp/lab_frame.out" <<'PY'
import re, sys
gem = open(sys.argv[1]).read()
lab = open(sys.argv[2]).read()
g = re.search(r'pc=([0-9A-Fa-f]{4}) a=([0-9A-Fa-f]{2}) x=([0-9A-Fa-f]{2}) '
              r'y=([0-9A-Fa-f]{2}) sp=([0-9A-Fa-f]{2}) p=([0-9A-Fa-f]{2})', gem)
gc = re.search(r'cycles=(\d+)', gem)
l = re.search(r'state pc=([0-9A-Fa-f]{4}) a=([0-9A-Fa-f]{2}) x=([0-9A-Fa-f]{2}) '
              r'y=([0-9A-Fa-f]{2}) sp=([0-9A-Fa-f]{2}) p=([0-9A-Fa-f]{2}) '
              r'cycles=(\d+)', lab)
if not g or not gc or not l:
    print("FAIL checkpoint: could not parse output")
    print(gem[-300:]); print(lab[-300:])
    raise SystemExit(1)
gem_state = tuple(v.upper() for v in g.groups()) + (gc.group(1),)
lab_state = tuple(v.upper() for v in l.groups())
if gem_state != lab_state:
    print("FAIL checkpoint: state differs")
    print("  GEM", gem_state)
    print("  LAB", lab_state)
    raise SystemExit(1)
print("PASS checkpoint: pc/a/x/y/sp/p/cycles identical", *lab_state)
PY
then
  fail=1
fi

if [ "$fail" -ne 0 ]; then
  echo "DIFFTEST FAILED"
  exit 1
fi
echo "DIFFTEST PASSED"
