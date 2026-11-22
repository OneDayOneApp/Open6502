import Lake
open Lake DSL

/-
  Open6502 formal model.

  This package deliberately has **no external dependencies**: it only uses the
  Lean 4 core library (`Std`), so a fresh checkout builds in seconds without
  Mathlib.  The model is a typed, cycle-accurate description of
  `rtl/cpu/open6502_cpu.sv` plus an ISA-level specification and the proofs that
  connect the two (see `Open6502/README.md`).
-/

package «open6502-formal» where
  leanOptions := #[
    -- Keep the elaborator strict: an accidental implicit parameter in a
    -- theorem statement should be an error, not a silently generalised goal.
    ⟨`autoImplicit, false⟩
  ]

@[default_target]
lean_lib «Open6502» where
  srcDir := "."

/-- Executable that emits the machine-checked decode/ALU tables as
SystemVerilog and JSON (the "extraction" half of the CktLean-style loop). -/
lean_exe «open6502-extract» where
  root := `Extract

/-- Executable that runs the Lean model on a flat 64 KiB image (used to
replay the ROM-based tests such as Klaus). -/
lean_exe «open6502-run» where
  root := `Run
