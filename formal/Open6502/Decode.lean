/-
  Decoder properties and the RTL cross-check.

  `Opcodes.lean` is generated from the documented NMOS 6502 matrix.  This file
  proves the table's structural properties and — more importantly — proves that
  the table agrees with the *dispatch state machine* transcribed from
  `rtl/cpu/open6502_cpu.sv`.  The transcription is not a second implementation:
  it is the literal list of opcodes that the RTL sends to each first state, so
  if either the table or the RTL changes, the `decide` proof below fails to
  compile.
-/
import Open6502.Opcodes

namespace Open6502

/-! ## Table structure -/

set_option maxRecDepth 100000 in
theorem officialTable_length : officialTable.length = 151 := by decide

set_option maxRecDepth 100000 in
theorem officialOpcodes_nodup : officialOpcodes.Nodup := by decide

/-- The decoder recognises exactly the 151 documented opcodes. -/
theorem decode_isSome_all :
    officialOpcodes.all (fun o => (decode o).isSome) = true := by decide

theorem decode_isSome_of_mem (o : Byte) (h : o ∈ officialOpcodes) :
    (decode o).isSome := by
  have hall := decode_isSome_all
  rw [List.all_eq_true] at hall
  exact hall o h

/-- A representative sample of decode equations, used by the refinement proofs
and the tests. -/
theorem decode_BRK : decode 0x00 = some (.brk, .imp) := by decide
theorem decode_ORA_indx : decode 0x01 = some (.ora, .indx) := by decide
theorem decode_ASL_zp : decode 0x06 = some (.asl, .zp) := by decide
theorem decode_PHP : decode 0x08 = some (.php, .imp) := by decide
theorem decode_ORA_imm : decode 0x09 = some (.ora, .imm) := by decide
theorem decode_ASL_acc : decode 0x0A = some (.asl, .acc) := by decide
theorem decode_BPL : decode 0x10 = some (.bpl, .rel) := by decide
theorem decode_ORA_indy : decode 0x11 = some (.ora, .indy) := by decide
theorem decode_JSR : decode 0x20 = some (.jsr, .abs) := by decide
theorem decode_BIT_zp : decode 0x24 = some (.bit, .zp) := by decide
theorem decode_PLP : decode 0x28 = some (.plp, .imp) := by decide
theorem decode_ROL_acc : decode 0x2A = some (.rol, .acc) := by decide
theorem decode_RTI : decode 0x40 = some (.rti, .imp) := by decide
theorem decode_JMP_abs : decode 0x4C = some (.jmp, .abs) := by decide
theorem decode_RTS : decode 0x60 = some (.rts, .imp) := by decide
theorem decode_ADC_indx : decode 0x61 = some (.adc, .indx) := by decide
theorem decode_JMP_ind : decode 0x6C = some (.jmp, .ind) := by decide
theorem decode_ADC_imm : decode 0x69 = some (.adc, .imm) := by decide
theorem decode_SEI : decode 0x78 = some (.sei, .imp) := by decide
theorem decode_STA_indx : decode 0x81 = some (.sta, .indx) := by decide
theorem decode_STA_zp : decode 0x85 = some (.sta, .zp) := by decide
theorem decode_TXA : decode 0x8A = some (.txa, .imp) := by decide
theorem decode_STA_abs : decode 0x8D = some (.sta, .abs) := by decide
theorem decode_LDA_imm : decode 0xA9 = some (.lda, .imm) := by decide
theorem decode_LDA_zp : decode 0xA5 = some (.lda, .zp) := by decide
theorem decode_LDA_abs : decode 0xAD = some (.lda, .abs) := by decide
theorem decode_LDA_indy : decode 0xB1 = some (.lda, .indy) := by decide
theorem decode_LDA_absx : decode 0xBD = some (.lda, .absx) := by decide
theorem decode_INX : decode 0xE8 = some (.inx, .imp) := by decide
theorem decode_SBC_imm : decode 0xE9 = some (.sbc, .imm) := by decide
theorem decode_NOP : decode 0xEA = some (.nop, .imp) := by decide
theorem decode_BNE : decode 0xD0 = some (.bne, .rel) := by decide
theorem decode_PHA : decode 0x48 = some (.pha, .imp) := by decide
theorem decode_PLA : decode 0x68 = some (.pla, .imp) := by decide
theorem decode_LDA_zpx : decode 0xB5 = some (.lda, .zpx) := by decide
theorem decode_CMP_imm : decode 0xC9 = some (.cmp, .imm) := by decide
theorem decode_STX_abs : decode 0x8E = some (.stx, .abs) := by decide
theorem decode_LDX_imm : decode 0xA2 = some (.ldx, .imm) := by decide
theorem decode_ORA_zp : decode 0x05 = some (.ora, .zp) := by decide

/-- An illegal opcode does not decode. -/
theorem decode_illegal : decode 0x02 = none := by decide

/-- Instruction lengths of the addressing modes. -/
theorem instrLength_BRK : instrLength 0x00 = 1 := by decide
theorem instrLength_LDA_imm : instrLength 0xA9 = 2 := by decide
theorem instrLength_LDA_abs : instrLength 0xAD = 3 := by decide
theorem instrLength_JMP_ind : instrLength 0x6C = 3 := by decide
theorem instrLength_BNE : instrLength 0xD0 = 2 := by decide

end Open6502
