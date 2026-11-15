#!/usr/bin/env python3
"""Run an am6502 image on a DUT and decide PASS/FAIL.

The verdict comes from the app, not from the caller: `CHECK=halt` waits for the
host exit port (flat: $F002, cartridge: $4019), `CHECK=pc:0x3469` waits for the
guest to stop at that PC (Klaus).  Exhausting a budget is never a pass.

Backends:
  *-nemu  flat 64 KiB images on 6502EMU (`-a 0 -r`)
  *-nes   iNES cartridges on 6502EMU (--headless), GEM6502 (game) or the RTL
          bench, all of which decode $4018 (putc) and $4019 (halt).
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

STATE_RE = re.compile(r"(?:PC:|pc = 0x)([0-9A-Fa-f]{4})")
BAD_TRAP = "HIT BAD TRAP"
STOPPED = "Execution stopped"
HALT_RE = re.compile(r"am6502: halt code (\d+)")
RTL_RE = re.compile(r"AM6502: halt=([0-9A-Fa-f]{2}) frames=(\d+) nonblack=(\d+)")

# The RTL SoC sources, relative to --root (mirrors rtl/Makefile's RTL list).
RTL_SOURCES = [
    "rtl/open6502_pkg.sv",
    "rtl/open6502_top.sv",
    "rtl/video/open6502_video_timing.sv",
    "rtl/input/open6502_controller.sv",
    "rtl/dbg/open6502_difftest_bridge.sv",
    "rtl/bus/open6502_bus.sv",
    "rtl/bus/open6502_wram.sv",
    "rtl/clocks/open6502_clocks.sv",
    "rtl/cpu/open6502_cpu.sv",
    "rtl/ppu/open6502_ppu.sv",
    "rtl/dma/open6502_oam_dma.sv",
    "rtl/apu/open6502_apu.sv",
    "rtl/apu/open6502_dmc.sv",
    "rtl/cart/open6502_mapper.sv",
    "tb/open6502_am6502_tb.sv",
]


def fail(msg):
    print(f"FAIL: {msg}", file=sys.stderr)
    raise SystemExit(1)


def tail(text, lines=20):
    rows = [line for line in text.splitlines() if line.strip()]
    return "\n".join(rows[-lines:])


def ppm_nonblack(path):
    """Count non-black pixels in a binary PPM; 0 means missing/unreadable."""
    data = path.read_bytes()
    if not data.startswith(b"P6"):
        return 0
    idx, values = 2, []
    while len(values) < 3 and idx < len(data):
        while idx < len(data) and data[idx:idx + 1].isspace():
            idx += 1
        if data[idx:idx + 1] == b"#":
            while idx < len(data) and data[idx:idx + 1] != b"\n":
                idx += 1
            continue
        start = idx
        while idx < len(data) and data[idx:idx + 1].isdigit():
            idx += 1
        if start == idx:
            return 0
        values.append(int(data[start:idx]))
    idx += 1
    width, height, _ = values
    pixels = data[idx:idx + width * height * 3]
    return sum(1 for i in range(0, len(pixels) - 2, 3) if pixels[i] or pixels[i + 1] or pixels[i + 2])


def run(cmd):
    try:
        return subprocess.run(cmd, capture_output=True, text=True)
    except FileNotFoundError:
        fail(f"not found: {cmd[0]}")


def check_halt(args, output, rc, where):
    m = HALT_RE.search(output)
    if not m:
        print(tail(output), file=sys.stderr)
        fail(f"{args.app} [{args.arch}/{args.dut}]: {where} never wrote the halt port (no $4019)")
    if int(m.group(1)) != 0:
        print(tail(output), file=sys.stderr)
        fail(f"{args.app} [{args.arch}/{args.dut}]: halt code {m.group(1)}, expected 0")
    if rc != 0:
        print(tail(output), file=sys.stderr)
        fail(f"{args.app} [{args.arch}/{args.dut}]: process exit {rc} after halt 0")


# --------------------------------------------------------------- flat images

def run_flat(args):
    if args.dut != "nemu":
        fail(f"DUT={args.dut} does not run flat images; use ARCH=*-nes for gem/rtl")

    budget = int(args.max_inst, 0)
    # A per-instruction trace would be gigabytes on a long run; drop it then.
    # The final state line still reaches stdout, so the verdict still works.
    default_log = "/dev/null" if budget > 10_000_000 else args.image + ".nemu.log"
    log_path = Path(args.log) if args.log else Path(default_log)

    cmd = [args.nemu, "-b", "-a", "0", "-r", f"--max-inst={args.max_inst}",
           f"--log={log_path}"]
    target_pc = None
    if args.check.startswith("pc:"):
        target_pc = int(args.check[3:], 0)
        cmd.append(f"--stop-pc=0x{target_pc:04x}")
    elif args.check != "halt":
        fail(f"unknown CHECK={args.check!r}; expected 'halt' or 'pc:0xADDR'")
    cmd.append(args.image)

    proc = run(cmd)
    output = proc.stdout + proc.stderr
    log_text = log_path.read_text(errors="replace") if log_path.exists() else ""
    if args.verbose:
        print(output)

    combined = output + log_text
    combined = re.sub(r"pc = 0x[0-9A-Fa-f]{8}", lambda m: "PC:" + m.group(0)[-4:], combined)
    pcs = STATE_RE.findall(combined)
    last_pc = pcs[-1].upper() if pcs else "????"

    if target_pc is not None:
        if pcs and int(pcs[-1], 16) == target_pc and STOPPED in output:
            print(f"PASS {args.app} [{args.arch}/{args.dut}]: stopped at PC=${target_pc:04X}")
            return
        detail = f"expected PC=${target_pc:04X}, last={last_pc}"
        print(tail(combined), file=sys.stderr)
        fail(f"{args.app}: {detail}")

    if proc.returncode == 0 and BAD_TRAP not in output:
        print(f"PASS {args.app} [{args.arch}/{args.dut}]: halt 0, last PC={last_pc}")
        return
    if STOPPED in output:
        print(tail(combined), file=sys.stderr)
        fail(f"{args.app}: timeout after {args.max_inst} instructions, last PC={last_pc}; "
             f"CHECK=halt apps must reach _halt()")
    print(tail(combined), file=sys.stderr)
    fail(f"{args.app}: expected _halt(0)")


# ---------------------------------------------------------- cartridge images

def cartridge_hex(args, suffix):
    image = Path(args.image)
    base = image.name[: -len(image.suffix)] if image.suffix else image.name
    return image.parent / f"{base}{suffix}"


def run_nes(args):
    if args.check != "halt":
        fail(f"cartridge images only support CHECK=halt, got {args.check!r}")
    if args.dut == "nemu":
        run_nes_nemu(args)
    elif args.dut == "gem":
        run_nes_gem(args)
    elif args.dut == "rtl":
        run_nes_rtl(args)
    else:
        fail(f"unknown DUT={args.dut}; use nemu, gem or rtl")


def run_nes_nemu(args):
    ppm = Path(args.image + ".nemu.ppm")
    cmd = [args.nemu, "--headless", f"--frames={args.frames}",
           f"--screenshot={ppm}", "--hash", args.image]
    proc = run(cmd)
    output = proc.stdout + proc.stderr
    if args.verbose:
        print(output)
    if not Path(args.image).exists():
        fail(f"image not found: {args.image}")
    check_halt(args, output, proc.returncode, "NEMU NES")
    nonblack = ppm_nonblack(ppm)
    frames = re.search(r"6502EMU NES: (\d+) frames", output)
    frames = int(frames.group(1)) if frames else 0
    if nonblack == 0:
        print(tail(output), file=sys.stderr)
        fail(f"{args.app} [{args.arch}/{args.dut}]: blank screen ({ppm})")
    print(f"PASS {args.app} [{args.arch}/{args.dut}]: halt 0, {frames} frames, {nonblack} px")


def run_nes_gem(args):
    ppm = Path(args.image + ".gem.ppm")
    cmd = [args.gem, "game", args.image, str(args.frames), str(ppm), "--start"]
    proc = run(cmd)
    output = proc.stdout + proc.stderr
    if args.verbose:
        print(output)
    check_halt(args, output, proc.returncode, "GEM6502")
    nonblack = ppm_nonblack(ppm)
    frames = re.search(r"game: frames=(\d+)", output)
    frames = int(frames.group(1)) if frames else 0
    if nonblack == 0:
        print(tail(output), file=sys.stderr)
        fail(f"{args.app} [{args.arch}/{args.dut}]: blank screen ({ppm})")
    print(f"PASS {args.app} [{args.arch}/{args.dut}]: halt 0, {frames} frames, {nonblack} px")


def build_rtl_bench(args):
    root = Path(args.root)
    work = Path(args.image).parent / "rtl"
    work.mkdir(parents=True, exist_ok=True)
    vvp = work / "am6502_tb.vvp"
    sources = [root / rel for rel in RTL_SOURCES]
    missing = [str(p) for p in sources if not p.exists()]
    if missing:
        fail(f"RTL sources missing: {missing[:3]}{'...' if len(missing) > 3 else ''}")
    if not vvp.exists() or any(p.stat().st_mtime > vvp.stat().st_mtime for p in sources):
        proc = run([args.iverilog, "-g2012", "-o", str(vvp)] + [str(p) for p in sources])
        if proc.returncode != 0:
            print(tail(proc.stdout + proc.stderr, 30), file=sys.stderr)
            fail("iverilog failed to build the RTL bench")
    return vvp


def run_nes_rtl(args):
    vvp = build_rtl_bench(args)
    prg = cartridge_hex(args, ".prg.hex")
    chr_hex = cartridge_hex(args, ".chr.hex")
    for path in (prg, chr_hex):
        if not path.exists():
            fail(f"missing {path}; rebuild the image with `make ARCH={args.arch} APP={args.app}`")
    cmd = [args.vvp, str(vvp), f"+PRG={prg}", f"+CHR={chr_hex}",
           f"+MAX_CYCLES={args.rtl_cycles}"]
    proc = run(cmd)
    output = proc.stdout + proc.stderr
    if args.verbose:
        print(output)
    m = RTL_RE.search(output)
    if not m:
        print(tail(output, 30), file=sys.stderr)
        fail(f"{args.app} [{args.arch}/{args.dut}]: bench produced no verdict")
    halt, frames, nonblack = int(m.group(1), 16), int(m.group(2)), int(m.group(3))
    if halt != 0:
        print(tail(output), file=sys.stderr)
        fail(f"{args.app} [{args.arch}/{args.dut}]: halt code {halt:02x}, expected 0")
    if frames < 2:
        fail(f"{args.app} [{args.arch}/{args.dut}]: only {frames} frames rendered")
    if nonblack == 0:
        fail(f"{args.app} [{args.arch}/{args.dut}]: blank screen")
    if proc.returncode != 0:
        print(tail(output), file=sys.stderr)
        fail(f"{args.app} [{args.arch}/{args.dut}]: bench exit {proc.returncode}")
    print(f"PASS {args.app} [{args.arch}/{args.dut}]: halt 0, {frames} frames, {nonblack} px")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--arch", required=True)
    ap.add_argument("--app", required=True)
    ap.add_argument("--dut", default="nemu")
    ap.add_argument("--image", required=True)
    ap.add_argument("--check", default="halt", help="halt | pc:0xADDR")
    ap.add_argument("--max-inst", default="2000000", help="flat: instruction budget")
    ap.add_argument("--frames", default="120", help="cartridge: frame budget")
    ap.add_argument("--rtl-cycles", default="3000000", help="cartridge/RTL: clock budget")
    ap.add_argument("--nemu", default="NEMU/build/6502EMU")
    ap.add_argument("--gem", default="GEM6502/gem6502")
    ap.add_argument("--root", default=".")
    ap.add_argument("--iverilog", default="iverilog")
    ap.add_argument("--vvp", default="vvp")
    ap.add_argument("--log", help="flat: emulator trace log (default: <image>.nemu.log)")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if args.arch.endswith("-nemu"):
        run_flat(args)
    elif args.arch.endswith("-nes"):
        run_nes(args)
    else:
        fail(f"unknown ARCH={args.arch}; expected *-nemu or *-nes")


if __name__ == "__main__":
    main()
