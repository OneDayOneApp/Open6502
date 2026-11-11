#!/usr/bin/env python3
import sys

def load(path):
    rows = []
    for line in open(path):
        p = line.split()
        if len(p) >= 12:
            rows.append(p)
    return rows

def main():
    if len(sys.argv) != 3:
        raise SystemExit(f"usage: {sys.argv[0]} nemu.trace gem.trace")
    n, g = load(sys.argv[1]), load(sys.argv[2])
    lim = min(len(n), len(g))
    for i in range(lim):
        a, b = n[i], g[i]
        same = (a[:5] == b[:5] and
                (int(a[5], 16) & 0xCF) == (int(b[5], 16) & 0xCF) and
                a[6:12] == b[6:12])
        if not same:
            print(f"FAIL: first divergence at line {i}")
            print("NEMU:", " ".join(a))
            print("GEM :", " ".join(b))
            for j in range(max(0, i - 3), min(lim, i + 4)):
                print(f"{j:4d} NEMU {' '.join(n[j])}")
                print(f"{j:4d} GEM  {' '.join(g[j])}")
            return 1
    print(f"PASS: {lim} instructions match (P mask 0xCF)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
