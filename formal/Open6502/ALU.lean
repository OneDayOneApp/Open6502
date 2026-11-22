/-
  Open6502 ALU.

  Two layers:

  * `adcBinary`/`sbcBinary`/`adcDecimal`/`sbcDecimal` are bit-accurate copies of
    the combinational block in `rtl/cpu/open6502_cpu.sv`.
  * `adcValue`/`adcStatus`/`sbcValue`/`sbcStatus`/`cmp`/`asl`/... are the
    instruction-level operations: they also update the processor status byte
    `p`, exactly like the RTL's `case (opcode)` arms.

  The theorems below are the functional specification: for every 8-bit operand
  pair and every carry input, the implementation equals the mathematical
  definition (wrapping binary arithmetic, signed overflow, BCD arithmetic).
  All are closed by `bv_decide`, Lean's bit-blasting decision procedure (its
  per-goal certificate is checked by `Std.Tactic.BVDecide.Reflect.verifyBVExpr`,
  so `#print axioms` shows a `bv_decide.ax` trust point for these lemmas); no
  `native_decide` or `sorry` is used anywhere in this file.
-/
import Open6502.Basic

namespace Open6502

set_option linter.unusedSimpArgs false

/-! ## Binary addition -/

/-- Nine-bit sum used by the RTL so the carry out is visible. -/
def sum9 (a m : Byte) (cin : Bool) : BitVec 9 :=
  BitVec.setWidth 9 a + BitVec.setWidth 9 m + (if cin then 1 else 0)

/-- Binary ADC result (low eight bits). -/
def adcBinary (a m : Byte) (cin : Bool) : Byte := BitVec.setWidth 8 (sum9 a m cin)

/-- Binary ADC carry out. -/
def adcBinaryCarry (a m : Byte) (cin : Bool) : Bool := bitAt (sum9 a m cin) 8

/-- The mathematical carry-out condition: the exact sum, widened to 16 bits,
is at least 256. -/
def adcCarrySpec (a m : Byte) (cin : Bool) : Bool :=
  BitVec.ule 256 (BitVec.setWidth 16 a + BitVec.setWidth 16 m + (if cin then 1 else 0))

theorem adcBinary_spec (a m : Byte) (cin : Bool) :
    adcBinary a m cin = a + m + (if cin then 1 else 0) := by
  simp only [adcBinary, sum9]
  bv_decide

theorem adcBinaryCarry_spec (a m : Byte) (cin : Bool) :
    adcBinaryCarry a m cin = adcCarrySpec a m cin := by
  simp only [adcBinaryCarry, adcCarrySpec, sum9, bitAt]
  bv_decide

/-- Signed overflow of an addition, the definition hardware implements. -/
theorem overflowAdd_spec (a m r : Byte) :
    overflowAdd a m r =
      (BitVec.slt a 0 == BitVec.slt m 0 && (BitVec.slt r 0 != BitVec.slt a 0)) := by
  simp only [overflowAdd, msb, bitAt]
  bv_decide

/-! ## Binary subtraction -/

/-- Nine-bit difference; bit 8 is the borrow. -/
def diff9 (a m : Byte) (cin : Bool) : BitVec 9 :=
  BitVec.setWidth 9 a - BitVec.setWidth 9 m - (if cin then 0 else 1)

/-- Binary SBC result (low eight bits). -/
def sbcBinary (a m : Byte) (cin : Bool) : Byte := BitVec.setWidth 8 (diff9 a m cin)

/-- Binary SBC carry out.  The 6502 defines carry as *not borrow*. -/
def sbcBinaryCarry (a m : Byte) (cin : Bool) : Bool := !bitAt (diff9 a m cin) 8

/-- Carry condition of `a - m - (1 - cin)`, stated as a 16-bit comparison. -/
def sbcCarrySpec (a m : Byte) (cin : Bool) : Bool :=
  BitVec.ule (BitVec.setWidth 16 m + (if cin then 0 else 1)) (BitVec.setWidth 16 a)

theorem sbcBinary_spec (a m : Byte) (cin : Bool) :
    sbcBinary a m cin = a - m - (if cin then 0 else 1) := by
  simp only [sbcBinary, diff9]
  bv_decide

theorem sbcBinaryCarry_spec (a m : Byte) (cin : Bool) :
    sbcBinaryCarry a m cin = sbcCarrySpec a m cin := by
  simp only [sbcBinaryCarry, sbcCarrySpec, diff9, bitAt]
  bv_decide

theorem overflowSub_spec (a m r : Byte) :
    overflowSub a m r =
      (BitVec.slt a 0 != BitVec.slt m 0 && (BitVec.slt r 0 != BitVec.slt a 0)) := by
  simp only [overflowSub, msb, bitAt]
  bv_decide

/-! ## Decimal (BCD) arithmetic

The RTL's decimal mode is the classic NMOS nibble algorithm: add the low
nibbles, add six if the result exceeds nine, carry into the high nibble, and
repeat.  The specification below is arithmetic, not algorithmic: for valid BCD
inputs the result must be a valid BCD byte whose numeric value is the sum
modulo 100, and the carry must be set exactly when the sum reaches 100. -/

/-- Decimal ADC, mirroring the RTL's 5-bit nibble temporaries. -/
def adcDecimal (a m : Byte) (cin : Bool) : Byte × Bool :=
  let loC : BitVec 5 := BitVec.setWidth 5 (nibLo a) + BitVec.setWidth 5 (nibLo m)
                      + (if cin then 1 else 0)
  let hi0 : BitVec 5 := BitVec.setWidth 5 (nibHi a) + BitVec.setWidth 5 (nibHi m)
  let lo1 : BitVec 5 := if loC > 9 then loC + 6 else loC
  let hi1 : BitVec 5 := if loC > 9 then hi0 + 1 else hi0
  let hi2 : BitVec 5 := if hi1 > 9 then hi1 + 6 else hi1
  let r : Byte := (BitVec.setWidth 8 (hi2 &&& 0x0F) <<< 4)
                  ||| BitVec.setWidth 8 (lo1 &&& 0x0F)
  (r, hi2 > 15)

/-- Decimal SBC, mirroring the RTL's signed 6-bit temporaries.  The carry
returned is *not borrow*. -/
def sbcDecimal (a m : Byte) (cin : Bool) : Byte × Bool :=
  let borrow : Bool := !cin
  let slo : BitVec 6 := BitVec.setWidth 6 (nibLo a) - BitVec.setWidth 6 (nibLo m)
                        - (if borrow then 1 else 0)
  let shi0 : BitVec 6 := BitVec.setWidth 6 (nibHi a) - BitVec.setWidth 6 (nibHi m)
  let slo1 : BitVec 6 := if BitVec.slt slo 0 then slo - 6 else slo
  let shi1 : BitVec 6 := if BitVec.slt slo 0 then shi0 - 1 else shi0
  let shi2 : BitVec 6 := if BitVec.slt shi1 0 then shi1 - 6 else shi1
  let r : Byte := (BitVec.setWidth 8 (shi2 &&& 0x0F) <<< 4)
                  ||| BitVec.setWidth 8 (slo1 &&& 0x0F)
  (r, !BitVec.slt shi2 0)

/-- BCD sum of two valid bytes plus carry-in. -/
def bcdSum (a m : Byte) (cin : Bool) : Byte :=
  bcdVal a + bcdVal m + (if cin then 1 else 0)

/-- The carry flag of a decimal addition: the BCD sum reached 100. -/
def bcdSumCarry (a m : Byte) (cin : Bool) : Bool :=
  BitVec.ule 100 (bcdSum a m cin)

/-- The expected decimal result: the BCD sum reduced modulo 100. -/
def bcdSumResult (a m : Byte) (cin : Bool) : Byte :=
  if bcdSumCarry a m cin then bcdSum a m cin - 100 else bcdSum a m cin

theorem adcDecimal_valid (a m : Byte) (cin : Bool)
    (ha : bcdValid a = true) (hm : bcdValid m = true) :
    bcdValid (adcDecimal a m cin).1 = true := by
  simp only [bcdValid, nibLo, nibHi, Bool.and_eq_true] at ha hm
  simp only [adcDecimal, bcdValid, nibLo, nibHi]
  bv_decide

theorem adcDecimal_value (a m : Byte) (cin : Bool)
    (ha : bcdValid a = true) (hm : bcdValid m = true) :
    bcdVal (adcDecimal a m cin).1 = bcdSumResult a m cin := by
  simp only [bcdValid, nibLo, nibHi, Bool.and_eq_true] at ha hm
  simp only [adcDecimal, bcdVal, bcdSumResult, bcdSumCarry, bcdSum, nibLo, nibHi]
  bv_decide

theorem adcDecimal_carry (a m : Byte) (cin : Bool)
    (ha : bcdValid a = true) (hm : bcdValid m = true) :
    (adcDecimal a m cin).2 = bcdSumCarry a m cin := by
  simp only [bcdValid, nibLo, nibHi, Bool.and_eq_true] at ha hm
  simp only [adcDecimal, bcdSumCarry, bcdSum, bcdVal, nibLo, nibHi]
  bv_decide

/-- Decimal subtraction spec: `a - m - borrow`, corrected by +100 when it goes
negative. -/
def bcdDiff (a m : Byte) (cin : Bool) : Byte :=
  if BitVec.ule (bcdVal m + (if cin then 0 else 1)) (bcdVal a) then
    bcdVal a - bcdVal m - (if cin then 0 else 1)
  else
    bcdVal a - bcdVal m - (if cin then 0 else 1) + 100

theorem sbcDecimal_value (a m : Byte) (cin : Bool)
    (ha : bcdValid a = true) (hm : bcdValid m = true) :
    bcdVal (sbcDecimal a m cin).1 = bcdDiff a m cin := by
  simp only [bcdValid, nibLo, nibHi, Bool.and_eq_true] at ha hm
  simp only [sbcDecimal, bcdDiff, bcdVal, nibLo, nibHi]
  bv_decide

theorem sbcDecimal_carry (a m : Byte) (cin : Bool)
    (ha : bcdValid a = true) (hm : bcdValid m = true) :
    (sbcDecimal a m cin).2 =
      BitVec.ule (bcdVal m + (if cin then 0 else 1)) (bcdVal a) := by
  simp only [bcdValid, nibLo, nibHi, Bool.and_eq_true] at ha hm
  simp only [sbcDecimal, bcdVal, nibLo, nibHi]
  bv_decide

theorem sbcDecimal_valid (a m : Byte) (cin : Bool)
    (ha : bcdValid a = true) (hm : bcdValid m = true) :
    bcdValid (sbcDecimal a m cin).1 = true := by
  simp only [bcdValid, nibLo, nibHi, Bool.and_eq_true] at ha hm
  simp only [sbcDecimal, bcdValid, nibLo, nibHi]
  bv_decide

/-! ## Instruction-level operations (result and new status byte) -/

/-- The value ADC writes into `A`. -/
def adcValue (a m p : Byte) : Byte :=
  if Df p then (adcDecimal a m (Cf p)).1 else adcBinary a m (Cf p)

/-- The status byte produced by ADC.  N and Z come from the result (the RTL's
behaviour), V from the binary operands, C from the arithmetic carry. -/
def adcStatus (a m p : Byte) : Byte :=
  if Df p then
    setCV (nz (adcDecimal a m (Cf p)).1 p) (adcDecimal a m (Cf p)).2
      (overflowAdd a m (adcDecimal a m (Cf p)).1)
  else
    setCV (nz (adcBinary a m (Cf p)) p) (adcBinaryCarry a m (Cf p))
      (overflowAdd a m (adcBinary a m (Cf p)))

/-- ADC as one operation (value, status). -/
def adc (a m p : Byte) : Byte × Byte := (adcValue a m p, adcStatus a m p)

/-- The value SBC writes into `A`. -/
def sbcValue (a m p : Byte) : Byte :=
  if Df p then (sbcDecimal a m (Cf p)).1 else sbcBinary a m (Cf p)

/-- The status byte produced by SBC. -/
def sbcStatus (a m p : Byte) : Byte :=
  if Df p then
    setCV (nz (sbcDecimal a m (Cf p)).1 p) (sbcDecimal a m (Cf p)).2
      (overflowSub a m (sbcDecimal a m (Cf p)).1)
  else
    setCV (nz (sbcBinary a m (Cf p)) p) (sbcBinaryCarry a m (Cf p))
      (overflowSub a m (sbcBinary a m (Cf p)))

/-- SBC as one operation (value, status). -/
def sbc (a m p : Byte) : Byte × Byte := (sbcValue a m p, sbcStatus a m p)

/-- Compare: the register is untouched, N/Z describe `r - m`, C is r ≥ m. -/
def cmp (r m p : Byte) : Byte :=
  let d := diff9 r m true
  (nz (BitVec.setWidth 8 d) p &&& 0xFE) ||| boolByte (!bitAt d 8)

/-- Arithmetic shift left (also used for `ASL A`). -/
def asl (v p : Byte) : Byte × Byte :=
  let r := v <<< 1
  (r, (nz r p &&& 0xFE) ||| boolByte (bitAt v 7))

/-- Logical shift right. -/
def lsr (v p : Byte) : Byte × Byte :=
  let r := v >>> 1
  (r, (nz r p &&& 0xFE) ||| boolByte (bitAt v 0))

/-- Rotate left through carry. -/
def rol (v p : Byte) : Byte × Byte :=
  let r := (v <<< 1) ||| boolByte (Cf p)
  (r, (nz r p &&& 0xFE) ||| boolByte (bitAt v 7))

/-- Rotate right through carry. -/
def ror (v p : Byte) : Byte × Byte :=
  let r := (v >>> 1) ||| (boolByte (Cf p) <<< 7)
  (r, (nz r p &&& 0xFE) ||| boolByte (bitAt v 0))

/-- Increment. -/
def inc (v p : Byte) : Byte × Byte := (v + 1, nz (v + 1) p)

/-- Decrement. -/
def dec (v p : Byte) : Byte × Byte := (v - 1, nz (v - 1) p)

/-- BIT: N and V come from the memory operand, Z from `A & M`. -/
def bitOp (a m p : Byte) : Byte :=
  (p &&& 0x3D) ||| (m &&& 0xC0) ||| (if (a &&& m) == 0 then 0x02 else 0x00)

/-! ### Specifications of the instruction-level operations -/

theorem adcValue_binary (a m p : Byte) (hd : Df p = false) :
    adcValue a m p = a + m + (if Cf p then 1 else 0) := by
  simp only [adcValue, hd, ite_false, adcBinary, sum9]
  bv_decide

theorem sbcValue_binary (a m p : Byte) (hd : Df p = false) :
    sbcValue a m p = a - m - (if Cf p then 0 else 1) := by
  simp only [sbcValue, hd, ite_false, sbcBinary, diff9]
  bv_decide

/-- Every flag except N, Z, C, V is preserved by ADC. -/
theorem adc_preserves_flags (a m p : Byte) :
    adcStatus a m p &&& 0x3C = p &&& 0x3C := by
  unfold adcStatus
  split <;>
    simp only [setCV, nz, Cf, Df, bitAt, boolByte] <;>
    bv_decide

theorem sbc_preserves_flags (a m p : Byte) :
    sbcStatus a m p &&& 0x3C = p &&& 0x3C := by
  unfold sbcStatus
  split <;>
    simp only [setCV, nz, Cf, Df, bitAt, boolByte] <;>
    bv_decide

/-- CMP sets carry exactly when the register is unsigned-greater-or-equal. -/
theorem cmp_carry (r m p : Byte) :
    Cf (cmp r m p) = BitVec.ule m r := by
  simp only [cmp, Cf, bitAt, diff9, boolByte, nz, msb]
  bv_decide

/-- CMP sets Z exactly when the operands are equal. -/
theorem cmp_zero (r m p : Byte) :
    Zf (cmp r m p) = (r == m) := by
  simp only [cmp, Zf, bitAt, diff9, boolByte, nz]
  bv_decide

/-- CMP sets N from the sign of the difference. -/
theorem cmp_negative (r m p : Byte) :
    Nf (cmp r m p) = msb (r - m) := by
  simp only [cmp, Nf, bitAt, diff9, boolByte, nz, msb]
  bv_decide

/-- ASL result is multiplication by two. -/
theorem asl_result (v p : Byte) : (asl v p).1 = v + v := by
  simp only [asl]
  bv_decide

/-- ASL carry is the old bit 7. -/
theorem asl_carry (v p : Byte) : Cf (asl v p).2 = msb v := by
  simp only [asl, Cf, bitAt, msb, boolByte, nz]
  bv_decide

theorem lsr_result (v p : Byte) : (lsr v p).1 = v >>> 1 := by
  simp only [lsr]

theorem rol_result (v p : Byte) :
    (rol v p).1 = (v <<< 1) ||| boolByte (Cf p) := by
  simp only [rol]

theorem ror_result (v p : Byte) :
    (ror v p).1 = (v >>> 1) ||| (boolByte (Cf p) <<< 7) := by
  simp only [ror]

theorem inc_result (v p : Byte) : (inc v p).1 = v + 1 := by
  simp only [inc]

theorem dec_result (v p : Byte) : (dec v p).1 = v - 1 := by
  simp only [dec]

/-- BIT sets Z from `A & M`. -/
theorem bitOp_zero (a m p : Byte) : Zf (bitOp a m p) = ((a &&& m) == 0) := by
  simp only [bitOp, Zf, bitAt]
  bv_decide

/-- BIT copies bit 7 of the operand into N. -/
theorem bitOp_negative (a m p : Byte) : Nf (bitOp a m p) = msb m := by
  simp only [bitOp, Nf, bitAt, msb]
  bv_decide

/-- BIT copies bit 6 of the operand into V. -/
theorem bitOp_overflow (a m p : Byte) : Vf (bitOp a m p) = bitAt m 6 := by
  simp only [bitOp, Vf, bitAt]
  bv_decide

end Open6502
