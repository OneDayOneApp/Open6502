# Open6502 formal model (Lean 4)

A machine-checked model of the Open6502 CPU: an ISA-level specification, a
cycle-level transcription of `rtl/cpu/open6502_cpu.sv`, and proofs that the
cycle-level machine implements the specification.  The development has **no
external dependencies** (Lean 4 core only), builds in about 15 seconds, and
contains no `sorry`.  The ALU specification lemmas are discharged by Lean's
`bv_decide` decision procedure (its per-goal certificate axiom,
`Std.Tactic.BVDecide.Reflect.verifyBVExpr … = true`, appears in
`#print axioms`); every other result — including all refinement theorems and
program tests — is a plain kernel proof depending only on `propext`,
`Classical.choice` and `Quot.sound`.

The method follows CktFormalizer / CktLean
([project page](https://ckt-formalizer.github.io/#cktlean),
[code](https://github.com/menik1126/NL2Chip)):

| CktLean step | Open6502 formal model |
| --- | --- |
| Typed hardware representation (widths, state, clock domains are types) | Every register is a `BitVec` of an explicit width (`Byte = BitVec 8`, `Word = BitVec 16`); the FSM state is an inductive type; `MState` mirrors the RTL register-for-register |
| Elaboration rejects ill-typed circuits | `lake build` type-checks the model *and* every theorem before anything is run |
| Source-level proofs checked by Lean | `decide`/`simp` kernel proofs for refinement and program tests; `bv_decide` (Lean's official bit-blaster) for the exhaustive 2¹⁶-case arithmetic lemmas |
| `#synthesizeVerilog` extraction | `lake exe open6502-extract` derives `build/open6502_decode.sv` and `build/decode.json` from the same Lean table the proofs use |
| Implementation ≡ specification equivalence | `Open6502/Refinement.lean`: for each verified instruction, `mrun n m = step (toMachine m)` for *all* states and memory images |
| Tool feedback loop | `lake build`, `lake exe open6502-run`, and `scripts/check_rtl_sync.py` in CI |

## Layout

| File | Contents |
| --- | --- |
| `Open6502/Basic.lean` | widths, bit/flags helpers, sign extension, little-endian reads, page-wrap, memory update |
| `Open6502/Opcodes.lean` | generated documented NMOS 6502 matrix (151 opcodes), decoder, lengths, cycle table |
| `Open6502/ALU.lean` | ADC/SBC (binary and decimal), CMP, shifts, INC/DEC, BIT — implementation and specification theorems (`bv_decide`) |
| `Open6502/ISA.lean` | architectural state, reset, stack, addressing modes, one-instruction `step` |
| `Open6502/FSM.lean` | `FState`, the opcode → first-state dispatch transcribed from the RTL, and the exhaustive agreement proof |
| `Open6502/Micro.lean` | cycle-level `MState`, `busAddr`/`busWdata`/`busWe`, `mstep`, `mrun` |
| `Open6502/Decode.lean` | table structure and decode equations used by the proofs |
| `Open6502/Refinement.lean` | per-instruction `mrun n = step` theorems |
| `Open6502/Tests.lean` | executable programs with expected results (kernel runs them via `decide`) |
| `Extract.lean`, `Run.lean` | SystemVerilog/JSON extraction and a flat-image runner |
| `scripts/check_rtl_sync.py` | parses the real `open6502_cpu.sv` and checks its dispatch against the emitted table |

## What is proved

**Decoder.** `officialTable_length = 151`, opcode uniqueness, and
`dispatch_check`: for every official opcode the decoder agrees with the
first FSM state the RTL dispatches to.  `scripts/check_rtl_sync.py` repeats that
check against the actual SystemVerilog text, so a change to either side breaks
CI.

**ALU specification** (`Open6502/ALU.lean`), all closed by `bv_decide` over all
2¹⁶ operand pairs and both carry inputs (the `bv_decide` certificate is checked
by Lean's built-in `Std.Tactic.BVDecide` verifier; see the trust note above):

* `adcBinary_spec` — binary ADC is wrapping addition; `adcBinaryCarry_spec` —
  carry is the exact sum ≥ 256; `overflowAdd_spec` — V is signed overflow.
* `adcDecimal_valid`, `adcDecimal_value`, `adcDecimal_carry` — for **valid BCD
  inputs** the decimal result is valid BCD, its numeric value is
  `(a + b + carry) mod 100`, and C is set exactly when the sum reaches 100.
* The same four results for SBC (`sbcDecimal_value`, `sbcDecimal_carry`, …),
  including the borrow correction by 100.
* `cmp_carry`/`cmp_zero`/`cmp_negative` — CMP is unsigned comparison with N/Z
  from the difference; `asl_carry`, `rol_result`, `bitOp_zero`, … for shifts
  and BIT; `adc_preserves_flags`/`sbc_preserves_flags` — Decimal/Irq/Break/
  Unused bits are untouched.

**Addressing and stack.** Zero-page index wrap, `($nn,X)` zero-page pointer
wrap, `($nn),Y` and absolute indexed 16-bit adds, the NMOS `JMP ($xxxx)`
page-wrap quirk, stack push/pull convention, and reset-vector handling are all
written explicitly in `ISA.lean` and exercised by `Tests.lean`.

**Refinement** (`Open6502/Refinement.lean`).  For an arbitrary `MState` at the
fetch boundary, running the RTL FSM for the instruction's cycle count produces
exactly the architectural state of `step`:

| Instruction | Cycles | Theorem |
| --- | ---: | --- |
| `LDA #imm` | 2 | `lda_imm_refine` |
| `LDA $nn` | 3 | `lda_zp_refine` |
| `LDA $hhhh` | 4 | `lda_abs_refine` |
| `LDA $hhhh,X` | 4 | `lda_absx_refine` |
| `LDA ($nn),Y` | 5 | `lda_indy_refine` |
| `STA $hhhh` | 4 | `sta_abs_refine` |
| `ADC #imm` | 2 | `adc_imm_refine` |
| `CMP #imm` | 2 | `cmp_imm_refine` |
| `INX` | 2 | `inx_refine` |
| `PHA` / `PLA` | 2 / 2 | `pha_refine`, `pla_refine` |
| `JMP $hhhh` / `JMP ($hhhh)` | 3 / 5 | `jmp_abs_refine`, `jmp_ind_refine` |
| `JSR $hhhh` / `RTS` | 6 / 3 | `jsr_refine`, `rts_refine` |
| `BNE` taken / not taken | 3 / 3 | `bne_taken_refine`, `bne_not_taken_refine` |
| `BRK` / `RTI` | 6 / 4 | `brk_refine`, `rti_refine` |

**Co-simulation.** `formal/scripts/cosim.py` runs four of those programs on
both the Lean ISA model and the actual SystemVerilog (`iverilog`) and compares
the final `pc a x y p sp`; all four agree, including the decimal ADC/SBC and
JSR/RTS cases:

```text
ok   adc      pc=8008 a=01 x=00 y=00 p=24 sp=fd
ok   decimal  pc=8011 a=99 x=00 y=00 p=ac sp=fd
ok   jsr      pc=8006 a=42 x=00 y=00 p=24 sp=fd
ok   branch   pc=8007 a=00 x=05 y=00 p=27 sp=fd
```

**Programs.** `Open6502/Tests.lean` assembles 15 small programs (binary ADC,
decimal ADC/SBC, JSR/RTS stack balance, BRK/RTI, RMW, zero-page wrap, indirect
JMP wrap, a branch loop, an illegal opcode) and states their expected results
as theorems; the kernel executes the model while checking them.

## Build and run

```bash
cd formal
lake build                      # type-check + prove everything
lake exe open6502-extract build # emit build/open6502_decode.sv and decode.json
python3 scripts/check_rtl_sync.py \
  --rtl ../rtl/cpu/open6502_cpu.sv --json build/decode.json
python3 scripts/cosim.py        # Lean vs RTL on four programs (needs iverilog)
make check-rtl check-cosim      # same, via make
```

The toolchain is pinned in `lean-toolchain` (`leanprover/lean4:v4.36.0`); with
[elan](https://github.com/leanprover/elan) installed, `lake build` fetches it
automatically.  A pre-downloaded toolchain can also live in `formal/.toolchain`
(ignored by git) and be put on `PATH`.

Run a flat 64 KiB image (for example a test ROM loaded at address 0):

```bash
lake exe open6502-run build/6502_functional_test.bin --entry 0x0400 --trace
```

The runner stops when `pc` stops changing (the usual "trap" convention of the
6502 test ROMs) and prints `pc a x y p sp`.  A full Klaus run is opt-in because
the executable model is an interpreter: it takes minutes, whereas the proofs
themselves take seconds.

## Fidelity notes and known deviations

The model is a transcription of the **current RTL**, not of the datasheet, and
the differences are deliberate and documented:

* **Cycle counts.** `mrun n` uses the FSM's cycle count.  The RTL does not
  implement page-cross penalties (`LDA $hhhh,X` is 4 cycles even when the index
  crosses a page) or the dummy stack cycles of some instructions (`RTS` is 3,
  `RTI` 4, `PHA` 2, `PLA` 2, `BRK` 6).  The refinement theorems prove those
  numbers; they do not claim datasheet timing.
* **Decimal flags.** The RTL computes N/Z from the adjusted BCD result and V
  from the binary operands.  On an NMOS 6502 the Z flag reflects the binary
  sum.  The Lean specification states the RTL behaviour explicitly
  (`adcStatus`), and the BCD *value* and carry are proved correct for valid
  BCD inputs.
* **Interrupts.** `mstep` models NMI edge latching and IRQ level sampling, and
  the FSM sequences for `BRK`/`RTI` are verified, but the IRQ/NMI entry
  sequences are not yet the subject of a refinement theorem.
* **Illegal opcodes.** The RTL treats them as a one-byte implied no-op; the
  decoder returns `none` and `step` advances `pc` by one, which a test pins
  down.
* **Reset.** `mreset` and the two-cycle reset sequence are modelled; the
  architectural `reset` function is used by the program tests.  A refinement
  theorem for the reset sequence is on the roadmap.

## Roadmap

1. Refinement for the remaining official opcodes (the pattern is one `simp`
   line per instruction) and for IRQ/NMI entry and reset.
2. Page-cross and dummy-read timing in the RTL, then tighten the refinement
   cycle counts to the datasheet.
3. A larger verified program suite (16-bit multiply, memcpy) with loop
   invariants, reusing the abstract `step`.
4. Optional: emit the ALU itself as SystemVerilog from Lean and prove the
   emitted RTL equivalent to the model, closing the loop exactly as CktLean
   does for its benchmarks.

## 中文摘要

`formal/` 用 Lean 4 对 Open6502 的 CPU 做形式化验证，方法对齐 CktFormalizer /
CktLean：用类型表达位宽与状态、用内核检查的证明建立“实现 ≡ 规格”，再把
Lean 中的译码表抽取成 SystemVerilog/JSON，并与手写 RTL 做自动一致性检查。

已证明：151 条官方指令译码表与 RTL 分派一致；ADC/SBC 的二进制与十进制
（BCD）语义、进位/溢出/N/Z、CMP、移位、BIT 的规格；零页回绕、间接 JMP
页回绕、堆栈约定；19 条指令的逐周期精化定理（任意寄存器与内存状态下成立）；
15 个可执行小程序的功能测试。`lake build` 即完成全部检查，无 `sorry`；
精化定理与程序测试是纯内核证明，算术规格由 Lean 官方 `bv_decide` 位向量判定过程
给出（其证书由 Lean 内置校验器检查）。
