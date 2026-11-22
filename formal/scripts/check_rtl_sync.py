#!/usr/bin/env python3
"""Check that the Lean decoder and the SystemVerilog dispatch agree.

The Lean side proves `dispatch_check` (Open6502/FSM.lean) against a transcription
of the RTL's `case (bus_rdata)`.  This script closes the loop on the *actual
source file*: it parses `rtl/cpu/open6502_cpu.sv`, extracts the opcode → first
state table from `ST_FETCH`, and compares it with `build/decode.json`, which
`lake exe open6502-extract` emits from the same Lean table used by the proofs.

Usage:
    python3 scripts/check_rtl_sync.py [--rtl PATH] [--json PATH]

Exit code 0 means: every listed opcode has the expected first state, no opcode
is listed twice, and every non-implied official opcode is listed.
"""

import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RTL_DEFAULT = os.path.join(ROOT, "rtl", "cpu", "open6502_cpu.sv")
JSON_DEFAULT = os.path.join(ROOT, "formal", "build", "decode.json")

# Expected first state, mirroring `Open6502.firstState` in Open6502/FSM.lean.
MODE_STATE = {
    "imm": "ST_IMM",
    "zp": "ST_ZP",
    "zpx": "ST_ZPX",
    "zpy": "ST_ZPY",
    "absx": "ST_ABSX_LO",
    "absy": "ST_ABSY_LO",
    "indx": "ST_INDX_PTR",
    "indy": "ST_INDY_PTR",
    "abs": "ST_ABS_LO",
    "ind": "ST_JMP_IND_LO",
    "rel": "ST_BRANCH",
}

DEDICATED = {
    "RTS": "ST_RTS_PULL_LO",
    "RTI": "ST_RTI_PULL_P",
    "PHA": "ST_PUSH_A",
    "PLA": "ST_PULL_A",
    "PHP": "ST_PUSH_P",
    "PLP": "ST_PULL_P",
    "BRK": "ST_BRK_PUSH_HI",
}

DEFAULT_STATE = "ST_IMPLIED"


def expected_state(mnemonic: str, mode: str) -> str:
    if mnemonic in DEDICATED:
        return DEDICATED[mnemonic]
    if mnemonic == "JMP" and mode == "ind":
        return "ST_JMP_IND_LO"
    if mode in ("imp", "acc"):
        return DEFAULT_STATE
    return MODE_STATE[mode]


def parse_rtl_dispatch(text: str) -> dict[int, str]:
    """Extract opcode -> first state from the `ST_FETCH` case statement."""
    fetch = text.index("ST_FETCH: begin")
    case_start = text.index("case (bus_rdata)", fetch)
    # The matching endcase: the RTL has no nested case inside this one.
    case_end = text.index("endcase", case_start)
    body = text[case_start:case_end]

    # Split into `8'hXX, ... : state <= ST_YYY` statements.
    table: dict[int, str] = {}
    for stmt in body.split(";"):
        ops = re.findall(r"8'h([0-9A-Fa-f]{2})", stmt)
        if not ops:
            continue
        m = re.search(r"state\s*<=\s*(ST_[A-Z_0-9]+)", stmt)
        if not m:
            continue
        state = m.group(1)
        for op in ops:
            code = int(op, 16)
            if code in table and table[code] != state:
                raise SystemExit(
                    f"RTL lists opcode 0x{code:02X} in both {table[code]} and {state}"
                )
            table[code] = state
    return table


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--rtl", default=RTL_DEFAULT)
    ap.add_argument("--json", default=JSON_DEFAULT)
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    text = open(args.rtl, encoding="utf-8").read()
    rtl = parse_rtl_dispatch(text)
    data = json.load(open(args.json, encoding="utf-8"))
    lean = {e["opcode"]: e for e in data["opcodes"]}

    errors: list[str] = []

    if data["official_count"] != len(lean):
        errors.append(
            f"JSON says {data['official_count']} opcodes but lists {len(lean)}"
        )

    for code, entry in sorted(lean.items()):
        want = expected_state(entry["mnemonic"], entry["mode"])
        got = rtl.get(code, DEFAULT_STATE)
        if got != want:
            errors.append(
                f"0x{code:02X} {entry['mnemonic']} {entry['mode']}: "
                f"RTL dispatches to {got}, Lean expects {want}"
            )
        elif args.verbose:
            print(f"ok 0x{code:02X} {entry['mnemonic']:3} {entry['mode']:5} -> {got}")

    for code in sorted(rtl):
        if code not in lean:
            errors.append(
                f"RTL lists 0x{code:02X} ({rtl[code]}) but the Lean table has no "
                f"such official opcode"
            )

    if errors:
        print(f"FAIL: {len(errors)} mismatch(es)")
        for e in errors:
            print("  " + e)
        return 1

    print(
        f"OK: {len(lean)} official opcodes agree with the RTL dispatch "
        f"({len(rtl)} opcodes are listed explicitly, the rest use the implied default)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
