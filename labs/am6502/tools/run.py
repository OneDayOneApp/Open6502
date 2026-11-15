#!/usr/bin/env python3
"""Run an am6502 image on a DUT and decide PASS/FAIL.

The verdict comes from the app, not from the caller: `CHECK=halt` waits for the
host exit port ($F002 on mos6502-nemu), `CHECK=pc:0x3469` waits for the guest to
stop at that PC (Klaus).  Exhausting the instruction budget is never a pass.

M1 implements DUT=nemu only; gem and rtl are M2/M3 (docs/design.md §11).
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

STATE_RE = re.compile(r"(?:PC:|pc = 0x)([0-9A-Fa-f]{4})")
BAD_TRAP = "HIT BAD TRAP"
STOPPED = "Execution stopped"


def fail(msg):
    print(f"FAIL: {msg}", file=sys.stderr)
    raise SystemExit(1)


def tail(text, lines=20):
    rows = [line for line in text.splitlines() if line.strip()]
    return "\n".join(rows[-lines:])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--arch", required=True)
    ap.add_argument("--app", required=True)
    ap.add_argument("--dut", default="nemu")
    ap.add_argument("--image", required=True)
    ap.add_argument("--check", default="halt", help="halt | pc:0xADDR")
    ap.add_argument("--max-inst", default="2000000")
    ap.add_argument("--nemu", default="NEMU/build/6502EMU")
    ap.add_argument("--log", help="emulator trace log (default: <image>.nemu.log)")
    ap.add_argument("--verbose", action="store_true", help="always show emulator output")
    args = ap.parse_args()

    if args.dut != "nemu":
        fail(f"DUT={args.dut} is not implemented yet (M2/M3); use DUT=nemu")
    if not args.arch.endswith("-nemu"):
        fail(f"ARCH={args.arch} is not a flat image; use DUT=nemu only with *-nemu")

    # 6502EMU prints a per-instruction trace by default; send it to a file so
    # the program's own stdout stays readable, and keep the file for failures.
    # Long runs (Klaus is ~96M instructions) would write gigabytes, so they
    # discard the trace instead; the final state line still reaches stdout.
    budget = int(args.max_inst, 0)
    default_log = "/dev/null" if budget > 10_000_000 else args.image + ".nemu.log"
    log_path = Path(args.log) if args.log else Path(default_log)
    cmd = [
        args.nemu, "-b",
        "-a", "0", "-r",
        f"--max-inst={args.max_inst}",
        f"--log={log_path}",
    ]

    target_pc = None
    if args.check.startswith("pc:"):
        target_pc = int(args.check[3:], 0)
        cmd.append(f"--stop-pc=0x{target_pc:04x}")
    elif args.check != "halt":
        fail(f"unknown CHECK={args.check!r}; expected 'halt' or 'pc:0xADDR'")

    cmd.append(args.image)

    try:
        proc = subprocess.run(cmd, capture_output=True, text=True)
    except FileNotFoundError:
        fail(f"emulator not found: {args.nemu} (build it: make -C NEMU m6502_defconfig && make -C NEMU)")

    output = proc.stdout + proc.stderr
    log_text = log_path.read_text(errors="replace") if log_path.exists() else ""
    if args.verbose:
        print(output)
        print(log_text)

    combined = output + log_text
    combined = re.sub(r"pc = 0x[0-9A-Fa-f]{8}", lambda m: "PC:" + m.group(0)[-4:], combined)
    pcs = STATE_RE.findall(combined)
    last_pc = pcs[-1].upper() if pcs else "????"

    if target_pc is not None:
        ok = bool(pcs) and int(pcs[-1], 16) == target_pc and STOPPED in output
        if ok:
            print(f"PASS {args.app} [{args.arch}/{args.dut}]: stopped at PC=${target_pc:04X}")
            return
        detail = (
            f"expected PC=${target_pc:04X}, last={last_pc}"
            if pcs else f"no PC in emulator output; expected ${target_pc:04X}"
        )
        print(f"{args.app} [{args.arch}/{args.dut}]: {detail}", file=sys.stderr)
        print(tail(output), file=sys.stderr)
        fail(f"{args.app}: {detail}")

    if proc.returncode == 0 and BAD_TRAP not in output:
        print(f"PASS {args.app} [{args.arch}/{args.dut}]: halt 0, last PC={last_pc}")
        return

    if STOPPED in output:
        print(tail(combined), file=sys.stderr)
        fail(
            f"{args.app}: timeout after {args.max_inst} instructions, last PC={last_pc}; "
            f"CHECK=halt apps must reach _halt()"
        )

    print(f"{args.app} [{args.arch}/{args.dut}]: exit={proc.returncode}, last PC={last_pc}",
          file=sys.stderr)
    print(tail(combined), file=sys.stderr)
    fail(f"{args.app}: expected _halt(0)")


if __name__ == "__main__":
    main()
