/-
  The Open6502 FSM state enumeration and the opcode → first-state dispatch.

  This file is the bridge between the abstract decoder (`Opcodes.lean`) and the
  cycle-level model (`Micro.lean`).  `dispatchState` is a verbatim transcription
  of the `case (bus_rdata)` inside `ST_FETCH` of `rtl/cpu/open6502_cpu.sv`.
  `dispatch_check` proves, by exhaustive `decide`, that the generated decoder
  agrees with that transcription for all 151 documented opcodes.
-/
import Open6502.Opcodes

namespace Open6502

/-! ## RTL dispatch cross-check

The states below are exactly the `state_t` enumeration of
`rtl/cpu/open6502_cpu.sv`.  Only the first state each opcode dispatches to is
needed for the check; the rest of the sequence is verified in
`Open6502/Refinement.lean`. -/

/-- First FSM state after the opcode fetch, mirroring `state_t`. -/
inductive FState where
  | fetch | imm | zp | zpx | zpy
  | absLo | absHi | absxLo | absxHi | absyLo | absyHi | absData
  | indxPtr | indxLo | indxHi | indyPtr | indyLo | indyHi
  | branch | branchExec | implied | nop
  | jsrPushHi | jsrPushLo | jsrSet
  | rtsPullLo | rtsPullHi
  | pushA | pullA | pushP | pullP | rmwWrite
  | brkPushHi | brkPushLo | brkPushP | brkVecLo | brkVecHi
  | rtiPullP | rtiPullLo | rtiPullHi
  | jmpIndLo | jmpIndHi | jmpIndReadLo | jmpIndReadHi
  | resetLo | resetHi
  | irqPushHi | irqPushLo | irqPushP | irqVecLo | irqVecHi
  deriving DecidableEq, Repr, Inhabited

/-- The RTL's `case (bus_rdata)` dispatch, transcribed verbatim from
`rtl/cpu/open6502_cpu.sv` (the `ST_FETCH` arm). -/
def dispatchState (o : Byte) : FState :=
  match o with
  -- immediate
  | 0xA9 | 0xA2 | 0xA0 | 0x69 | 0xE9 | 0x29 | 0x09 | 0x49 | 0xC9 | 0xE0 | 0xC0 => .imm
  -- zero page
  | 0xA5 | 0xA6 | 0xA4 | 0x85 | 0x86 | 0x84
  | 0x65 | 0xE5 | 0x25 | 0x05 | 0x45
  | 0xC5 | 0xE4 | 0xC4 | 0x06 | 0x46 | 0x26 | 0x66
  | 0xE6 | 0xC6 | 0x24 => .zp
  -- zero page,X
  | 0xB5 | 0x95 | 0x75 | 0xF5 | 0x35 | 0x15 | 0x55
  | 0xD5 | 0xB4 | 0x94 | 0x16 | 0x56 | 0x36 | 0x76
  | 0xF6 | 0xD6 => .zpx
  -- zero page,Y
  | 0xB6 | 0x96 => .zpy
  -- absolute,X
  | 0xBD | 0x9D | 0x7D | 0xFD | 0x3D | 0x1D | 0x5D
  | 0xDD | 0xBC | 0x1E | 0x5E | 0x3E | 0x7E
  | 0xFE | 0xDE => .absxLo
  -- absolute,Y
  | 0xB9 | 0x99 | 0x79 | 0xF9 | 0x39 | 0x19 | 0x59
  | 0xD9 | 0xBE => .absyLo
  -- (zp,X)
  | 0xA1 | 0x81 | 0x61 | 0xE1 | 0x21 | 0x01 | 0x41 | 0xC1 => .indxPtr
  -- (zp),Y
  | 0xB1 | 0x91 | 0x71 | 0xF1 | 0x31 | 0x11 | 0x51 | 0xD1 => .indyPtr
  -- absolute (including JMP/JSR)
  | 0xAD | 0xAE | 0xAC | 0x8D | 0x8E | 0x8C | 0x4C | 0x20
  | 0x0E | 0x4E | 0x2E | 0x6E | 0xEE | 0xCE
  | 0x2C | 0xEC | 0xCC | 0xCD
  | 0x0D | 0x2D | 0x4D | 0x6D | 0xED => .absLo
  -- dedicated sequences
  | 0x6C => .jmpIndLo
  | 0x60 => .rtsPullLo
  | 0x48 => .pushA
  | 0x68 => .pullA
  | 0x08 => .pushP
  | 0x28 => .pullP
  | 0x00 => .brkPushHi
  | 0x40 => .rtiPullP
  -- branches
  | 0xD0 | 0xF0 | 0x90 | 0xB0 | 0x10 | 0x30 | 0x50 | 0x70 => .branch
  -- everything else is the RTL's implied/NOP catch-all
  | _ => .implied

/-- Expected first state from `(opcode, mode)`, independent of the RTL. -/
def firstState (op : Op) (m : Mode) : FState :=
  match op with
  | .rts => .rtsPullLo
  | .rti => .rtiPullP
  | .pha => .pushA
  | .pla => .pullA
  | .php => .pushP
  | .plp => .pullP
  | .brk => .brkPushHi
  | .jmp => match m with
            | .ind => .jmpIndLo
            | _ => .absLo
  | _ =>
    match m with
    | .imp | .acc => .implied
    | .imm => .imm
    | .zp => .zp
    | .zpx => .zpx
    | .zpy => .zpy
    | .abs => .absLo
    | .absx => .absxLo
    | .absy => .absyLo
    | .indx => .indxPtr
    | .indy => .indyPtr
    | .rel => .branch
    | .ind => .jmpIndLo

/-- The generated decoder and the RTL dispatch agree on every official
opcode.  This is a kernel-checked, exhaustive 151-entry equivalence, not a
sampled test. -/
theorem dispatch_check :
    officialTable.all (fun e => dispatchState e.opcode == firstState e.op e.mode) = true := by
  decide

/-- Evaluation lemmas for the opcodes used by the refinement proofs. -/
theorem dispatchState_A9 : dispatchState 0xA9 = .imm := by decide
theorem dispatchState_A5 : dispatchState 0xA5 = .zp := by decide
theorem dispatchState_AD : dispatchState 0xAD = .absLo := by decide
theorem dispatchState_BD : dispatchState 0xBD = .absxLo := by decide
theorem dispatchState_8D : dispatchState 0x8D = .absLo := by decide
theorem dispatchState_69 : dispatchState 0x69 = .imm := by decide
theorem dispatchState_E9 : dispatchState 0xE9 = .imm := by decide
theorem dispatchState_E8 : dispatchState 0xE8 = .implied := by decide
theorem dispatchState_20 : dispatchState 0x20 = .absLo := by decide
theorem dispatchState_60 : dispatchState 0x60 = .rtsPullLo := by decide
theorem dispatchState_D0 : dispatchState 0xD0 = .branch := by decide
theorem dispatchState_6C : dispatchState 0x6C = .jmpIndLo := by decide
theorem dispatchState_00 : dispatchState 0x00 = .brkPushHi := by decide
theorem dispatchState_40 : dispatchState 0x40 = .rtiPullP := by decide
theorem dispatchState_48 : dispatchState 0x48 = .pushA := by decide
theorem dispatchState_68 : dispatchState 0x68 = .pullA := by decide
theorem dispatchState_4C : dispatchState 0x4C = .absLo := by decide
theorem dispatchState_B1 : dispatchState 0xB1 = .indyPtr := by decide
theorem dispatchState_A2 : dispatchState 0xA2 = .imm := by decide
theorem dispatchState_C9 : dispatchState 0xC9 = .imm := by decide
theorem dispatchState_8E : dispatchState 0x8E = .absLo := by decide
theorem dispatchState_05 : dispatchState 0x05 = .zp := by decide

end Open6502
