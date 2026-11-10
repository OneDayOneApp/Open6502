#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)
NEMU="$ROOT/NEMU/build/6502EMU"
IV=${IV:-iverilog}
python3 - "$HERE" <<'PY'
import sys
from pathlib import Path
here = Path(sys.argv[1])
mem = bytearray([0xEA] * 65536)
code = bytes([0xA9,0x42,0x8D,0x00,0x02,0xA2,0x03,0xE8,0x4C,0x08,0x80])
mem[0x8000:0x8000+len(code)] = code
mem[0xFFFC] = 0x00
mem[0xFFFD] = 0x80
(here / "difftest.hex").write_text("\n".join(f"{b:02x}" for b in mem) + "\n")
(here / "difftest.bin").write_bytes(code)
PY
cd "$HERE"
"$NEMU" -b --load-addr=0x8000 --pc=0x8000 --max-inst=5 --log="$HERE/nemu.log" \
  difftest.bin > nemu.out 2>&1 || true
"$IV" -g2012 -o rtl.vvp "$ROOT/rtl/cpu/open6502_cpu.sv" \
  "$ROOT/tb/open6502_cpu_difftest_tb.sv"
vvp rtl.vvp > rtl.out
nemu_line=$(grep '6502 A:' nemu.out | tail -1)
rtl_line=$(grep '^RTL ' rtl.out | tail -1)
echo "NEMU: $nemu_line"
echo "RTL : $rtl_line"
python3 - "$nemu_line" "$rtl_line" <<'PY'
import re, sys
n, r = sys.argv[1], sys.argv[2]
nm = re.search(r'A:([0-9A-F]{2}) X:([0-9A-F]{2}).*PC:([0-9A-F]{4})', n)
rm = re.search(r'A=([0-9A-F]{2}) X=([0-9A-F]{2}) PC=([0-9A-F]{4})', r)
if not nm or not rm:
    raise SystemExit("parse failure")
if nm.groups() != rm.groups():
    raise SystemExit(f"mismatch NEMU={nm.groups()} RTL={rm.groups()}")
print("PASS: NEMU/RTL final-state DiffTest", *rm.groups())
PY
