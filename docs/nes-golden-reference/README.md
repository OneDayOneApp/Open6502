# NES/Famicom Golden Reference (NEMU-based)

This directory defines the NEMU-based golden-reference design, test plan and
acceptance checklist for a complete NES/Famicom implementation.

- [Design](design.md)
- [Test plan](test-plan.md)
- [Checklist](checklist.md)
- [Must-have feature matrix](feature-matrix.md)

## Definition

A golden reference is a dedicated NEMU build that:

- reuses NEMU's CPU execution, ISA, memory, monitor, trace and DiffTest
  infrastructure;
- adds the NES platform: 2A03/2C02/APU, bus and mappers;
- produces a REF executable and a `TARGET_SHARE` REF `.so`;
- lets a DUT (RTL, another NEMU build or an external emulator) compare against
  it through NEMU DiffTest and a NES extension protocol;
- is not the current `6502EMU` demo frontend. That frontend is a prototype and
  smoke-test target.

## Principles

1. NEMU is the base; do not build a second independent emulator.
2. REF and DUT must be separate builds.
3. CPU DiffTest comes first; PPU/APU/mapper need an extended protocol.
4. Master tick, snapshots, hashes and traces are part of the REF.
5. Hardware captures and specifications outrank emulator-to-emulator
   comparisons.
6. The default "complete" scope is T0+T1; T2/T3 are separate acceptance levels.

## Current status

v1.0.0 is a functional parity baseline:

- REF CPU + PPU + APU + mapper frame loop
- bundled NROM game frame/audio hashes match the frontend
- deterministic state hash, snapshots and input replay
- CPU-only NEMU DiffTest

The v1.5 work in progress adds:

- dot-loop background pixels and sprite 0 dot checks
- APU DMC core
- Open6502 RTL chip scaffold

Still missing:

- full dot-level background fetch/shift and sprite evaluation
- DMC CPU DMA stall
- NES extended DiffTest
- hardware capture alignment
- T2/T3 mappers, accessories and PAL
