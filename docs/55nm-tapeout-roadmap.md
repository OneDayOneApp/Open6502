# Open6502 Roadmap: NEMU -> RTL -> FPGA -> ASIC

## One-line roadmap

```text
NEMU reference  ->  RTL SoC  ->  FPGA prototype  ->  55nm ASIC tapeout
```

Each stage is a gate. A later stage is not allowed to hide failures in an
earlier stage.

## Stage overview

| Stage | Purpose | Current status | Exit gate |
|---|---|---|---|
| 1. NEMU | Playable reference model | **Working** | Games playable; CPU Klaus PASS; reference frames stable |
| 2. RTL | SystemVerilog SoC | **Functional prototype** | RTL games match NEMU; CPU/PPU/APU/mapper verified |
| 3. FPGA | Real hardware prototype | **Not started** | Bitstream runs a game on a board |
| 4. ASIC | 55nm tapeout | **Planning** | Timing/DFT/DRC/LVS clean; shuttle secured |

## Stage 1: NEMU reference

The NEMU-based `6502EMU` is the golden reference and the fastest way for users
to play games today.

Done:

- 6502 CPU with Klaus PASS
- NES PPU/APU/cartridge frontend
- SDL window, keyboard and audio
- open-source ROM bundle

Gate:

```text
games are playable
CPU Klaus PASS
reference screenshots are reproducible
```

## Stage 2: RTL SoC

The RTL is the chip design under test. It must match the NEMU reference before
FPGA or ASIC work is meaningful.

Current state:

- CPU Klaus PASS at `PC=0x3469`
- 28 RTL tests PASS
- RTL title frame is byte-identical to NEMU
- RTL enters the FALLING gameplay frame after START
- NEMU/RTL DiffTest smoke tests exist

Still required:

- cycle-accurate CPU timing
- PPU fetch/shift and exact sprite evaluation
- DMC bus arbitration and DMA alignment
- full NEMU/RTL lockstep state comparison
- golden frames for NROM/MMC1/MMC3 games
- bus transaction uniqueness for every peripheral

Gate:

```text
multiple games match NEMU frames
no unexplained CPU/PPU/APU/mapper state divergence
regression is deterministic and green
```

## Stage 3: FPGA prototype

FPGA is the first real-hardware proof. It validates timing, video, audio,
controller and cartridge loading before committing to silicon.

Required:

- full Yosys synthesis
- BRAM inference for CHR/VRAM/OAM/palette
- timing constraints and Fmax report
- PLL and reset strategy
- VGA/HDMI output
- audio DAC or sigma-delta output
- controller input
- ROM/Flash/SD cartridge loading
- board bring-up

Gate:

```text
bitstream runs a game on a real board
video, audio, controller stable
no RTL simulation-only assumptions
```

## Stage 4: ASIC / 55nm tapeout

ASIC work starts only after FPGA proves the architecture.

Required:

- 55nm standard-cell library and SRAM macros
- SRAM replacement for CHR/VRAM/OAM/palette
- sequential sprite scan and PPU pipeline
- synthesis, area, timing and power reports
- DFT/scan insertion and coverage targets
- clock/reset/power intent
- DRC/LVS clean
- pad ring, ESD, package plan
- foundry shuttle and funding

Gate:

```text
timing/DFT/DRC/LVS clean
tapeout checklist reviewed
shuttle and funding secured
```

## Current position

```text
NEMU  [##########] working
RTL   [######----] functional prototype
FPGA  [----------] not started
ASIC  [----------] planning
```

## Work required before moving from RTL to FPGA

1. Complete bus transaction strobe for all peripherals.
2. Make CPU cycle timing accurate for NMI/DMA-sensitive games.
3. Replace PPU flip-flop memories with BRAM-friendly arrays.
4. Rewrite sprite selection as a sequential scan.
5. Rewrite background rendering as a fetch/shift pipeline.
6. Complete DMC bus arbitration.
7. Add deterministic frame-index DiffTest.
8. Add full-SoC synthesis and timing constraints.

## Work required before moving from FPGA to ASIC

1. Replace BRAM with 55nm SRAM macros.
2. Add DFT/scan and test coverage.
3. Add clock/reset/power intent.
4. Run synthesis with the 55nm library.
5. Run place-and-route and timing closure.
6. Run DRC/LVS.
7. Define package, pad ring and ESD.
8. Secure a foundry shuttle.

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

## Contributing

See [CONTRIBUTING.md](../CONTRIBUTING.md) and the workstreams:

```text
NEMU reference  -> verification, game compatibility, frame goldens
RTL             -> CPU/PPU/APU/mapper RTL and DiffTest
FPGA            -> synthesis, BRAM, timing, board
ASIC            -> 55nm flow, SRAM, DFT, physical design
```

## Non-negotiable rules

- No copyrighted ROMs or proprietary PDK files in the repository.
- No tapeout-readiness claims before timing/DFT/DRC/LVS are clean.
- No golden hash changes without review.
- No deleting tests to make a change pass.
