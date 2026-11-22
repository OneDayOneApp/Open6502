/-
  Open6502 formal model — basic types and bit helpers.

  The model deliberately mirrors the SystemVerilog in `rtl/cpu/open6502_cpu.sv`:
  every register is an explicitly sized `BitVec`, the processor status register
  stays a single byte (`p`), and arithmetic is defined with the same 9-bit and
  signed-6-bit temporaries the RTL uses.  This is the "typed hardware
  representation" half of the CktLean method: widths and signedness are part of
  the Lean types, so an accidental 16-bit/8-bit mix-up cannot typecheck.
-/
import Std

namespace Open6502

/-- One 6502 data bus beat. -/
abbrev Byte := BitVec 8

/-- One 6502 address. -/
abbrev Word := BitVec 16

/-! ## Bit access -/

/-- `bitAt x n` is bit `n` of `x` (zero for `n ≥ width`). -/
def bitAt {w : Nat} (x : BitVec w) (n : Nat) : Bool :=
  ((x >>> n) &&& 1) != 0

/-- Most significant bit, the 6502's sign bit. -/
def msb (b : Byte) : Bool := bitAt b 7

/-- Least significant bit. -/
def lsb (b : Byte) : Bool := bitAt b 0

/-! ## Width conversion

We spell the conversions with `setWidth` and `ofNat` rather than relying on
coercions, so the generated semantics stay readable in proof goals. -/

/-- Zero-extend a byte to an address. -/
def zext (b : Byte) : Word := b.setWidth 16

/-- Sign-extend a byte to an address (branch displacement). -/
def sext (b : Byte) : Word := BitVec.ofNat 16 (b.toNat + 0xFF00)

/-- Low byte of an address. -/
def lo (w : Word) : Byte := w.setWidth 8

/-- High byte of an address. -/
def hi (w : Word) : Byte := (w >>> 8).setWidth 8

/-- Little-endian pair. -/
def wordOf (l h : Byte) : Word := zext l ||| (zext h <<< 8)

/-- Little-endian 16-bit read.  A read at `#$FFFF` wraps to `#$0000` for the
second byte, exactly like the hardware. -/
def read16 (mem : Word → Byte) (addr : Word) : Word :=
  wordOf (mem addr) (mem (addr + 1))

/-- The NMOS 6502's famous indirect-JMP page-wrap: incrementing the low byte of
an address never carries into the high byte. -/
def pageWrapInc (addr : Word) : Word :=
  (addr &&& 0xFF00) ||| zext (lo addr + 1)

/-- Address of a stack slot for stack pointer `sp` (`#$0100 + sp`). -/
def stackAddr (sp : Byte) : Word := wordOf sp 0x01

/-! ## Processor status register

`p` is a byte, like the RTL register.  These accessors give the individual
flags names in specifications. -/

/-- Carry. -/
def Cf (p : Byte) : Bool := bitAt p 0
/-- Zero. -/
def Zf (p : Byte) : Bool := bitAt p 1
/-- IRQ disable. -/
def If (p : Byte) : Bool := bitAt p 2
/-- Decimal. -/
def Df (p : Byte) : Bool := bitAt p 3
/-- Break (only meaningful in pushed copies of `p`). -/
def Bf (p : Byte) : Bool := bitAt p 4
/-- Unused, always one in pushed copies. -/
def Uf (p : Byte) : Bool := bitAt p 5
/-- Overflow. -/
def Vf (p : Byte) : Bool := bitAt p 6
/-- Negative. -/
def Nf (p : Byte) : Bool := bitAt p 7

/-- `boolByte b` is `1` when `b` is true and `0` otherwise. -/
def boolByte (b : Bool) : Byte := if b then 1 else 0

/-- Replace bit `n` of `b`. -/
def setBit (b : Byte) (n : Nat) (v : Bool) : Byte :=
  if v then b ||| ((1 : Byte) <<< n) else b &&& ~~~((1 : Byte) <<< n)

/-- Set the carry flag. -/
def setC (p : Byte) (v : Bool) : Byte := (p &&& 0xFE) ||| boolByte v
/-- Set the interrupt-disable flag. -/
def setI (p : Byte) (v : Bool) : Byte := (p &&& 0xFB) ||| (boolByte v <<< 2)
/-- Set the decimal flag. -/
def setD (p : Byte) (v : Bool) : Byte := (p &&& 0xF7) ||| (boolByte v <<< 3)
/-- Set the overflow flag. -/
def setV (p : Byte) (v : Bool) : Byte := (p &&& 0xBF) ||| (boolByte v <<< 6)
/-- Clear the break bit (only used for pushed status bytes). -/
def clearB (p : Byte) : Byte := p &&& 0xEF

/-- Update N and Z from `v`, preserving every other flag.  This is the RTL's
`nz` function: mask `0x7D` keeps all flags but N/Z, then OR in the new ones. -/
def nz (v old : Byte) : Byte :=
  (old &&& 0x7D) ||| (if v == 0 then 0x02 else 0x00) ||| (if msb v then 0x80 else 0x00)

/-- Unsigned-carry-free carry/borrow update used by ADC/SBC: keep the flags
other than C and V, then insert both. -/
def setCV (p : Byte) (c v : Bool) : Byte :=
  (p &&& 0xBE) ||| boolByte c ||| (boolByte v <<< 6)

/-- Signed overflow of an addition: both inputs the same sign and the result a
different sign. -/
def overflowAdd (a m r : Byte) : Bool :=
  (msb a == msb m) && (msb a != msb r)

/-- Signed overflow of a subtraction: inputs of different signs and the result
sign different from the minuend. -/
def overflowSub (a m r : Byte) : Bool :=
  (msb a != msb m) && (msb a != msb r)

/-! ## Memory -/

/-- A 64 KiB address space. -/
abbrev Mem := Word → Byte

/-- Functional memory update. -/
def Mem.write (m : Mem) (addr : Word) (v : Byte) : Mem :=
  fun x => if x = addr then v else m x

@[simp] theorem Mem.write_same (m : Mem) (addr : Word) (v : Byte) :
    Mem.write m addr v addr = v := by
  simp [Mem.write]

theorem Mem.write_other (m : Mem) {addr other : Word} (h : other ≠ addr) (v : Byte) :
    Mem.write m addr v other = m other := by
  simp [Mem.write, h]

/-! ## Binary-coded decimal -/

/-- Low nibble. -/
def nibLo (b : Byte) : Byte := b &&& 0x0F
/-- High nibble. -/
def nibHi (b : Byte) : Byte := b >>> 4

/-- A byte is a valid packed BCD digit pair when both nibbles are ≤ 9. -/
def bcdValid (b : Byte) : Bool :=
  BitVec.ult (nibLo b) 10 && BitVec.ult (nibHi b) 10

/-- Numeric value of a packed BCD byte.  Only meaningful together with
`bcdValid`. -/
def bcdVal (b : Byte) : Byte := nibHi b * 10 + nibLo b

end Open6502
