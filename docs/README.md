# Open6502 Documentation

Two ways in:

- **Practice the flow** → [learning-path.zh-CN.md](learning-path.zh-CN.md) (Chinese): the core story, the three loops, a 12-week route and what does / does not transfer from the XiangShan-style process.
- **Review the design** → [open6502-chip/README.md](open6502-chip/README.md), then follow the sections below.

The docs are grouped by the loop they serve: inner (minutes), middle (every commit) and outer (every iteration).

## Learning path

| Doc | What it answers |
|---|---|
| [learning-path.zh-CN.md](learning-path.zh-CN.md) | How to rehearse the XiangShan-style flow on this repo; which gaps matter first |
| [../site/learn.zh.html](../site/learn.zh.html) | Web version: three loops, the eight-dimension XiangShan scorecard and the 12-week route |

## Inner loop — verification and debug (minutes)

| Doc | What it answers |
|---|---|
| [open6502-chip/verification.md](open6502-chip/verification.md) | Verification levels, test harnesses and what each level proves |
| [open6502-chip/difftest.md](open6502-chip/difftest.md) | The REF/DUT comparison protocol; where to extend it to lockstep |
| [nes-golden-reference/test-plan.md](nes-golden-reference/test-plan.md) | Test plan for the NEMU-based golden reference |
| [nes-golden-reference/design.md](nes-golden-reference/design.md) | How REF builds, clocks, snapshots and hashes work |
| [game-testing.md](game-testing.md) | Running the same game through NEMU and through RTL, frame by frame |

## Middle loop — every commit

| Doc | What it answers |
|---|---|
| [nes-golden-reference/feature-matrix.md](nes-golden-reference/feature-matrix.md) | 73 must-have NES features and their verified status |
| [nes-golden-reference/checklist.md](nes-golden-reference/checklist.md) | Golden-reference acceptance checklist |
| [open6502-chip/checklist.md](open6502-chip/checklist.md) | Chip-level gate checklist |
| [../CONTRIBUTING.md](../CONTRIBUTING.md) | Rules for changes: test first, run lint/sim/DiffTest, keep golden hashes reviewed |

## Outer loop — architecture and backend

| Doc | What it answers |
|---|---|
| [open6502-chip/architecture.md](open6502-chip/architecture.md) | Block diagram, address map, ports, clock domains |
| [open6502-chip/synthesis-area.md](open6502-chip/synthesis-area.md) | Yosys per-module area, and where the area actually goes |
| [synthesis-and-cost.md](synthesis-and-cost.md) | Die-area and manufacturing cost model, with assumptions |
| [55nm-tapeout-roadmap.md](55nm-tapeout-roadmap.md) | NEMU → RTL → FPGA → ASIC gates and exit criteria |
| [open6502-chip/startup-recovery-plan.md](open6502-chip/startup-recovery-plan.md) | START/gameplay bring-up recovery plan |

## Models and analysis

| Doc | What it answers |
|---|---|
| [../GEM6502/README.md](../GEM6502/README.md) | The C cycle model: legacy fast model vs the v0.2 timing platform |
| [../NEMU/6502EMU.md](../NEMU/6502EMU.md) | The NEMU-based 6502EMU frontend and its options |
| [aot-acceleration-analysis.zh-CN.md](aot-acceleration-analysis.zh-CN.md) | Why AOT/JIT cannot replace the golden reference (Chinese) |

## Labs — self-contained experiments

| Doc | What it answers |
|---|---|
| [../labs/am6502/README.md](../labs/am6502/README.md) | Abstract-Machine-style packaging for 6502/NES: one image contract and one command per program (M1–M3: images on NEMU/GEM6502/RTL plus a per-instruction DiffTest) |
| [../labs/am6502/docs/design.md](../labs/am6502/docs/design.md) | Artifacts, ABI, host ports, ARCH matrix, milestones and risks for am6502 |
| [../labs/jit6502/README.md](../labs/jit6502/README.md) | 6502 → AArch64 JIT lab: interpreter, block JIT, NES frame comparison |

## Status

- [../README.md](../README.md) — project overview and current gate status
- [../STATUS.md](../STATUS.md) — verified claims and known gaps
