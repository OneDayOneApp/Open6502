# Synthesis and Manufacturing Cost Estimate

This document turns the Yosys area report into a planning-level cost model.
It is not a foundry quote and not a signoff result.

## Yosys area summary

Generic-cell counts from `docs/open6502-chip/synthesis-area.md`:

| Block | Generic cells |
|---|---:|
| CPU | 4,138 |
| APU + DMC | 2,683 |
| Mapper | 745 |
| OAM DMA | 154 |
| WRAM | 37,003 |
| Bus/clock/video/controller | 779 |
| PPU | 975,919 |
| **Total** | **1,021,421** |

The PPU number is dominated by expanded CHR/VRAM/OAM/palette storage:
84,224 bits, currently implemented as flip-flops and read muxes.

## 55nm planning estimate

Assumptions:

```text
NAND2 equivalent: 2.0-3.0 um^2
DFF: 10-15 um^2
6T SRAM bit: 0.35-0.55 um^2
standard-cell utilisation: 60-70%
IO/PAD/ESD: 0.3-0.8 mm^2
```

### Scenario A: current RTL, memory as flip-flops

```text
die area: 4-7 mm^2
```

This is not recommended for production.

### Scenario B: SRAM + sequential sprite/PPU refactor

```text
core logic: 0.4-0.7 mm^2
SRAM: ~0.1 mm^2
IO/PAD/ESD: 0.3-0.8 mm^2
die: 1.2-2.0 mm^2
```

Planning target:

```text
1.5 mm^2 die
```

## Wafer and unit-cost model

Planning assumptions for a 300 mm wafer:

```text
usable wafer area: ~70,000 mm^2
55nm wafer price: USD 2,500-4,500
die area: 1.5-2.0 mm^2
gross dies: 35,000-46,000
yield: 70-85%
```

Gross die cost:

```text
USD 0.05-0.13 per die
```

Add packaging, test and yield loss:

```text
package + test: USD 0.20-0.50
unit cost at volume: roughly USD 0.3-0.7
```

For the unoptimised 4-7 mm^2 RTL:

```text
gross dies: 10,000-17,500
die cost: USD 0.15-0.45
unit cost: roughly USD 0.5-1.5
```

## NRE

55nm mask set, EDA, IP and bring-up are the dominant early cost:

```text
mask set: roughly USD 150k-400k
EDA/IP/board/test engineering: project dependent
```

## Why the SRAM refactor matters

Current PPU storage:

```text
CHR   65,536 bit
VRAM  16,384 bit
OAM    2,048 bit
PAL      256 bit
total 84,224 bit
```

As flip-flops:

```text
84,224 x ~12 um^2 = ~1.0 mm^2
```

As SRAM:

```text
84,224 x ~0.45 um^2 = ~0.04 mm^2
```

That single change is the largest area/cost lever.

## Next steps for a real number

1. Select a 55nm PDK and standard-cell library.
2. Run synthesis with the vendor library.
3. Generate SRAM macros with the memory compiler.
4. Run place-and-route and timing closure.
5. Get a foundry/OSAT quote for the chosen package and volume.
