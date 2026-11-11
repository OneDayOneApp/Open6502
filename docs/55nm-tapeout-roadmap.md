# Open6502 55nm Tapeout Roadmap

## Goal

Build an open, verifiable 6502/NES-compatible SoC and take it through a 55nm
tapeout.

This is a long-running engineering goal. The current repository is a
**functional prototype**, not a tapeout-ready design.

## Current baseline

| Item | Status |
|---|---|
| RTL CPU | Klaus PASS |
| RTL SoC | Title and in-game frame in Verilator |
| NEMU reference | Playable NES frontend |
| RTL tests | 28 PASS |
| DiffTest | Final-state, PC trace, ALU/stack smoke tests |
| Synthesis | Per-module Yosys area report |
| FPGA | Wrapper only, no bitstream |
| ASIC | No PDK integration, no DFT, no timing closure |

## Readiness levels

### M0 — Functional prototype (current)

- CPU, PPU, APU, bus, WRAM, mapper, DMA, controller present.
- Game title and START gameplay frame rendered.
- NEMU/RTL comparison exists.

Exit criteria:

```text
RTL Klaus PASS
RTL frame parity on selected games
Regression green
```

### M1 — Reference parity

- Lockstep NEMU/RTL comparison for CPU, PPU, APU and mapper state.
- Golden frames for multiple games and mapper types.
- Deterministic frame-index tests.

Exit criteria:

```text
multiple NROM/MMC1/MMC3 games match NEMU frames
no unexplained state divergence
```

### M2 — FPGA prototype

- Yosys synthesis completes for the full SoC.
- BRAM inference for CHR/VRAM/OAM/palette.
- Timing constraints and Fmax report.
- Board with VGA/HDMI, audio, controller, ROM storage.

Exit criteria:

```text
bitstream runs a game on a real board
stable video and input
```

### M3 — ASIC feasibility

- 55nm standard-cell library and SRAM macros.
- SRAM replacement for CHR/VRAM/OAM/palette.
- Sequential sprite scan and PPU pipeline.
- Synthesis, area, timing and power reports.
- DFT/scan insertion and coverage targets.
- Clock/reset/power intent.
- DRC/LVS clean.

Exit criteria:

```text
tapeout checklist reviewed
timing/DRC/LVS clean
test coverage and DFT signed off
foundry shuttle secured
```

### M4 — Tapeout

- Mask generation and foundry shuttle.
- Silicon bring-up and validation.
- Errata and respin plan.

## Architecture work required before 55nm

1. Replace PPU storage flip-flops with SRAM macros.
2. Rewrite sprite selection as a sequential scan.
3. Rewrite background rendering as fetch/shift pipeline.
4. Add bus transaction strobe (`bus_strobe`) for all peripherals.
5. Make CPU cycle timing accurate for NMI/DMA-sensitive games.
6. Complete DMC bus arbitration.
7. Add DFT-friendly clock/reset and scan structure.
8. Add pad ring, ESD, power grid and package plan.

## Area and cost planning

Current Yosys generic-cell estimate:

```text
total: ~1.02M generic cells
PPU:   ~976k
CPU:   ~4.1k
```

55nm planning estimate:

```text
current RTL, FF memory: 4-7 mm^2
SRAM + PPU refactor:   1.2-2.0 mm^2
planning target:       ~1.5 mm^2
```

See:

- [synthesis-area.md](open6502-chip/synthesis-area.md)
- [synthesis-and-cost.md](synthesis-and-cost.md)

## What contributors can do now

| Area | Help wanted |
|---|---|
| PPU | fetch/shift pipeline, sprite evaluation, sprite-0 |
| CPU | cycle accuracy, NMI/DMA corner cases |
| APU | DMC arbitration, frame sequencer |
| Mappers | MMC3 A12, CHR banking |
| Verification | lockstep DiffTest, frame hashes |
| FPGA | synthesis, BRAM, constraints, board |
| ASIC | 55nm flow, SRAM, DFT, timing |
| Docs | reproducible guides and test reports |

See [CONTRIBUTING.md](../CONTRIBUTING.md).

## What is needed from the community

- Reviewers with NES/6502 hardware knowledge.
- Verification engineers with cocotb/Verilator/formal experience.
- FPGA engineers with board bring-up experience.
- ASIC engineers with 55nm synthesis/DFT/PDK experience.
- Test ROMs and hardware captures with clear licenses.
- Funding or shuttle access for tapeout.

## Non-negotiable rules

- No copyrighted ROMs or proprietary PDK files in the repository.
- No tapeout-readiness claims before timing/DFT/DRC/LVS are clean.
- No golden hash changes without review.
- No deleting tests to make a change pass.
