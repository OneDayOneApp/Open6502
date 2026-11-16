#!/usr/bin/env python3
"""Compare NEMU and RTL per-instruction state traces.

Format (one line per retired instruction, hex):
    PC A X Y SP P

The 6502 B/U flag bits are not architectural, so P is compared with mask 0xCF.
This is the same contract sim/klaus/compare_trace.py already uses, with a
first-divergence report that names the instruction index and both states.

Exit code 0 = match, 1 = divergence, 2 = usage/length problem.
"""

import argparse
import sys

MASK = 0xCF


def load(path):
    rows = []
    with open(path) as f:
        for number, line in enumerate(f, 1):
            parts = line.split()
            if len(parts) < 6:
                continue
            try:
                rows.append(tuple(int(p, 16) for p in parts[:6]))
            except ValueError:
                raise SystemExit(f"difftrace: {path}:{number}: not hex state")
    return rows


def same(a, b):
    return a[:5] == b[:5] and (a[5] & MASK) == (b[5] & MASK)


def fmt(row):
    return "PC={:04x} A={:02x} X={:02x} Y={:02x} SP={:02x} P={:02x}".format(*row)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("nemu")
    ap.add_argument("rtl")
    ap.add_argument("--context", type=int, default=4)
    args = ap.parse_args()

    n, r = load(args.nemu), load(args.rtl)
    if not n or not r:
        print(f"difftrace: empty trace (nemu={len(n)} rtl={len(r)})", file=sys.stderr)
        return 2

    for i, (a, b) in enumerate(zip(n, r)):
        if not same(a, b):
            print(f"FAIL: first divergence at instruction {i}")
            print(f"  NEMU: {fmt(a)}")
            print(f"  RTL : {fmt(b)}")
            lo = max(0, i - args.context)
            hi = min(len(n), len(r), i + args.context)
            for j in range(lo, hi):
                mark = ">>>" if j == i else "   "
                print(f"{mark} {j:6d} NEMU[{fmt(n[j])}] RTL[{fmt(r[j])}]")
            return 1

    if len(n) != len(r):
        print(
            f"FAIL: trace length differs: NEMU retired {len(n)} instructions, "
            f"RTL retired {len(r)}"
        )
        longer, side = (n, "NEMU") if len(n) > len(r) else (r, "RTL")
        if len(r) < len(n):
            print(f"  RTL stopped at instruction {len(r)}: {fmt(longer[len(r)])} (NEMU side)")
        else:
            print(f"  NEMU stopped at instruction {len(n)}: {fmt(longer[len(n)])} (RTL side)")
        return 1

    print(f"PASS: {len(n)} retired instructions match (P mask 0x{MASK:02X})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
