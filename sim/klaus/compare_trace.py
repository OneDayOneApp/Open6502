#!/usr/bin/env python3
"""Compare NEMU and RTL CPU state traces.

Format: PC A X Y SP P (hex), one line per instruction.
The 6502 B/U bits are not architectural state, so P is compared with mask 0xCF.
"""
import sys

MASK = 0xCF


def load(path):
    rows = []
    with open(path) as f:
        for line in f:
            p = line.split()
            if len(p) == 6:
                rows.append((p[0].lower(),) + tuple(int(x, 16) for x in p[1:]))
    return rows


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f"usage: {sys.argv[0]} nemu.trace rtl.trace")
    n, r = load(sys.argv[1]), load(sys.argv[2])
    for i, (a, b) in enumerate(zip(n, r)):
        if a[:5] != b[:5] or (a[5] & MASK) != (b[5] & MASK):
            print(f"first divergence at line {i}")
            print(f"NEMU: {a}")
            print(f"RTL : {b}")
            for j in range(max(0, i - 4), min(len(n), len(r), i + 4)):
                print(f"{j:6d} NEMU={n[j]} RTL={r[j]}")
            return 1
    print(f"PASS: {min(len(n), len(r))} states match (P mask 0x{MASK:02X})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
