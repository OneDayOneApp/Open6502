#!/usr/bin/env python3
"""Assemble and check am6502 images.

Two producers, one contract (see labs/am6502/docs/design.md §4):

  flat      a ld65 segment laid out at AM_LOAD_ADDR, padded down to $0000 and
            up to $FFFF, to make the 65,536-byte memory image mos6502-nemu runs
  external  a prebuilt binary (Klaus) that only needs its reset vector filled in

Every image is verified: right size, and a reset vector that points inside it.
The hex output is one byte per line, which is what $readmemh wants.
"""

import argparse
import hashlib
import sys
from pathlib import Path

IMAGE_SIZE = 0x10000
RESET_VECTOR = 0xFFFC


def fail(msg):
    print(f"mkimage: error: {msg}", file=sys.stderr)
    raise SystemExit(1)


def parse_int(text):
    try:
        return int(text, 0)
    except ValueError:
        fail(f"not a number: {text!r}")


def reset_vector(data):
    return data[RESET_VECTOR] | (data[RESET_VECTOR + 1] << 8)


def verify(data, load_addr):
    if len(data) != IMAGE_SIZE:
        fail(f"image is {len(data)} bytes, expected {IMAGE_SIZE}")
    vec = reset_vector(data)
    if vec < load_addr or vec >= IMAGE_SIZE:
        fail(
            f"reset vector ${vec:04X} is outside the image "
            f"(${load_addr:04X}-$FFFF); did the VECTORS segment move?"
        )
    if data[0xFFFA:0x10000] == b"\x00" * 6:
        fail("NMI/RESET/IRQ vectors are all zero")


def write_outputs(data, out, hex_path):
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(data)
    if hex_path:
        hex_path.parent.mkdir(parents=True, exist_ok=True)
        hex_path.write_text("\n".join(f"{b:02x}" for b in data) + "\n")
    digest = hashlib.sha256(data).hexdigest()
    print(
        f"image: {out} size={len(data)} reset=${reset_vector(data):04X} "
        f"sha256={digest}"
    )


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
    if args.check:
        verify(data, load_addr)
    return data, load_addr


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
    if args.check:
        verify(data, 0)
    return bytes(data), 0


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--segment", help="ld65 binary segment laid out at --load-addr")
    src.add_argument("--input", help="prebuilt binary to pad into a 64 KiB image")
    ap.add_argument("--load-addr", default="0x0400", help="flat image load address")
    ap.add_argument("--set-vector", help="overwrite $FFFC with this address")
    ap.add_argument("--out", required=True, help="output image path")
    ap.add_argument("--hex", help="also write one-byte-per-line hex for $readmemh")
    ap.add_argument("--check", action="store_true", help="verify size and reset vector")
    args = ap.parse_args()

    data, load_addr = build_flat(args) if args.segment else build_external(args)
    write_outputs(data, Path(args.out), Path(args.hex) if args.hex else None)


if __name__ == "__main__":
    main()
