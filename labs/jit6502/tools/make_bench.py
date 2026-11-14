#!/usr/bin/env python3
"""Generate the lab's benchmark images.

Every benchmark is a tight 6502 loop at $8000 with its data at $2000, so the
code window passed to the JIT is [0x8000, 0x10000) and stores never look like
code.  Usage: python3 tools/make_bench.py OUTDIR
"""
import pathlib
import sys

BENCHES = {
    # 8-bit accumulate + indexed store: LDA/ADC/STA/INX/BNE
    "arith_loop": bytes([
        0xA9, 0x00,        # 8000 LDA #$00
        0xA2, 0x00,        # 8002 LDX #$00
        0x18,              # 8004 CLC
        0x69, 0x05,        # 8005 ADC #$05
        0x9D, 0x00, 0x20,  # 8007 STA $2000,X
        0xE8,              # 800A INX
        0xD0, 0xF8,        # 800B BNE $8005
        0xA2, 0x00,        # 800D LDX #$00
        0x4C, 0x05, 0x80,  # 800F JMP $8005
    ]),
    # indirect-indexed copy: LDA ($10),Y / STA ($12),Y / INY / BNE
    "copy_loop": bytes([
        0xA0, 0x00,        # 8000 LDY #$00
        0xB1, 0x10,        # 8002 LDA ($10),Y
        0x91, 0x12,        # 8004 STA ($12),Y
        0xC8,              # 8006 INY
        0xD0, 0xF9,        # 8007 BNE $8002
        0x4C, 0x00, 0x80,  # 8009 JMP $8000
    ]),
    # subroutine call overhead: JSR/RTS with a small add
    "call_loop": bytes([
        0xA2, 0x00,        # 8000 LDX #$00
        0x20, 0x20, 0x80,  # 8002 JSR $8020
        0xE8,              # 8005 INX
        0xD0, 0xFA,        # 8006 BNE $8002
        0x4C, 0x00, 0x80,  # 8008 JMP $8000
        0xEA,              # 800B NOP padding
        0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA,
        0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA,
        0x18,              # 8020 CLC
        0x69, 0x03,        # 8021 ADC #$03
        0x60,              # 8023 RTS
    ]),
}


def main() -> int:
    outdir = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "bench")
    outdir.mkdir(parents=True, exist_ok=True)
    for name, code in BENCHES.items():
        img = bytearray(65536)
        img[0x8000:0x8000 + len(code)] = code
        img[0xFFFC:0xFFFE] = b"\x00\x80"
        # zero-page pointers for copy_loop
        if name == "copy_loop":
            img[0x10:0x14] = bytes([0x00, 0x28, 0x00, 0x2A])  # $2800 -> $2A00
        (outdir / f"{name}.bin").write_bytes(img)
        print(f"wrote {outdir / (name + '.bin')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
