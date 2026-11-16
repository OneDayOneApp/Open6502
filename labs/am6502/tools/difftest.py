#!/usr/bin/env python3
"""Lockstep DiffTest for flat am6502 images: NEMU vs RTL, per instruction.

Both sides run the same 65,536-byte image (no bench-patched vectors) and emit
one state line per retired instruction; `difftrace.py` compares them and names
the first divergence.

  make difftest                       # hello on mos6502-nemu
  make difftest APP=klaus             # ~96M instructions, ~1 GiB of trace
"""

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

TB = "tb/open6502_am6502_flat_tb.sv"
CPU = "rtl/cpu/open6502_cpu.sv"


def fail(msg):
    print(f"FAIL: {msg}", file=sys.stderr)
    raise SystemExit(1)


def run(cmd, env=None):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, env=env)
    except FileNotFoundError:
        fail(f"not found: {cmd[0]}")


def build_bench(args, workdir):
    vvp = workdir / "flat_tb.vvp"
    sources = [Path(args.root) / CPU, Path(args.root) / TB]
    missing = [str(p) for p in sources if not p.exists()]
    if missing:
        fail(f"missing RTL sources: {missing}")
    if not vvp.exists() or any(p.stat().st_mtime > vvp.stat().st_mtime for p in sources):
        proc = run([args.iverilog, "-g2012", "-o", str(vvp)] + [str(p) for p in sources])
        if proc.returncode != 0:
            print((proc.stdout + proc.stderr)[-2000:], file=sys.stderr)
            fail("iverilog failed to build the flat DiffTest bench")
    return vvp


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--arch", required=True)
    ap.add_argument("--app", required=True)
    ap.add_argument("--image", required=True)
    ap.add_argument("--check", default="halt")
    ap.add_argument("--max-inst", default="2000000")
    ap.add_argument("--nemu", default="NEMU/build/6502EMU")
    ap.add_argument("--iverilog", default="iverilog")
    ap.add_argument("--vvp", default="vvp")
    ap.add_argument("--root", default=".")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not args.arch.endswith("-nemu"):
        fail(f"DiffTest needs a flat image; ARCH={args.arch} is not *-nemu")

    image = Path(args.image)
    hex_path = image.with_suffix(".hex")
    if not hex_path.exists():
        fail(f"missing {hex_path}; rebuild with `make ARCH={args.arch} APP={args.app}`")

    workdir = image.parent / "difftest"
    workdir.mkdir(parents=True, exist_ok=True)
    nemu_trace = workdir / "nemu.trace"
    rtl_trace = workdir / "rtl.trace"

    env = dict(os.environ, NEMU_TRACE_STATE=str(nemu_trace))
    cmd = [args.nemu, "-b", "-a", "0", "-r", f"--max-inst={args.max_inst}",
           f"--log={workdir / 'nemu.log'}"]
    target_pc = None
    if args.check.startswith("pc:"):
        target_pc = int(args.check[3:], 0)
        cmd.append(f"--stop-pc=0x{target_pc:04x}")
    cmd.append(str(image))

    proc = run(cmd, env=env)
    nemu_out = proc.stdout + proc.stderr
    if args.verbose:
        print(nemu_out)
    if not nemu_trace.exists() or nemu_trace.stat().st_size == 0:
        print(nemu_out[-3000:], file=sys.stderr)
        fail("NEMU produced no state trace")
    if target_pc is None and proc.returncode != 0:
        print(nemu_out[-3000:], file=sys.stderr)
        fail(f"NEMU exited {proc.returncode} instead of halting cleanly")

    vvp = build_bench(args, workdir)
    rtl_cmd = [args.vvp, str(vvp), f"+IMAGE={hex_path}",
               f"+TRACE={rtl_trace}", f"+MAX_INST={args.max_inst}"]
    if target_pc is not None:
        rtl_cmd.append(f"+STOP_PC=0x{target_pc:04x}")
    rtl = run(rtl_cmd)
    rtl_out = rtl.stdout + rtl.stderr
    if args.verbose:
        print(rtl_out)
    summary = re.search(r"AM6502 FLAT: inst=(\d+) halted=([01])", rtl_out)
    if not summary:
        print(rtl_out[-3000:], file=sys.stderr)
        fail("RTL bench produced no summary")
    if rtl.returncode != 0:
        print(rtl_out[-3000:], file=sys.stderr)
        fail(f"RTL bench exited {rtl.returncode}")

    compare = run([sys.executable, str(Path(__file__).parent / "difftrace.py"),
                   str(nemu_trace), str(rtl_trace)])
    report = (compare.stdout + compare.stderr).strip()
    if args.verbose:
        print(report)
    if compare.returncode != 0:
        print(report, file=sys.stderr)
        fail(f"{args.app}: NEMU and RTL diverge (traces in {workdir})")

    match = re.search(r"PASS: (\d+) retired", report)
    count = match.group(1) if match else summary.group(1)
    print(f"PASS {args.app} [{args.arch}/difftest]: {count} retired instructions match")


if __name__ == "__main__":
    main()
