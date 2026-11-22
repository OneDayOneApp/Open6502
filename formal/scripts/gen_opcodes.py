#!/usr/bin/env python3
"""Generate Open6502/Opcodes.lean from the documented NMOS 6502 matrix.

Keeping the matrix in one place and generating Lean from it means the decoder
can never drift from the table by a typo.  The generator is checked in for
reproducibility; the generated file is committed so a plain `lake build` works
without Python.

Usage: python3 scripts/gen_opcodes.py > Open6502/Opcodes.lean
"""

# 16x16 documented NMOS 6502 opcode matrix.  "---" marks an unimplemented
# (illegal) opcode; 0xEA is the documented NOP; 0xEB is the undocumented SBC #
# that the reference table in NEMU also accepts.
ROWS = [
    # 0x0_
    ["BRK IMP", "ORA INDX", "---", "---", "---", "ORA ZP", "ASL ZP", "---",
     "PHP IMP", "ORA IMM", "ASL ACC", "---", "---", "ORA ABS", "ASL ABS", "---"],
    # 0x1_
    ["BPL REL", "ORA INDY", "---", "---", "---", "ORA ZPX", "ASL ZPX", "---",
     "CLC IMP", "ORA ABSY", "---", "---", "---", "ORA ABSX", "ASL ABSX", "---"],
    # 0x2_
    ["JSR ABS", "AND INDX", "---", "---", "BIT ZP", "AND ZP", "ROL ZP", "---",
     "PLP IMP", "AND IMM", "ROL ACC", "---", "BIT ABS", "AND ABS", "ROL ABS", "---"],
    # 0x3_
    ["BMI REL", "AND INDY", "---", "---", "---", "AND ZPX", "ROL ZPX", "---",
     "SEC IMP", "AND ABSY", "---", "---", "---", "AND ABSX", "ROL ABSX", "---"],
    # 0x4_
    ["RTI IMP", "EOR INDX", "---", "---", "---", "EOR ZP", "LSR ZP", "---",
     "PHA IMP", "EOR IMM", "LSR ACC", "---", "JMP ABS", "EOR ABS", "LSR ABS", "---"],
    # 0x5_
    ["BVC REL", "EOR INDY", "---", "---", "---", "EOR ZPX", "LSR ZPX", "---",
     "CLI IMP", "EOR ABSY", "---", "---", "---", "EOR ABSX", "LSR ABSX", "---"],
    # 0x6_
    ["RTS IMP", "ADC INDX", "---", "---", "---", "ADC ZP", "ROR ZP", "---",
     "PLA IMP", "ADC IMM", "ROR ACC", "---", "JMP IND", "ADC ABS", "ROR ABS", "---"],
    # 0x7_
    ["BVS REL", "ADC INDY", "---", "---", "---", "ADC ZPX", "ROR ZPX", "---",
     "SEI IMP", "ADC ABSY", "---", "---", "---", "ADC ABSX", "ROR ABSX", "---"],
    # 0x8_
    ["---", "STA INDX", "---", "---", "STY ZP", "STA ZP", "STX ZP", "---",
     "DEY IMP", "---", "TXA IMP", "---", "STY ABS", "STA ABS", "STX ABS", "---"],
    # 0x9_
    ["BCC REL", "STA INDY", "---", "---", "STY ZPX", "STA ZPX", "STX ZPY", "---",
     "TYA IMP", "STA ABSY", "TXS IMP", "---", "---", "STA ABSX", "---", "---"],
    # 0xA_
    ["LDY IMM", "LDA INDX", "LDX IMM", "---", "LDY ZP", "LDA ZP", "LDX ZP", "---",
     "TAY IMP", "LDA IMM", "TAX IMP", "---", "LDY ABS", "LDA ABS", "LDX ABS", "---"],
    # 0xB_
    ["BCS REL", "LDA INDY", "---", "---", "LDY ZPX", "LDA ZPX", "LDX ZPY", "---",
     "CLV IMP", "LDA ABSY", "TSX IMP", "---", "LDY ABSX", "LDA ABSX", "LDX ABSY", "---"],
    # 0xC_
    ["CPY IMM", "CMP INDX", "---", "---", "CPY ZP", "CMP ZP", "DEC ZP", "---",
     "INY IMP", "CMP IMM", "DEX IMP", "---", "CPY ABS", "CMP ABS", "DEC ABS", "---"],
    # 0xD_
    ["BNE REL", "CMP INDY", "---", "---", "---", "CMP ZPX", "DEC ZPX", "---",
     "CLD IMP", "CMP ABSY", "---", "---", "---", "CMP ABSX", "DEC ABSX", "---"],
    # 0xE_
    ["CPX IMM", "SBC INDX", "---", "---", "CPX ZP", "SBC ZP", "INC ZP", "---",
     "INX IMP", "SBC IMM", "NOP IMP", "---", "CPX ABS", "SBC ABS", "INC ABS", "---"],
    # 0xF_
    ["BEQ REL", "SBC INDY", "---", "---", "---", "SBC ZPX", "INC ZPX", "---",
     "SED IMP", "SBC ABSY", "---", "---", "---", "SBC ABSX", "INC ABSX", "---"],
]

# Base cycle counts (documented NMOS 6502, no page-cross penalty).  Keyed by
# "MNEMONIC MODE".
CYCLES = {
    "BRK IMP": 7, "PHP IMP": 3, "PLP IMP": 4, "PHA IMP": 3, "PLA IMP": 4,
    "JSR ABS": 6, "RTS IMP": 6, "RTI IMP": 6, "JMP ABS": 3, "JMP IND": 5,
    "NOP IMP": 2,
    "CLC IMP": 2, "SEC IMP": 2, "CLI IMP": 2, "SEI IMP": 2,
    "CLV IMP": 2, "CLD IMP": 2, "SED IMP": 2,
    "TAX IMP": 2, "TXA IMP": 2, "TAY IMP": 2, "TYA IMP": 2,
    "TSX IMP": 2, "TXS IMP": 2, "INX IMP": 2, "INY IMP": 2,
    "DEX IMP": 2, "DEY IMP": 2,
    "BPL REL": 2, "BMI REL": 2, "BVC REL": 2, "BVS REL": 2,
    "BCC REL": 2, "BCS REL": 2, "BNE REL": 2, "BEQ REL": 2,
    "ASL ACC": 2, "LSR ACC": 2, "ROL ACC": 2, "ROR ACC": 2,
}

def cycles(mnemonic: str, mode: str) -> int:
    key = f"{mnemonic} {mode}"
    if key in CYCLES:
        return CYCLES[key]
    if mode in ("IMM",):
        return 2
    if mode in ("ZP",):
        return 6 if mnemonic in ("ASL", "LSR", "ROL", "ROR", "INC", "DEC") else 3
    if mode in ("ZPX", "ZPY"):
        return 7 if mnemonic in ("ASL", "LSR", "ROL", "ROR", "INC", "DEC") else 4
    if mode == "ABS":
        return 6 if mnemonic in ("ASL", "LSR", "ROL", "ROR", "INC", "DEC") else 4
    if mode in ("ABSX", "ABSY"):
        return 7 if mnemonic in ("ASL", "LSR", "ROL", "ROR", "INC", "DEC") else 4
    if mode in ("INDX", "INDY"):
        return 6
    return 2


def main() -> None:
    entries = []
    for hi, row in enumerate(ROWS):
        for lo, cell in enumerate(row):
            opcode = (hi << 4) | lo
            if cell == "---":
                continue
            mnemonic, mode = cell.split()
            entries.append((opcode, mnemonic, mode, cycles(mnemonic, mode)))

    op_names = sorted({e[1] for e in entries}, key=str.lower)

    def ctor(mnemonic: str) -> str:
        """Lean constructor name: `and` is a keyword, so AND becomes and_."""
        return "and_" if mnemonic == "AND" else mnemonic.lower()

    mode_names = ["imp", "acc", "imm", "zp", "zpx", "zpy",
                  "abs", "absx", "absy", "ind", "indx", "indy", "rel"]

    out = []
    w = out.append
    w("""/-
  Auto-generated by `scripts/gen_opcodes.py`.  Do not edit by hand.

  Documented NMOS 6502 opcode matrix: 151 official opcodes.  Illegal opcodes
  are intentionally absent; `decode` returns `none` for them.
-/
import Open6502.Basic

namespace Open6502

/-- Addressing modes of the 6502.  `ind` is the JMP indirect mode; `indx` and
`indy` are the zero-page indexed indirect forms written `($nn,X)` and
`($nn),Y` in assembly. -/
inductive Mode where
  | imp | acc | imm | zp | zpx | zpy | abs | absx | absy | ind | indx | indy | rel
  deriving DecidableEq, Repr, Inhabited

/-- The documented NMOS 6502 instruction set. -/
inductive Op where""")
    for name in op_names:
        w(f"  | {ctor(name)}")
    w("""  deriving DecidableEq, Repr, Inhabited

/-- One row of the opcode matrix. -/
structure Entry where
  opcode : Byte
  op : Op
  mode : Mode
  cycles : Nat
  deriving DecidableEq, Repr, Inhabited

/-- The 151 documented opcodes, in ascending opcode order. -/
def officialTable : List Entry := [""")
    for opcode, mnemonic, mode, cyc in entries:
        w(f"  {{ opcode := 0x{opcode:02X}, op := .{ctor(mnemonic)}, "
          f"mode := .{mode.lower()}, cycles := {cyc} }},")
    w("""]

/-- Every documented opcode, in ascending order. -/
def officialOpcodes : List Byte := officialTable.map (·.opcode)

/-- Table lookup: `decode o` is the instruction encoded by `o`, or `none` for
an illegal opcode. -/
def decode (o : Byte) : Option (Op × Mode) :=
  (officialTable.find? (fun e => e.opcode == o)).map (fun e => (e.op, e.mode))

/-- Base cycle count (no page-cross penalty) of a decoded opcode. -/
def baseCycles (o : Byte) : Option Nat :=
  (officialTable.find? (fun e => e.opcode == o)).map (·.cycles)

/-- Number of operand bytes that follow the opcode. -/
def operandBytes : Mode → Nat
  | .imp | .acc => 0
  | .imm | .zp | .zpx | .zpy | .indx | .indy | .rel => 1
  | .abs | .absx | .absy | .ind => 2

/-- Total instruction length including the opcode byte. -/
def instrLength (o : Byte) : Nat :=
  match decode o with
  | some (_, m) => 1 + operandBytes m
  | none => 1

/-- Human-readable mnemonic, used by the disassembler and the trace tools. -/
def Op.mnemonic : Op → String""")
    for name in op_names:
        w(f'  | .{ctor(name)} => "{name}"')
    w("""
end Open6502
""")
    print("\n".join(out))


if __name__ == "__main__":
    main()
