#!/usr/bin/env python3
"""Inspect and extract iNES cartridge images.

`mkimage.py` builds our own cartridges; this tool is for images that already
exist (bundled ROMs, downloaded test ROMs) and is the one place that knows the
container layout, replacing the hard-coded /tmp/falling paths.

  ines.py check   game.nes
  ines.py extract game.nes --out-dir DIR --prefix NAME

Extraction writes PRG/CHR as .hex (one byte per line, for $readmemh) and .bin.
"""

import argparse
import sys
from pathlib import Path

MAGIC = b"NES\x1a"


def fail(msg):
    print(f"ines: error: {msg}", file=sys.stderr)
    raise SystemExit(1)


def parse(path):
    data = Path(path).read_bytes()
    if len(data) < 16 or data[:4] != MAGIC:
        fail(f"{path} is not an iNES file")
    prg_units, chr_units = data[4], data[5]
    if prg_units == 0:
        fail(f"{path}: header claims zero PRG banks")
    mapper = (data[7] & 0xF0) | (data[6] >> 4)
    mirroring = "vertical" if data[6] & 1 else "horizontal"
    offset = 16
    if data[6] & 0x04:  # trainer
        offset += 512
    prg = data[offset:offset + prg_units * 16384]
    offset += prg_units * 16384
    chr_data = data[offset:offset + chr_units * 8192]
    if len(prg) != prg_units * 16384:
        fail(f"{path}: truncated PRG")
    if len(chr_data) != chr_units * 8192:
        fail(f"{path}: truncated CHR")
    if mapper == 0 and prg:
        vec = prg[-6] | (prg[-5] << 8)
        if not 0x8000 <= vec <= 0xFFFF:
            fail(f"{path}: reset vector ${vec:04X} is outside PRG space")
    return {
        "prg": prg,
        "chr": chr_data,
        "mapper": mapper,
        "mirroring": mirroring,
        "prg_size": len(prg),
        "chr_size": len(chr_data),
    }


def write_hex(data, path):
    path.write_text("\n".join(f"{b:02x}" for b in data) + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("mode", choices=("check", "extract"))
    ap.add_argument("rom")
    ap.add_argument("--out-dir", default=".")
    ap.add_argument("--prefix", help="output basename (default: ROM stem)")
    args = ap.parse_args()

    info = parse(args.rom)
    print(
        f"{args.rom}: mapper={info['mapper']} mirroring={info['mirroring']} "
        f"prg={info['prg_size']} chr={info['chr_size']}"
    )
    if args.mode == "check":
        return

    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    prefix = args.prefix or Path(args.rom).stem
    write_hex(info["prg"], out / f"{prefix}.prg.hex")
    (out / f"{prefix}.prg.bin").write_bytes(info["prg"])
    if info["chr_size"]:
        write_hex(info["chr"], out / f"{prefix}.chr.hex")
        (out / f"{prefix}.chr.bin").write_bytes(info["chr"])
    else:
        (out / f"{prefix}.chr.hex").write_text("")
    print(f"extracted to {out}/{prefix}.{{prg,chr}}.{{hex,bin}}")


if __name__ == "__main__":
    main()
