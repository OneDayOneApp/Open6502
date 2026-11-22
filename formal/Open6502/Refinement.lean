/-
  Refinement: the cycle-level model implements the ISA model.

  For each verified instruction we prove, for an arbitrary machine state, that
  running the FSM for the instruction's (RTL) cycle count and projecting the
  architectural registers yields exactly the state that the ISA-level `step`
  produces.  No simulation, no bounded model checking: the quantifiers range
  over all memory images and all register values.

  The cycle counts are the number of `mstep`s in the RTL FSM, which for these
  instructions equals the documented base cycle count.  (Page-cross penalties
  and the extra dummy cycles of some stack instructions are not implemented by
  the current RTL; see `formal/README.md` for the list of known deviations.)
-/
import Open6502.Micro
import Open6502.ISA
import Open6502.Decode

namespace Open6502

set_option linter.unusedSimpArgs false

/-- Architectural view of a microarchitectural state. -/
def toMachine (m : MState) : Machine :=
  { regs := { pc := m.pc, a := m.a, x := m.x, y := m.y, sp := m.sp, p := m.p },
    mem := m.mem }

/-- A stack write never clobbers the reset/IRQ vector page. -/
theorem stackAddr_ne_FFFE (sp : Byte) : stackAddr sp ≠ 0xFFFE := by
  simp only [stackAddr, wordOf, zext]
  bv_decide

theorem ne_FFFE_stackAddr (sp : Byte) : 0xFFFE ≠ stackAddr sp :=
  fun h => stackAddr_ne_FFFE sp h.symm

/-- The vector high byte is read at `#$FFFE + 1`. -/
theorem VecLo_succ : (0xFFFE : Word) + 1 = 0xFFFF := by decide

/-- The two spellings of the indirect-JMP page-wrap. -/
theorem pageWrapInc_eq (a : Word) : pageWrapInc a = wordOf (lo a + 1) (hi a) := by
  simp only [pageWrapInc, wordOf, lo, hi, zext]
  bv_decide

/-- LDA #imm (2 cycles). -/
theorem lda_imm_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0xA9) (hnmi : m.nmiPending = false) :
    toMachine (mrun 2 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.lda, .imm) := by
    rw [hop]; exact decode_LDA_imm
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_A9, decode_LDA_imm]

/-- STA $hhhh (4 cycles). -/
theorem sta_abs_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0x8D) (hnmi : m.nmiPending = false) :
    toMachine (mrun 4 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.sta, .abs) := by
    rw [hop]; exact decode_STA_abs
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_8D, decode_STA_abs]

/-- LDA $nn (3 cycles). -/
theorem lda_zp_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0xA5) (hnmi : m.nmiPending = false) :
    toMachine (mrun 3 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.lda, .zp) := by
    rw [hop]; exact decode_LDA_zp
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_A5, decode_LDA_zp]

/-- LDA $hhhh (4 cycles). -/
theorem lda_abs_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0xAD) (hnmi : m.nmiPending = false) :
    toMachine (mrun 4 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.lda, .abs) := by
    rw [hop]; exact decode_LDA_abs
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_AD, decode_LDA_abs]

/-- LDA $hhhh,X (4 cycles). -/
theorem lda_absx_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0xBD) (hnmi : m.nmiPending = false) :
    toMachine (mrun 4 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.lda, .absx) := by
    rw [hop]; exact decode_LDA_absx
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_BD, decode_LDA_absx]

/-- LDA ($nn),Y (5 cycles). -/
theorem lda_indy_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0xB1) (hnmi : m.nmiPending = false) :
    toMachine (mrun 5 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.lda, .indy) := by
    rw [hop]; exact decode_LDA_indy
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_B1, decode_LDA_indy]

/-- ADC #imm (2 cycles). -/
theorem adc_imm_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0x69) (hnmi : m.nmiPending = false) :
    toMachine (mrun 2 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.adc, .imm) := by
    rw [hop]; exact decode_ADC_imm
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_69, decode_ADC_imm]

/-- CMP #imm (2 cycles). -/
theorem cmp_imm_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0xC9) (hnmi : m.nmiPending = false) :
    toMachine (mrun 2 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.cmp, .imm) := by
    rw [hop]; exact decode_CMP_imm
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_C9, decode_CMP_imm]

/-- INX (2 cycles). -/
theorem inx_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0xE8) (hnmi : m.nmiPending = false) :
    toMachine (mrun 2 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.inx, .imp) := by
    rw [hop]; exact decode_INX
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_E8, decode_INX]

/-- PHA (2 cycles). -/
theorem pha_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0x48) (hnmi : m.nmiPending = false) :
    toMachine (mrun 2 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.pha, .imp) := by
    rw [hop]; exact decode_PHA
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_48, decode_PHA]

/-- PLA (2 cycles). -/
theorem pla_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0x68) (hnmi : m.nmiPending = false) :
    toMachine (mrun 2 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.pla, .imp) := by
    rw [hop]; exact decode_PLA
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_68, decode_PLA]

/-- JMP $hhhh (3 cycles). -/
theorem jmp_abs_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0x4C) (hnmi : m.nmiPending = false) :
    toMachine (mrun 3 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.jmp, .abs) := by
    rw [hop]; exact decode_JMP_abs
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_4C, decode_JMP_abs]

/-- JMP ($hhhh) (5 cycles). -/
theorem jmp_ind_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0x6C) (hnmi : m.nmiPending = false) :
    toMachine (mrun 5 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.jmp, .ind) := by
    rw [hop]; exact decode_JMP_ind
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_6C, decode_JMP_ind, pageWrapInc_eq]

/-- JSR $hhhh (6 cycles). -/
theorem jsr_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0x20) (hnmi : m.nmiPending = false) :
    toMachine (mrun 6 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.jsr, .abs) := by
    rw [hop]; exact decode_JSR
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_20, decode_JSR]

/-- RTS (3 cycles). -/
theorem rts_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0x60) (hnmi : m.nmiPending = false) :
    toMachine (mrun 3 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.rts, .imp) := by
    rw [hop]; exact decode_RTS
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_60, decode_RTS]

/-- BNE (taken) (3 cycles). -/
theorem bne_taken_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0xD0) (hnmi : m.nmiPending = false) (hz : Zf m.p = false) :
    toMachine (mrun 3 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.bne, .rel) := by
    rw [hop]; exact decode_BNE
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_D0, decode_BNE, hz]

/-- BNE (not taken) (3 cycles). -/
theorem bne_not_taken_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0xD0) (hnmi : m.nmiPending = false) (hz : Zf m.p = true) :
    toMachine (mrun 3 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.bne, .rel) := by
    rw [hop]; exact decode_BNE
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_D0, decode_BNE, hz]

/-- BRK (6 cycles). -/
theorem brk_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0x00) (hnmi : m.nmiPending = false) :
    toMachine (mrun 6 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.brk, .imp) := by
    rw [hop]; exact decode_BRK
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_00, decode_BRK, read16, wordOf, VecLo_succ,
    stackAddr_ne_FFFE, ne_FFFE_stackAddr]

/-- RTI (4 cycles). -/
theorem rti_refine (m : MState) (hst : m.st = .fetch) (hop : m.mem m.pc = 0x40) (hnmi : m.nmiPending = false) :
    toMachine (mrun 4 m) = step (toMachine m) := by
  have hdec : decode (m.mem m.pc) = some (.rti, .imp) := by
    rw [hop]; exact decode_RTI
  simp only [mrun, mstep, hst, hnmi, hop, busAddr, busWe, busWdata, isStore, decodeOp,
    Bool.not_true, Bool.false_and, Bool.true_and, Bool.false_eq_true,
    ite_false, ite_true, Bool.not_false, branchTo, toMachine, step, exec, calcOperand,
    push, pull, stackAddr,
    reduceCtorEq,
    hdec, dispatchState_40, decode_RTI]

end Open6502
