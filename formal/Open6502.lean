/-
  Open6502 formal model — umbrella module.

  Importing this module type-checks the model, the decoder, the RTL/decoder
  cross-check, and every theorem (ALU specification, stack/program tests and
  instruction-level refinement).  `lake build` therefore checks the whole
  proof development; there are no `sorry`s.
-/
import Open6502.Basic
import Open6502.Opcodes
import Open6502.FSM
import Open6502.ALU
import Open6502.ISA
import Open6502.Decode
import Open6502.Micro
import Open6502.Refinement
import Open6502.Tests
