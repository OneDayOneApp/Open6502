# Open6502 Verification Plan

## 1. Levels

| Level | Object | Method |
| --- | --- | --- |
| L0 | package/decode/counters | SV unit tests |
| L1 | CPU/PPU/APU blocks | directed tests + vectors |
| L2 | CPU + bus | Klaus, nestest, DiffTest |
| L3 | PPU + CPU | PPU ROMs, framebuffer hash |
| L4 | APU | apu_test, DMC, sample hash |
| L5 | mapper | bank/IRQ test ROMs |
| L6 | system | open-source games, hardware |
| L7 | FPGA | board video/audio/input |
| L8 | CPU ISA ↔ RTL FSM | Lean 4 refinement proofs (`formal/`) |

## 2. Required tests

- Klaus 6502 functional test (also replayable in the Lean model:
  `lake exe open6502-run <image> --entry 0x0400`; interpreter speed makes this
  an opt-in run, not a CI gate)
- nestest
- instr_test-v5
- cpu_timing_test6
- cpu_interrupts_v2
- ppu_vbl_nmi
- sprite_hit_tests
- sprite_overflow_tests
- ppu_open_bus
- apu_test / apu_reset / DMC
- 240p Test Suite
- open-source ROMs: Falling, Galaxy Patrol, Hanoi, Table Tennis, File Fixers,
  Stallar

## 2b. Formal verification (L8)

`formal/` contains a Lean 4 model of the CPU and kernel-checked proofs that the
cycle-level model implements an ISA-level specification.  It is the Open6502
adaptation of the CktFormalizer/CktLean method: typed hardware representation,
kernel-checked proofs, and extraction with an RTL consistency check.

What is covered:

- the 151 documented opcodes decode to the same first FSM state as the RTL
  (`dispatch_check`, plus `formal/scripts/check_rtl_sync.py` against the actual
  `rtl/cpu/open6502_cpu.sv`);
- binary and BCD ADC/SBC, CMP, shifts, INC/DEC and BIT satisfy their
  mathematical specifications for every 8-bit operand pair;
- zero-page wrap, indirect-JMP page wrap and the stack convention;
- per-instruction refinement theorems for 19 instructions (`mrun n m = step m`
  for all states), covering loads/stores, ADC/CMP, JSR/RTS, BRK/RTI, branches
  and indirect JMP;
- 15 executable program tests that the kernel runs while checking.

Build and checks:

```bash
cd formal
lake build
make check-rtl
```

The model mirrors the current RTL, so its cycle counts are the RTL's FSM
cycles.  Known deviations (no page-cross penalty, shortened stack sequences,
decimal N/Z flag behaviour) are listed in `formal/README.md` together with the
roadmap (IRQ/NMI entry refinement, timing tightening).

## 3. DiffTest gates

- CPU-only: Klaus + hello with zero mismatches
- frame-only: NROM game 60/300-frame hashes match
- scanline: PPU test ROMs match
- audio: APU sample hash/tolerance matches
- mapper: bank/IRQ state matches

## 4. FPGA validation

- stable 256x240 RGB output
- 60 Hz frame rate
- clean audio
- 1P/2P controllers
- ROM load/reset
- at least one hour of continuous operation

## 5. Hardware alignment

Requires:

- logic analyzer/bus captures
- PPU dot/scanline events
- APU channel output
- DMC DMA

No hardware is available in the current environment; deliver the capture format
and comparison tooling first.

## 6. Release gates

v0 FPGA prototype:

- CPU Klaus
- PPU vblank/NMI/sprite 0
- APU four channels
- mappers 0/1/2/3/4/7
- six open-source ROMs playable
- CPU + frame DiffTest passes

ASIC additionally requires DFT, timing closure, power, package and hardware
alignment.
