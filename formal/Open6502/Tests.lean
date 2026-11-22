/-
  Self-checking program tests.

  These mirror the directed SystemVerilog testbenches in `tb/` (`open6502_adc_tb`,
  `open6502_decimal_tb`, `open6502_jsr_tb`, `open6502_brk_tb`,
  `open6502_inc_tb`, `open6502_jmp_ind_tb`, ...).  Each test assembles a tiny
  6502 program into a memory image, runs the ISA model, and states the expected
  result as a closed proposition discharged by `decide`.  Because the model is
  executable, the kernel literally runs the program when checking the proof.

  These are functional smoke tests that complement the symbolic theorems in
  `Open6502/Refinement.lean`; they catch mistakes that a single-instruction
  refinement statement cannot (e.g. wrong reset vector handling).
-/
import Open6502.ISA

namespace Open6502

/-- Build a memory image from `(address, byte)` pairs; unlisted bytes read as
`#$EA` (NOP). -/
def gen (bytes : List (Word × Byte)) : Mem :=
  bytes.foldl (fun m p => Mem.write m p.1 p.2) (fun _ => 0xEA)

/-- Convenience: assemble a program at `base`. -/
def progAt (base : Word) (bs : List Byte) : List (Word × Byte) :=
  bs.zipIdx.map (fun (b, i) => (base + BitVec.ofNat 16 i, b))

/-- A reset vector pointing at `base`. -/
def vectors (base : Word) : List (Word × Byte) :=
  [(0xFFFC, lo base), (0xFFFD, hi base), (0xFFFE, lo base), (0xFFFF, hi base)]

/-! ## Binary ADC (cf. `tb/open6502_adc_tb.sv`) -/

/-- SEC; LDA #0; ADC #0; STA $0200. -/
def adcProg : List (Word × Byte) :=
  progAt 0x8000 [0x38, 0xA9, 0x00, 0x69, 0x00, 0x8D, 0x00, 0x02, 0x4C, 0x08, 0x80]

example : (run 5 (reset (gen (adcProg ++ vectors 0x8000)))).mem 0x0200 = 0x01 := by
  decide

example : Cf (run 5 (reset (gen (adcProg ++ vectors 0x8000)))).regs.p = false := by
  decide

/-! ## Decimal ADC/SBC (cf. `tb/open6502_decimal_tb.sv`)

SED; LDA #$99; CLC; ADC #$99; STA $0200; LDA #0; SEC; SBC #1; STA $0201. -/

def decimalProg : List (Word × Byte) :=
  progAt 0x8000
    [0xF8, 0xA9, 0x99, 0x18, 0x69, 0x99, 0x8D, 0x00, 0x02,
     0xA9, 0x00, 0x38, 0xE9, 0x01, 0x8D, 0x01, 0x02, 0x4C, 0x11, 0x80]

example : (run 9 (reset (gen (decimalProg ++ vectors 0x8000)))).mem 0x0200 = 0x98 := by
  decide

example : (run 9 (reset (gen (decimalProg ++ vectors 0x8000)))).mem 0x0201 = 0x99 := by
  decide

/-! ## JSR/RTS (cf. `tb/open6502_jsr_tb.sv`)

JSR $8010; STA $0200; JMP *; ...; $8010: LDA #$42; RTS. -/

def jsrProg : List (Word × Byte) :=
  progAt 0x8000
    ([0x20, 0x10, 0x80, 0x8D, 0x00, 0x02, 0x4C, 0x06, 0x80]
     ++ List.replicate 7 0xEA ++ [0xA9, 0x42, 0x60])

example : (run 4 (reset (gen (jsrProg ++ vectors 0x8000)))).mem 0x0200 = 0x42 := by
  decide

/-- JSR/RTS restore the stack pointer exactly. -/
example : (run 4 (reset (gen (jsrProg ++ vectors 0x8000)))).regs.sp = 0xFD := by
  decide

/-! ## BRK/RTI (cf. `tb/open6502_brk_tb.sv`)

BRK; LDA #$7E; JMP *; handler at $9000: STA $0200; RTI. -/

def brkProg : List (Word × Byte) :=
  progAt 0x8000 [0x00, 0xEA, 0xA9, 0x11, 0x8D, 0x01, 0x02]
  ++ progAt 0x9000 [0xA9, 0x7E, 0x8D, 0x00, 0x02, 0x40]

def brkImage : Mem :=
  gen (brkProg ++ [(0xFFFC, 0x00), (0xFFFD, 0x80), (0xFFFE, 0x00), (0xFFFF, 0x90)])

example : (run 6 (reset brkImage)).mem 0x0200 = 0x7E := by
  decide

example : (run 6 (reset brkImage)).mem 0x0201 = 0x11 := by
  decide

example : (run 6 (reset brkImage)).regs.a = 0x11 := by
  decide

example : (run 6 (reset brkImage)).regs.sp = 0xFD := by
  decide

/-! ## RMW: INC/ASL (cf. `tb/open6502_inc_tb.sv`, `tb/open6502_rmw_tb.sv`)

LDA #$10; STA $20; INC $20; ASL $20. -/

def rmwProg : List (Word × Byte) :=
  progAt 0x8000 [0xA9, 0x10, 0x85, 0x20, 0xE6, 0x20, 0x06, 0x20, 0x4C, 0x08, 0x80]

example : (run 4 (reset (gen (rmwProg ++ vectors 0x8000)))).mem 0x0020 = 0x22 := by
  decide

/-! ## Zero-page wrap (`tb/open6502_zp37_tb.sv`)

LDA #$AB; STA $FF; LDX #$01; LDA $FE,X  →  reads $FF. -/

def zpProg : List (Word × Byte) :=
  progAt 0x8000 [0xA9, 0xAB, 0x85, 0xFF, 0xA2, 0x01, 0xB5, 0xFE, 0x4C, 0x08, 0x80]

example : (run 4 (reset (gen (zpProg ++ vectors 0x8000)))).regs.a = 0xAB := by
  decide

/-! ## Indirect JMP page wrap (cf. `tb/open6502_jmp_ind_tb.sv`)

JMP ($10FF) with low byte at $10FF and high byte at $1000 (not $1100). -/

def jmpIndProg : List (Word × Byte) :=
  progAt 0x8000 [0x6C, 0xFF, 0x10]
  ++ [(0x10FF, 0x00), (0x1000, 0x90)]
  ++ progAt 0x9000 [0xA9, 0x55, 0x8D, 0x00, 0x02, 0x4C, 0x05, 0x90]

example : (run 3 (reset (gen (jmpIndProg ++ vectors 0x8000)))).mem 0x0200 = 0x55 := by
  decide

/-! ## Branches

LDX #$00; loop: INX; CPX #$05; BNE loop → X = 5. -/
def branchProg : List (Word × Byte) :=
  progAt 0x8000 [0xA2, 0x00, 0xE8, 0xE0, 0x05, 0xD0, 0xFB, 0x4C, 0x07, 0x80]

example : (run 14 (reset (gen (branchProg ++ vectors 0x8000)))).regs.x = 0x05 := by
  decide

/-! ## Illegal opcode behaves like a one-byte NOP, as in the RTL -/
def illegalProg : List (Word × Byte) :=
  progAt 0x8000 [0x02, 0xA9, 0x33, 0x4C, 0x03, 0x80]

example : (run 2 (reset (gen (illegalProg ++ vectors 0x8000)))).regs.a = 0x33 := by
  decide

end Open6502
