#!/bin/sh
# GEM6502 vs NEMU state-trace regression over directed 6502 programs.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)
GEM="$HERE/../gem6502"
NEMU="$ROOT/NEMU/build/6502EMU"
OUT=${TMPDIR:-/tmp}/gem6502-diff
mkdir -p "$OUT"

test -x "$GEM" || { echo "missing $GEM; run make" >&2; exit 1; }
test -x "$NEMU" || { echo "missing $NEMU; build NEMU first" >&2; exit 1; }

for case in alu decimal stack branch rmw brk; do
  python3 "$HERE/gen_case.py" "$OUT/$case.bin" "$case"
  NEMU_TRACE_STATE="$OUT/$case.nemu" NEMU_TRACE_MEM=1 \
    "$NEMU" -b --load-addr=0x8000 --pc=0x8000 --max-inst=80 \
    "$OUT/$case.bin" >"$OUT/$case.nemu.out" 2>&1 || true
  "$GEM" run "$OUT/$case.bin" 80 --trace-state="$OUT/$case.gem" \
    >"$OUT/$case.gem.out" 2>&1
  printf 'case %-8s ' "$case"
  python3 "$HERE/compare_trace.py" "$OUT/$case.nemu" "$OUT/$case.gem"
done
