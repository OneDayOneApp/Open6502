#!/usr/bin/env python3
"""Assemble and check am6502 images.

Three producers, one contract (see labs/am6502/docs/design.md §4):

  flat       an ld65 segment laid out at --load-addr, padded down to $0000 and
             up to $FFFF: the 65,536-byte memory image mos6502-nemu runs
  cartridge  a 32 KiB PRG segment plus CHR, wrapped in an iNES header (NROM):
             the .nes that NEMU, GEM6502 and the RTL bench all boot
  external   a prebuilt binary (Klaus) that only needs its reset vector filled

Every image is verified: right size, valid container, and a reset vector that
points inside the program.  Hex output is one byte per line, for $readmemh.
"""

import argparse
import hashlib
import sys
from pathlib import Path

IMAGE_SIZE = 0x10000
RESET_VECTOR = 0xFFFC
NES_MAGIC = b"NES\x1a"


def fail(msg):
    print(f"mkimage: error: {msg}", file=sys.stderr)
    raise SystemExit(1)


def parse_int(text):
    try:
        return int(text, 0)
    except ValueError:
        fail(f"not a number: {text!r}")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def write_hex(data, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(f"{b:02x}" for b in data) + "\n")


# ---------------------------------------------------------------- flat image

def build_flat(args):
    seg = Path(args.segment).read_bytes()
    load_addr = parse_int(args.load_addr)
    expected = IMAGE_SIZE - load_addr
    if len(seg) != expected:
        fail(
            f"{args.segment} is {len(seg)} bytes, expected {expected} "
            f"(load address ${load_addr:04X} to $FFFF); check the ld65 config"
        )
    data = bytes(load_addr) + seg
    vec = data[RESET_VECTOR] | (data[RESET_VECTOR + 1] << 8)
    if args.check:
        if not load_addr <= vec < IMAGE_SIZE:
            fail(f"reset vector ${vec:04X} is outside the image; did VECTORS move?")
        if data[0xFFFA:0x10000] == b"\x00" * 6:
            fail("NMI/RESET/IRQ vectors are all zero")
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(data)
    if args.hex:
        write_hex(data, Path(args.hex))
    print(f"image: {out} flat size={len(data)} reset=${vec:04X} sha256={sha256(data)}")


# ----------------------------------------------------------- cartridge image

def build_cartridge(args):
    prg = Path(args.prg).read_bytes()
    if not prg or len(prg) % 16384:
        fail(f"{args.prg} is {len(prg)} bytes, expected a multiple of 16 KiB")
    if args.chr:
        chr_data = Path(args.chr).read_bytes()
    elif args.chr_size:
        chr_data = bytes(parse_int(args.chr_size))
    else:
        chr_data = b""
    if len(chr_data) % 8192:
        fail(f"CHR is {len(chr_data)} bytes, expected a multiple of 8 KiB")

    mapper = parse_int(args.mapper)
    if not 0 <= mapper <= 255:
        fail(f"mapper {mapper} does not fit in the iNES header")
    mirror_bit = 1 if args.mirroring == "vertical" else 0

    header = bytearray(16)
    header[:4] = NES_MAGIC
    header[4] = len(prg) // 16384
    header[5] = len(chr_data) // 8192
    header[6] = ((mapper & 0x0F) << 4) | mirror_bit
    header[7] = mapper & 0xF0
    data = bytes(header) + prg + chr_data

    # The PRG ends in the 6502 vector table: NMI, RESET, IRQ.
    vec = prg[-6] | (prg[-5] << 8)
    if args.check:
        if prg[-6:-4] == b"\x00\x00":
            fail("reset vector is zero; the VECTORS segment is missing")
        if not 0x8000 <= vec <= 0xFFFF:
            fail(f"reset vector ${vec:04X} is not in PRG space")

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(data)
    if args.hex_dir:
        prefix = args.hex_prefix or out.stem
        base = Path(args.hex_dir)
        write_hex(prg, base / f"{prefix}.prg.hex")
        write_hex(chr_data, base / f"{prefix}.chr.hex")
    print(
        f"image: {out} iNES mapper={mapper} prg={len(prg)} chr={len(chr_data)} "
        f"reset=${vec:04X} sha256={sha256(data)}"
    )


# ------------------------------------------------------------ external image

def build_external(args):
    raw = Path(args.input).read_bytes()
    if len(raw) > IMAGE_SIZE:
        fail(f"{args.input} is {len(raw)} bytes, larger than one 64 KiB image")
    data = bytearray(raw) + bytes(IMAGE_SIZE - len(raw))
    if args.set_vector:
        vec = parse_int(args.set_vector)
        if not 0 <= vec < IMAGE_SIZE:
            fail(f"--set-vector ${vec:04X} is outside $0000-$FFFF")
        data[RESET_VECTOR] = vec & 0xFF
        data[RESET_VECTOR + 1] = vec >> 8
    if args.check and (data[RESET_VECTOR] == 0 and data[RESET_VECTOR + 1] == 0):
        fail("reset vector is zero after --set-vector")
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(bytes(data))
    if args.hex:
        write_hex(bytes(data), Path(args.hex))
    vec = data[RESET_VECTOR] | (data[RESET_VECTOR + 1] << 8)
    print(f"image: {out} flat size={len(data)} reset=${vec:04X} sha256={sha256(bytes(data))}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--segment", help="ld65 segment for a flat image")
    src.add_argument("--prg", help="ld65 PRG segment for a cartridge image")
    src.add_argument("--input", help="prebuilt binary to pad into a 64 KiB image")
    ap.add_argument("--load-addr", default="0x0400", help="flat image load address")
    ap.add_argument("--set-vector", help="external: overwrite $FFFC")
    ap.add_argument("--chr", help="cartridge: CHR-ROM file")
    ap.add_argument("--chr-size", help="cartridge: zero-filled CHR size")
    ap.add_argument("--mapper", default="0", help="cartridge: iNES mapper number")
    ap.add_argument("--mirroring", choices=("horizontal", "vertical"), default="horizontal")
    ap.add_argument("--out", required=True)
    ap.add_argument("--hex", help="flat/external: write image as hex")
    ap.add_argument("--hex-dir", help="cartridge: directory for .prg.hex/.chr.hex")
    ap.add_argument("--hex-prefix", help="cartridge: hex basename (default: image stem)")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    if args.segment:
        build_flat(args)
    elif args.prg:
        build_cartridge(args)
    else:
        build_external(args)


if __name__ == "__main__":
    main()
