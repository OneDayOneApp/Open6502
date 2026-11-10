# Open6502 START Recovery Plan

## Goal

Make the RTL SoC enter and run the first gameplay frame after START, with
NEMU as the golden reference.

This plan is deliberately layered and forward-only. Each layer adds tests
before changing RTL. No test or golden image is removed to make a later stage
pass.

## Current baseline

- RTL CPU passes Klaus at `PC=0x3469`.
- RTL renders the `falling.nes` title frame byte-identically to NEMU.
- NMI reaches the CPU; `nmi_hits` increments once per frame.
- After START, the game enters a fade/clear path and the palette becomes zero.
- The controller read routine produces `$0A=0x10` instead of the expected
  `$0A=0x08` for START.

## Architecture

```text
L4  Golden game frame       RTL == NEMU, byte exact
L3  SoC state               NMI, palette writes, game state variables
L2  CPU bus transaction     one bus_strobe per CPU access
L1  Device protocol         $4016/$2006/$2007/NMI
L0  Unit blocks             CPU, PPU, APU, mapper, controller
```

### Bus transaction contract

The CPU exports:

```systemverilog
output logic bus_valid;
output logic bus_we;
output logic [15:0] bus_addr;
output logic [7:0]  bus_wdata;
```

The SoC derives:

```systemverilog
assign bus_strobe = cpu_ce && bus_valid;
```

Every peripheral uses `bus_strobe` as its single commit point. No peripheral
may commit on a level held for multiple 21 MHz clocks.

### Controller contract

- One write to `$4016` loads both shift registers.
- One read from `$4016` shifts controller 1 only.
- One read from `$4017` shifts controller 2 only.
- Bit order: A, B, Select, Start, Up, Down, Left, Right.
- START must appear in bit 3.

## Stages

### P0: Freeze golden data

Add:

```text
tests/golden/falling/title.ppm.sha256
tests/golden/klaus.txt
tests/golden/controller.txt
```

Gate:

```text
title frame hash unchanged
26 RTL tests pass
RTL Klaus passes
```

### P1: CPU cycle-accurate decode

Implemented by decoding in `ST_FETCH` from the fetched opcode byte instead of
adding a separate `ST_DECODE` cycle. This restores 6502 instruction timing for
NMI handlers that must finish before the next vblank.

Gate:

```text
RTL Klaus PASS
28 RTL tests PASS
falling.nes Start enters mode $00=1 without bouncing to $00=2
```

### P2: Controller protocol

Add `tb/open6502_controller_tb.sv`:

```text
write $4016=1
write $4016=0
read $4016 eight times
store bits in $0A
```

With `joy1=0x08`, require `$0A=0x08`. Gate:

```text
A/B/Select/Start/Up/Down/Left/Right bit order passes
```

### P3: NMI transaction

Add `tb/open6502_nmi_tb.sv`; require exactly one NMI handler entry per vblank.
Gate:

```text
nmi_hits == frames
no duplicate NMI
```

### P4: Post-START state

Run `falling.nes` with START held:

```text
$0A == 0x08
$10/$11 progress
palette write sequence matches NEMU
```

Gate:

```text
palette is not cleared by the wrong path
```

### P5: First gameplay frame

Capture RTL frame 20 and NEMU frame 20 with START held. Compare SHA-256.
Gate:

```text
RTL frame 20 == NEMU frame 20
```

### P6: Full gameplay

Add input scripting and multi-frame golden checks for Day/Sunset/Night modes.

### P7: FPGA

Only after P5/P6: synthesis, timing, PLL, video/audio, controller, ROM loader,
bitstream, board validation.

## Forward-only rules

1. Add the test before changing RTL.
2. Reproduce the failure.
3. Make the smallest architectural fix.
4. Run all previous gates.
5. Commit in English.
6. Never delete or weaken a previous test.
7. Never patch game data, palette tables, or golden frames to hide a bug.
8. Never use revert; fix forward with a new commit.
