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
# LDA #$10; CLC; ADC #$05; PHA; LDA #0; PLA; STA $0200;
# LDX #2; DEX; BNE $800E; JMP $8011
code = bytes([
  0xA9,0x10, 0x18, 0x69,0x05, 0x48, 0xA9,0x00, 0x68,
  0x8D,0x00,0x02, 0xA2,0x02, 0xCA, 0xD0,0xFD, 0x4C,0x11,0x80])
mem[0x8000:0x8000+len(code)] = code
mem[0xFFFC] = 0x00
mem[0xFFFD] = 0x80
(here / "alu.hex").write_text("\n".join(f"{b:02x}" for b in mem) + "\n")
(here / "alu.bin").write_bytes(code)
PY
cd "$HERE"
"$NEMU" -b --load-addr=0x8000 --pc=0x8000 --max-inst=13 --log="$HERE/alu-nemu.log" \
  alu.bin > alu-nemu.out 2>&1 || true
"$IV" -g2012 -o rtl_alu.vvp "$ROOT/rtl/cpu/open6502_cpu.sv" \
  "$ROOT/tb/open6502_cpu_difftest_tb.sv"
cp alu.hex difftest.hex
vvp rtl_alu.vvp +STOP=8011 > alu-rtl.out
nemu_line=$(grep '6502 A:' alu-nemu.out | tail -1)
rtl_line=$(grep '^RTL ' alu-rtl.out | tail -1)
echo "NEMU: $nemu_line"
echo "RTL : $rtl_line"
python3 - "$nemu_line" "$rtl_line" <<'PY'
import re, sys
n, r = sys.argv[1], sys.argv[2]
nm = re.search(r'A:([0-9A-F]{2}) X:([0-9A-F]{2}) Y:([0-9A-F]{2}) SP:([0-9A-F]{2}) .*P:([0-9A-F]{2}).*PC:([0-9A-F]{4})', n, re.I)
rm = re.search(r'A=([0-9A-F]{2}) X=([0-9A-F]{2}) Y=([0-9A-F]{2}) SP=([0-9A-F]{2}) P=([0-9A-F]{2}) PC=([0-9A-F]{4})', r, re.I)
if not nm or not rm:
    raise SystemExit("parse failure")
ng = tuple(v.upper() for v in nm.groups())
rg = tuple(v.upper() for v in rm.groups())
if ng != rg:
    raise SystemExit(f"mismatch NEMU={ng} RTL={rg}")
print("PASS: NEMU/RTL ALU/stack DiffTest", *rg)
PY
