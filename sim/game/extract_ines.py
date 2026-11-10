#!/usr/bin/env python3
"""Extract NROM PRG/CHR from an iNES file into $readmemh images."""
import sys
from pathlib import Path

def main():
    if len(sys.argv) != 4:
        raise SystemExit(f"usage: {sys.argv[0]} game.nes outdir prefix")
    data = Path(sys.argv[1]).read_bytes()
    if data[:4] != b"NES\x1a":
        raise SystemExit("not an iNES file")
    prg_banks, chr_banks = data[4], data[5]
    if data[6] & 0x0f != 0 or data[7] & 0xf0 != 0:
        raise SystemExit("only mapper 0 is supported here")
    off = 16
    prg = data[off:off + prg_banks * 16384]
    off += prg_banks * 16384
    if data[6] & 0x04:
        off += 512
    chr = data[off:off + chr_banks * 8192]
    out = Path(sys.argv[2]); out.mkdir(parents=True, exist_ok=True)
    (out / f"{sys.argv[3]}_prg.hex").write_text("\n".join(f"{b:02x}" for b in prg) + "\n")
    (out / f"{sys.argv[3]}_chr.hex").write_text("\n".join(f"{b:02x}" for b in chr) + "\n")
    (out / f"{sys.argv[3]}.size").write_text(f"{len(prg)} {len(chr)}\n")
    print(f"PRG={len(prg)} CHR={len(chr)}")

if __name__ == "__main__":
    main()
