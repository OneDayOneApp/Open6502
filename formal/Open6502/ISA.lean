/-
  Open6502 instruction-set architecture (ISA) model.

  This is the *specification* side of the refinement proof: a single-instruction
  abstract machine whose state is the architectural state a programmer sees
  (`pc`, `a`, `x`, `y`, `sp`, `p`, and a 64 KiB byte-addressed memory).  It is
  written in the same style as the RTL: every address calculation is explicit,
  including the NMOS zero-page wrap and the indirect-JMP page-wrap quirk.

  Illegal opcodes are handled as the RTL handles them: the fetch advances `pc`
  by one and nothing else changes.
-/
import Open6502.Basic
import Open6502.Opcodes
import Open6502.ALU

namespace Open6502

/-- Architectural register file. -/
structure Regs where
  pc : Word
  a : Byte
  x : Byte
  y : Byte
  sp : Byte
  p : Byte
  deriving Repr, DecidableEq, Inhabited

/-- Whole machine state. -/
structure Machine where
  regs : Regs
  mem : Mem
  deriving Inhabited

/-- Power-on / reset state of the RTL: the reset vector is read from `#$FFFC`,
the stack pointer starts at `#$FD` and `p` at `#$24` (I set, U set). -/
def reset (mem : Mem) : Machine :=
  { regs := { pc := read16 mem 0xFFFC, a := 0, x := 0, y := 0, sp := 0xFD, p := 0x24 }
    mem := mem }

/-! ## Stack -/

/-- Push a byte: write at `#$0100 + sp`, then decrement `sp` (the hardware
convention: `sp` points at the next free slot). -/
def push (s : Machine) (v : Byte) : Machine :=
  { s with mem := Mem.write s.mem (stackAddr s.regs.sp) v,
           regs := { s.regs with sp := s.regs.sp - 1 } }

/-- Pull a byte: read at `#$0100 + sp`, then increment `sp`. -/
def pull (s : Machine) : Byte × Machine :=
  let v := s.mem (stackAddr (s.regs.sp + 1))
  (v, { s with regs := { s.regs with sp := s.regs.sp + 1 } })

/-! ## Effective addresses -/

/-- Result of decoding the addressing mode of one instruction.  `value` is the
operand byte (the immediate for `.imm`, the accumulator for `.acc`), `addr` is
the effective address, and `next` is the machine with `pc` advanced past the
operand bytes. -/
structure Operand where
  value : Byte
  addr : Word
  next : Machine

/-- Fetch the operand and compute the effective address for `m`. -/
def calcOperand (s : Machine) (m : Mode) : Operand :=
  let pc := s.regs.pc
  -- Operand bytes are read at `pc + 1` and `pc + 2`; `pc` itself points at the
  -- opcode, which `step` has already decoded.
  let b0 := s.mem (pc + 1)
  let b1 := s.mem (pc + 1 + 1)
  match m with
  | .imp | .acc =>
      { value := s.regs.a, addr := 0, next := { s with regs := { s.regs with pc := pc + 1 } } }
  | .rel =>
      { value := b0, addr := pc + 1 + 1 + sext b0,
        next := { s with regs := { s.regs with pc := pc + 1 + 1 } } }
  | .imm =>
      { value := b0, addr := pc + 1, next := { s with regs := { s.regs with pc := pc + 1 + 1 } } }
  | .zp =>
      { value := s.mem (zext b0), addr := zext b0,
        next := { s with regs := { s.regs with pc := pc + 1 + 1 } } }
  | .zpx =>
      let a := zext (b0 + s.regs.x)
      { value := s.mem a, addr := a,
        next := { s with regs := { s.regs with pc := pc + 1 + 1 } } }
  | .zpy =>
      let a := zext (b0 + s.regs.y)
      { value := s.mem a, addr := a,
        next := { s with regs := { s.regs with pc := pc + 1 + 1 } } }
  | .abs =>
      let a := wordOf b0 b1
      { value := s.mem a, addr := a,
        next := { s with regs := { s.regs with pc := pc + 1 + 1 + 1 } } }
  | .absx =>
      let a := wordOf b0 b1 + zext s.regs.x
      { value := s.mem a, addr := a,
        next := { s with regs := { s.regs with pc := pc + 1 + 1 + 1 } } }
  | .absy =>
      let a := wordOf b0 b1 + zext s.regs.y
      { value := s.mem a, addr := a,
        next := { s with regs := { s.regs with pc := pc + 1 + 1 + 1 } } }
  | .ind =>
      let ptr := wordOf b0 b1
      let a := wordOf (s.mem ptr) (s.mem (pageWrapInc ptr))
      { value := s.mem a, addr := a,
        next := { s with regs := { s.regs with pc := pc + 1 + 1 + 1 } } }
  | .indx =>
      let ptr := b0 + s.regs.x
      let a := wordOf (s.mem (zext ptr)) (s.mem (zext (ptr + 1)))
      { value := s.mem a, addr := a,
        next := { s with regs := { s.regs with pc := pc + 1 + 1 } } }
  | .indy =>
      let ptr := b0
      let base := wordOf (s.mem (zext ptr)) (s.mem (zext (ptr + 1)))
      let a := base + zext s.regs.y
      { value := s.mem a, addr := a,
        next := { s with regs := { s.regs with pc := pc + 1 + 1 } } }

/-- Conditional branch helper: either keep the advanced `pc` or take the
target. -/
def branchTo (s : Machine) (taken : Bool) (target : Word) : Machine :=
  if taken then { s with regs := { s.regs with pc := target } } else s

/-- Execute one decoded instruction. -/
def exec (op : Op) (m : Mode) (s : Machine) : Machine :=
  let o := calcOperand s m
  let r := o.next.regs
  match op with
  | .lda => { o.next with regs := { r with a := o.value, p := nz o.value r.p } }
  | .ldx => { o.next with regs := { r with x := o.value, p := nz o.value r.p } }
  | .ldy => { o.next with regs := { r with y := o.value, p := nz o.value r.p } }
  | .sta => { o.next with mem := Mem.write o.next.mem o.addr r.a }
  | .stx => { o.next with mem := Mem.write o.next.mem o.addr r.x }
  | .sty => { o.next with mem := Mem.write o.next.mem o.addr r.y }
  | .ora => { o.next with regs := { r with a := r.a ||| o.value, p := nz (r.a ||| o.value) r.p } }
  | .and_ => { o.next with regs := { r with a := r.a &&& o.value, p := nz (r.a &&& o.value) r.p } }
  | .eor => { o.next with regs := { r with a := r.a ^^^ o.value, p := nz (r.a ^^^ o.value) r.p } }
  | .adc =>
      let x := adc r.a o.value r.p
      { o.next with regs := { r with a := x.1, p := x.2 } }
  | .sbc =>
      let x := sbc r.a o.value r.p
      { o.next with regs := { r with a := x.1, p := x.2 } }
  | .cmp => { o.next with regs := { r with p := cmp r.a o.value r.p } }
  | .cpx => { o.next with regs := { r with p := cmp r.x o.value r.p } }
  | .cpy => { o.next with regs := { r with p := cmp r.y o.value r.p } }
  | .bit => { o.next with regs := { r with p := bitOp r.a o.value r.p } }
  | .asl | .lsr | .rol | .ror =>
      let f : Byte → Byte → Byte × Byte :=
        match op with
        | .asl => asl
        | .lsr => lsr
        | .rol => rol
        | _ => ror
      if m = .acc then
        let v := f r.a r.p
        { o.next with regs := { r with a := v.1, p := v.2 } }
      else
        let v := f o.value r.p
        { o.next with regs := { r with p := v.2 },
                       mem := Mem.write o.next.mem o.addr v.1 }
  | .inc =>
      let v := inc o.value r.p
      { o.next with regs := { r with p := v.2 },
                     mem := Mem.write o.next.mem o.addr v.1 }
  | .dec =>
      let v := dec o.value r.p
      { o.next with regs := { r with p := v.2 },
                     mem := Mem.write o.next.mem o.addr v.1 }
  | .tax => { o.next with regs := { r with x := r.a, p := nz r.a r.p } }
  | .tay => { o.next with regs := { r with y := r.a, p := nz r.a r.p } }
  | .txa => { o.next with regs := { r with a := r.x, p := nz r.x r.p } }
  | .tya => { o.next with regs := { r with a := r.y, p := nz r.y r.p } }
  | .tsx => { o.next with regs := { r with x := r.sp, p := nz r.sp r.p } }
  | .txs => { o.next with regs := { r with sp := r.x } }
  | .inx => { o.next with regs := { r with x := r.x + 1, p := nz (r.x + 1) r.p } }
  | .iny => { o.next with regs := { r with y := r.y + 1, p := nz (r.y + 1) r.p } }
  | .dex => { o.next with regs := { r with x := r.x - 1, p := nz (r.x - 1) r.p } }
  | .dey => { o.next with regs := { r with y := r.y - 1, p := nz (r.y - 1) r.p } }
  | .clc => { o.next with regs := { r with p := setC r.p false } }
  | .sec => { o.next with regs := { r with p := setC r.p true } }
  | .cli => { o.next with regs := { r with p := setI r.p false } }
  | .sei => { o.next with regs := { r with p := setI r.p true } }
  | .clv => { o.next with regs := { r with p := setV r.p false } }
  | .cld => { o.next with regs := { r with p := setD r.p false } }
  | .sed => { o.next with regs := { r with p := setD r.p true } }
  | .nop => o.next
  | .pha => push o.next o.next.regs.a
  | .php => push o.next (o.next.regs.p ||| 0x30)
  | .pla =>
      let x := pull o.next
      { x.2 with regs := { x.2.regs with a := x.1, p := nz x.1 x.2.regs.p } }
  | .plp =>
      let x := pull o.next
      { x.2 with regs := { x.2.regs with p := (x.1 &&& 0xCF) ||| 0x20 } }
  | .jmp => { o.next with regs := { o.next.regs with pc := o.addr } }
  | .jsr =>
      let ret := o.next.regs.pc - 1
      let s1 := push o.next (hi ret)
      let s2 := push s1 (lo ret)
      { s2 with regs := { s2.regs with pc := o.addr } }
  | .rts =>
      let x1 := pull o.next
      let x2 := pull x1.2
      { x2.2 with regs := { x2.2.regs with pc := wordOf x1.1 x2.1 + 1 } }
  | .rti =>
      let x1 := pull o.next
      let x2 := pull x1.2
      let x3 := pull x2.2
      { x3.2 with regs := { x3.2.regs with
          p := (x1.1 &&& 0xCF) ||| 0x20, pc := wordOf x2.1 x3.1 } }
  | .brk =>
      let ret := o.next.regs.pc + 1
      let s1 := push o.next (hi ret)
      let s2 := push s1 (lo ret)
      let s3 := push s2 (o.next.regs.p ||| 0x30)
      { s3 with regs := { s3.regs with
          p := s3.regs.p ||| 0x04, pc := read16 s3.mem 0xFFFE } }
  | .bcc => branchTo o.next (!Cf r.p) o.addr
  | .bcs => branchTo o.next (Cf r.p) o.addr
  | .bne => branchTo o.next (!Zf r.p) o.addr
  | .beq => branchTo o.next (Zf r.p) o.addr
  | .bpl => branchTo o.next (!Nf r.p) o.addr
  | .bmi => branchTo o.next (Nf r.p) o.addr
  | .bvc => branchTo o.next (!Vf r.p) o.addr
  | .bvs => branchTo o.next (Vf r.p) o.addr

/-- Execute one instruction. -/
def step (s : Machine) : Machine :=
  match decode (s.mem s.regs.pc) with
  | some (op, m) => exec op m s
  | none => { s with regs := { s.regs with pc := s.regs.pc + 1 } }

/-- Run `n` instructions. -/
def run : Nat → Machine → Machine
  | 0, s => s
  | n + 1, s => run n (step s)

/-- Run until `pc` stops changing (a simple trap detector for the ROM tests). -/
def runUntilTrap (fuel : Nat) (s : Machine) : Machine :=
  match fuel with
  | 0 => s
  | n + 1 =>
      let s' := step s
      if s'.regs.pc = s.regs.pc then s' else runUntilTrap n s'

end Open6502
