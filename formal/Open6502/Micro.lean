/-
  Open6502 cycle-level model.

  `MState` is a field-for-field transcription of the registers in
  `rtl/cpu/open6502_cpu.sv`, and `mstep` is a transcription of its
  `always_ff @(posedge clk)` block, including the combinational
  `bus_addr`/`bus_wdata`/`bus_we` steering.  One `mstep` is one CPU cycle with
  `cpu_en` high and `rst_n` high.

  Memory writes are applied inside `mstep` the same way the SoC bus applies
  them on the clock edge, so a sequence of `mstep`s corresponds exactly to a
  sequence of RTL cycles.
-/
import Open6502.Basic
import Open6502.ALU
import Open6502.FSM

namespace Open6502

/-- Registers and FSM state of the CPU core. -/
structure MState where
  st : FState
  pc : Word
  ea : Word
  a : Byte
  x : Byte
  y : Byte
  sp : Byte
  p : Byte
  opcode : Byte
  op : Op
  tmpLo : Byte
  zpPtr : Byte
  rmwData : Byte
  nmiPrev : Bool
  nmiPending : Bool
  irqNmi : Bool
  mem : Mem
  deriving Inhabited

/-- Reset values, mirroring the `if (!rst_n)` arm. -/
def mreset (mem : Mem) : MState :=
  { st := .resetLo, pc := 0, ea := 0, a := 0, x := 0, y := 0, sp := 0xFD, p := 0x24
    opcode := 0, op := .nop, tmpLo := 0, zpPtr := 0, rmwData := 0
    nmiPrev := true, nmiPending := false, irqNmi := false, mem := mem }

/-- Decode an opcode to an instruction; illegal opcodes become `NOP`, which is
exactly how the RTL's implied catch-all behaves. -/
def decodeOp (o : Byte) : Op :=
  match decode o with
  | some (op, _) => op
  | none => .nop

/-- Does this instruction write memory from `ST_ABS_DATA`?  (`we`) -/
def isStore (op : Op) : Bool :=
  match op with
  | .sta | .stx | .sty => true
  | _ => false

/-- The `bus_addr` multiplexer. -/
def busAddr (s : MState) : Word :=
  match s.st with
  | .resetLo => 0xFFFC
  | .resetHi => 0xFFFD
  | .zp | .zpx | .zpy | .absxLo | .absxHi | .absyLo | .absyHi => s.pc
  | .indxPtr | .indyPtr => s.pc
  | .indxLo | .indxHi | .indyLo | .indyHi => zext s.zpPtr
  | .jsrPushHi | .jsrPushLo => wordOf s.sp 0x01
  | .rtsPullLo | .rtsPullHi => wordOf (s.sp + 1) 0x01
  | .pushA | .pushP | .brkPushHi | .brkPushLo | .brkPushP
  | .irqPushHi | .irqPushLo | .irqPushP => wordOf s.sp 0x01
  | .pullA | .pullP | .rtiPullP => wordOf (s.sp + 1) 0x01
  | .rtiPullLo | .rtiPullHi => wordOf (s.sp + 1) 0x01
  | .brkVecLo => 0xFFFE
  | .brkVecHi => 0xFFFF
  | .irqVecLo => if s.irqNmi then 0xFFFA else 0xFFFE
  | .irqVecHi => if s.irqNmi then 0xFFFB else 0xFFFF
  | .absData | .rmwWrite | .jmpIndReadLo => s.ea
  | .jmpIndReadHi => wordOf (lo s.ea + 1) (hi s.ea)
  | _ => s.pc

/-- The `bus_wdata` multiplexer. -/
def busWdata (s : MState) : Byte :=
  match s.st with
  | .jsrPushHi => hi (s.pc - 1)
  | .jsrPushLo => lo (s.pc - 1)
  | .pushP | .brkPushP => s.p ||| 0x30
  | .brkPushHi => hi (s.pc + 1)
  | .brkPushLo => lo (s.pc + 1)
  | .irqPushHi => hi s.pc
  | .irqPushLo => lo s.pc
  | .irqPushP => (s.p &&& 0xEF) ||| 0x20
  | .rmwWrite => s.rmwData
  | _ =>
      match s.op with
      | .stx => s.x
      | .sty => s.y
      | _ => s.a

/-- The write-enable multiplexer. -/
def busWe (s : MState) : Bool :=
  match s.st with
  | .jsrPushHi | .jsrPushLo | .pushA | .pushP | .rmwWrite
  | .brkPushHi | .brkPushLo | .brkPushP
  | .irqPushHi | .irqPushLo | .irqPushP => true
  | .absData => isStore s.op
  | _ => false

/-- One enabled CPU cycle. -/
def mstep (irq_n nmi_n : Bool) (s : MState) : MState :=
  let rd := s.mem (busAddr s)
  let mem' := if busWe s then Mem.write s.mem (busAddr s) (busWdata s) else s.mem
  let base : MState :=
    { s with mem := mem', nmiPrev := nmi_n
             nmiPending := if (!nmi_n && s.nmiPrev) then true else s.nmiPending }
  match s.st with
  | .resetLo => { base with tmpLo := rd, st := .resetHi }
  | .resetHi => { base with pc := wordOf s.tmpLo rd, st := .fetch }
  | .fetch =>
      if s.nmiPending then
        { base with irqNmi := true, st := .irqPushHi }
      else if (!irq_n && !If s.p) then
        { base with irqNmi := false, st := .irqPushHi }
      else
        { base with opcode := rd, op := decodeOp rd, pc := s.pc + 1, st := dispatchState rd }
  | .imm =>
      let p' :=
        match s.op with
        | .lda => nz rd s.p
        | .ldx | .ldy => nz rd s.p
        | .adc => (adc s.a rd s.p).2
        | .sbc => (sbc s.a rd s.p).2
        | .and_ | .ora | .eor => nz (match s.op with
            | .and_ => s.a &&& rd | .ora => s.a ||| rd | _ => s.a ^^^ rd) s.p
        | _ => s.p
      let a' :=
        match s.op with
        | .lda | .adc | .sbc => match s.op with
            | .lda => rd
            | .adc => (adc s.a rd s.p).1
            | _ => (sbc s.a rd s.p).1
        | .and_ => s.a &&& rd
        | .ora => s.a ||| rd
        | .eor => s.a ^^^ rd
        | _ => s.a
      let x' := if s.op = .ldx then rd else s.x
      let y' := if s.op = .ldy then rd else s.y
      let p'' :=
        match s.op with
        | .cmp => cmp s.a rd s.p
        | .cpx => cmp s.x rd s.p
        | .cpy => cmp s.y rd s.p
        | _ => p'
      { base with a := a', x := x', y := y', p := p'', pc := s.pc + 1, st := .fetch }
  | .zp => { base with ea := zext rd, pc := s.pc + 1, st := .absData }
  | .zpx => { base with ea := zext (rd + s.x), pc := s.pc + 1, st := .absData }
  | .zpy => { base with ea := zext (rd + s.y), pc := s.pc + 1, st := .absData }
  | .absxLo => { base with tmpLo := rd, pc := s.pc + 1, st := .absxHi }
  | .absxHi => { base with ea := wordOf s.tmpLo rd + zext s.x, pc := s.pc + 1, st := .absData }
  | .absyLo => { base with tmpLo := rd, pc := s.pc + 1, st := .absyHi }
  | .absyHi => { base with ea := wordOf s.tmpLo rd + zext s.y, pc := s.pc + 1, st := .absData }
  | .indxPtr => { base with zpPtr := rd + s.x, pc := s.pc + 1, st := .indxLo }
  | .indxLo => { base with tmpLo := rd, zpPtr := s.zpPtr + 1, st := .indxHi }
  | .indxHi => { base with ea := wordOf s.tmpLo rd, st := .absData }
  | .indyPtr => { base with zpPtr := rd, pc := s.pc + 1, st := .indyLo }
  | .indyLo => { base with tmpLo := rd, zpPtr := s.zpPtr + 1, st := .indyHi }
  | .indyHi => { base with ea := wordOf s.tmpLo rd + zext s.y, st := .absData }
  | .jmpIndLo => { base with tmpLo := rd, pc := s.pc + 1, st := .jmpIndHi }
  | .jmpIndHi => { base with ea := wordOf s.tmpLo rd, pc := s.pc + 1, st := .jmpIndReadLo }
  | .jmpIndReadLo => { base with tmpLo := rd, st := .jmpIndReadHi }
  | .jmpIndReadHi => { base with pc := wordOf s.tmpLo rd, st := .fetch }
  | .absLo => { base with tmpLo := rd, pc := s.pc + 1, st := .absHi }
  | .absHi =>
      if s.op = .jmp then
        { base with ea := wordOf s.tmpLo rd, pc := wordOf s.tmpLo rd, st := .fetch }
      else if s.op = .jsr then
        { base with ea := wordOf s.tmpLo rd, pc := s.pc + 1, st := .jsrPushHi }
      else
        { base with ea := wordOf s.tmpLo rd, pc := s.pc + 1, st := .absData }
  | .absData =>
      let r := s.p
      let p' :=
        match s.op with
        | .lda | .ldx | .ldy => nz rd r
        | .adc => (adc s.a rd r).2
        | .sbc => (sbc s.a rd r).2
        | .and_ => nz (s.a &&& rd) r
        | .ora => nz (s.a ||| rd) r
        | .eor => nz (s.a ^^^ rd) r
        | .cmp => cmp s.a rd r
        | .cpx => cmp s.x rd r
        | .cpy => cmp s.y rd r
        | .bit => bitOp s.a rd r
        | .inc => nz (rd + 1) r
        | .dec => nz (rd - 1) r
        | .asl => (nz (rd <<< 1) r &&& 0xFE) ||| boolByte (msb rd)
        | .lsr => (nz (rd >>> 1) r &&& 0xFE) ||| boolByte (lsb rd)
        | .rol => (nz ((rd <<< 1) ||| boolByte (Cf r)) r &&& 0xFE) ||| boolByte (msb rd)
        | .ror =>
            (nz ((rd >>> 1) ||| (boolByte (Cf r) <<< 7)) r &&& 0xFE) ||| boolByte (lsb rd)
        | _ => s.p
      let a' :=
        match s.op with
        | .lda => rd
        | .adc => (adc s.a rd r).1
        | .sbc => (sbc s.a rd r).1
        | .and_ => s.a &&& rd
        | .ora => s.a ||| rd
        | .eor => s.a ^^^ rd
        | _ => s.a
      let x' :=
        match s.op with
        | .ldx => rd
        | _ => s.x
      let y' :=
        match s.op with
        | .ldy => rd
        | _ => s.y
      let rmw' :=
        match s.op with
        | .inc => rd + 1
        | .dec => rd - 1
        | .asl => rd <<< 1
        | .lsr => rd >>> 1
        | .rol => (rd <<< 1) ||| boolByte (Cf r)
        | .ror => (rd >>> 1) ||| (boolByte (Cf r) <<< 7)
        | _ => s.rmwData
      let st' :=
        match s.op with
        | .inc | .dec | .asl | .lsr | .rol | .ror => .rmwWrite
        | _ => .fetch
      { base with a := a', x := x', y := y', p := p', rmwData := rmw', st := st' }
  | .irqPushHi => { base with sp := s.sp - 1, st := .irqPushLo }
  | .irqPushLo => { base with sp := s.sp - 1, st := .irqPushP }
  | .irqPushP => { base with sp := s.sp - 1, p := s.p ||| 0x04, st := .irqVecLo }
  | .irqVecLo => { base with tmpLo := rd, st := .irqVecHi }
  | .irqVecHi => { base with pc := wordOf s.tmpLo rd, nmiPending := false, st := .fetch }
  | .brkPushHi => { base with sp := s.sp - 1, st := .brkPushLo }
  | .brkPushLo => { base with sp := s.sp - 1, st := .brkPushP }
  | .brkPushP => { base with sp := s.sp - 1, p := s.p ||| 0x04, st := .brkVecLo }
  | .brkVecLo => { base with tmpLo := rd, st := .brkVecHi }
  | .brkVecHi => { base with pc := wordOf s.tmpLo rd, st := .fetch }
  | .rtiPullP => { base with sp := s.sp + 1, p := (rd &&& 0xCF) ||| 0x20, st := .rtiPullLo }
  | .rtiPullLo => { base with sp := s.sp + 1, tmpLo := rd, st := .rtiPullHi }
  | .rtiPullHi => { base with sp := s.sp + 1, pc := wordOf s.tmpLo rd, st := .fetch }
  | .jsrPushHi => { base with sp := s.sp - 1, st := .jsrPushLo }
  | .jsrPushLo => { base with sp := s.sp - 1, st := .jsrSet }
  | .jsrSet => { base with pc := s.ea, st := .fetch }
  | .rtsPullLo => { base with sp := s.sp + 1, tmpLo := rd, st := .rtsPullHi }
  | .rtsPullHi => { base with sp := s.sp + 1, pc := wordOf s.tmpLo rd + 1, st := .fetch }
  | .pushA => { base with sp := s.sp - 1, st := .fetch }
  | .pullA => { base with sp := s.sp + 1, a := rd, p := nz rd s.p, st := .fetch }
  | .pushP => { base with sp := s.sp - 1, st := .fetch }
  | .pullP => { base with sp := s.sp + 1, p := (rd &&& 0xCF) ||| 0x20, st := .fetch }
  | .rmwWrite => { base with st := .fetch }
  | .branch => { base with st := .branchExec }
  | .branchExec =>
      let taken :=
        match s.op with
        | .bne => !Zf s.p
        | .beq => Zf s.p
        | .bcc => !Cf s.p
        | .bcs => Cf s.p
        | .bpl => !Nf s.p
        | .bmi => Nf s.p
        | .bvc => !Vf s.p
        | .bvs => Vf s.p
        | _ => false
      { base with pc := if taken then s.pc + 1 + sext rd else s.pc + 1, st := .fetch }
  | .implied =>
      let a' :=
        match s.op with
        | .txa => s.x
        | .tya => s.y
        | .asl => s.a <<< 1
        | .lsr => s.a >>> 1
        | .rol => (s.a <<< 1) ||| boolByte (Cf s.p)
        | .ror => (s.a >>> 1) ||| (boolByte (Cf s.p) <<< 7)
        | _ => s.a
      let x' :=
        match s.op with
        | .tax => s.a
        | .tsx => s.sp
        | .inx => s.x + 1
        | .dex => s.x - 1
        | _ => s.x
      let y' :=
        match s.op with
        | .tay => s.a
        | .iny => s.y + 1
        | .dey => s.y - 1
        | _ => s.y
      let sp' := if s.op = .txs then s.x else s.sp
      let p' :=
        match s.op with
        | .tax => nz s.a s.p
        | .txa => nz s.x s.p
        | .tay => nz s.a s.p
        | .tya => nz s.y s.p
        | .tsx => nz s.sp s.p
        | .inx => nz (s.x + 1) s.p
        | .iny => nz (s.y + 1) s.p
        | .dex => nz (s.x - 1) s.p
        | .dey => nz (s.y - 1) s.p
        | .asl => (nz (s.a <<< 1) s.p &&& 0xFE) ||| boolByte (msb s.a)
        | .lsr => (nz (s.a >>> 1) s.p &&& 0xFE) ||| boolByte (lsb s.a)
        | .rol => (nz ((s.a <<< 1) ||| boolByte (Cf s.p)) s.p &&& 0xFE) ||| boolByte (msb s.a)
        | .ror => (nz ((s.a >>> 1) ||| (boolByte (Cf s.p) <<< 7)) s.p &&& 0xFE) ||| boolByte (lsb s.a)
        | .clc => setC s.p false
        | .sec => setC s.p true
        | .cli => setI s.p false
        | .sei => setI s.p true
        | .clv => setV s.p false
        | .cld => setD s.p false
        | .sed => setD s.p true
        | _ => s.p
      { base with a := a', x := x', y := y', sp := sp', p := p', st := .fetch }
  | .nop => { base with st := .fetch }

/-- Run `n` enabled cycles with no interrupt requests. -/
def mrun : Nat → MState → MState
  | 0, s => s
  | n + 1, s => mrun n (mstep true true s)

end Open6502
