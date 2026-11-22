/-
  Flat-image runner for the ISA model.

  Usage:
    open6502-run <image.bin> [--entry ADDR] [--steps N] [--trace]

  The image is loaded at address 0 (like the Klaus functional test) unless
  `--entry` is given, in which case execution starts at that address with the
  reset register values.  Without `--entry` the reset vector at $FFFC is used.
-/
import Open6502.ISA

open Open6502

def parseAddr (s : String) : Nat :=
  let hexDigit (c : Char) : Option Nat :=
    if '0' ≤ c ∧ c ≤ '9' then some (c.toNat - '0'.toNat)
    else if 'a' ≤ c ∧ c ≤ 'f' then some (c.toNat - 'a'.toNat + 10)
    else if 'A' ≤ c ∧ c ≤ 'F' then some (c.toNat - 'A'.toNat + 10)
    else none
  if s.startsWith "0x" || s.startsWith "0X" then
    (s.drop 2).toString.foldl (fun acc c =>
      match hexDigit c with
      | some d => acc * 16 + d
      | none => acc) 0
  else
    String.toNat? s |>.getD 0

def hex (n : Nat) : String :=
  let digits := "0123456789ABCDEF".toList
  let rec go (n : Nat) (acc : String) : String :=
    if n < 16 then s!"{digits[n]!}{acc}"
    else go (n / 16) s!"{digits[n % 16]!}{acc}"
  go n ""

def showState (s : Machine) : String :=
  s!"pc={hex s.regs.pc.toNat} a={hex s.regs.a.toNat} x={hex s.regs.x.toNat} \
     y={hex s.regs.y.toNat} p={hex s.regs.p.toNat} sp={hex s.regs.sp.toNat}"

partial def runTrace (s : Machine) (fuel : Nat) (trace : Bool) : IO Machine := do
  match fuel with
  | 0 => return s
  | n + 1 =>
      if trace then IO.println (showState s)
      let s' := step s
      if s'.regs.pc = s.regs.pc then
        if trace then IO.println s!"trap at pc={hex s.regs.pc.toNat}"
        return s'
      else runTrace s' n trace

def main (args : List String) : IO Unit := do
  let some path := args.head? | IO.eprintln "usage: open6502-run <image.bin> [--entry ADDR] [--steps N] [--trace]"; return
  let data ← IO.FS.readBinFile path
  let mem : Mem := fun a => BitVec.ofNat 8 (data[a.toNat]?.getD 0xEA).toNat
  let mut entry : Option Nat := none
  let mut steps := 1000000
  let mut trace := false
  let mut rest := args.tail
  while let some arg := rest.head? do
    rest := rest.tail
    match arg with
    | "--entry" => entry := rest.head?.map parseAddr; rest := rest.tail
    | "--steps" => steps := rest.head?.map parseAddr |>.getD 1000000; rest := rest.tail
    | "--trace" => trace := true
    | _ => pure ()
  let start : Machine :=
    match entry with
    | some e => { (reset mem) with regs := { (reset mem).regs with pc := BitVec.ofNat 16 e } }
    | none => reset mem
  IO.println s!"start {showState start}"
  let final ← runTrace start steps trace
  IO.println s!"final {showState final}"

