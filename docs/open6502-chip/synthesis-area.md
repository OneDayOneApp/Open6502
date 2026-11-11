# Open6502 Yosys Synthesis and Area

## Flow

Per-module synthesis was run with Yosys 0.60:

```sh
yosys -s rtl/scripts/synth_open6502.ys
```

The flow:

```text
read_verilog -sv -DSYNTHESIS
hierarchy -check
synth -top <module> -flatten -noabc
stat
```

`-noabc` is used because the full PPU combinational renderer is too large for
ABC mapping in practical time. Counts below are **generic Yosys cells**, not
FPGA LUTs and not ASIC µm².

## Per-module area

| Module | Generic cells |
|---|---:|
| `open6502_cpu` | 4,138 |
| `open6502_apu` | 1,855 |
| `open6502_dmc` | 828 |
| `open6502_mapper` | 745 |
| `open6502_oam_dma` | 154 |
| `open6502_wram` | 37,003 |
| `open6502_bus` | 171 |
| `open6502_clocks` | 360 |
| `open6502_video_timing` | 164 |
| `open6502_controller` | 19 |
| `open6502_difftest_bridge` | 65 |
| `open6502_ppu` | 975,919 |
| **Hierarchical sum** | **1,021,421** |

## PPU breakdown

The PPU is the dominant block:

```text
975,919 cells
 84,224 flip-flops
840,701 muxes
 40,283 AND
  9,301 OR
    674 XOR
```

Most of the PPU area is the expanded `chr[8192]`, `vram[2048]`, `oam[256]`
and `pal[32]` storage plus the per-pixel/attribute mux trees. A real FPGA flow
must map these to block RAM and reduce the combinational sprite scan.

## Important caveats

1. This is a **generic-cell estimate**, not placed-and-routed area.
2. `SYNTHESIS` skips memory reset loops; simulation still initialises them.
3. `SYNTHESIS` limits the PPU sprite scan to 8 sprites for runtime reasons.
   Simulation still scans all 64.
4. Full flattened `open6502_top` synthesis did not finish within 600 seconds;
   the table is the sum of per-module runs.
5. No timing closure, LUT mapping, BRAM mapping or ASIC standard-cell mapping
   has been done.

## Conclusion

The RTL is functionally synthesizable per module, but not yet a
production-ready FPGA/ASIC area result. The CPU is small (~4k generic cells),
while the current PPU ROM/RAM expansion dominates the area. The next area
step is to replace the PPU register arrays with BRAM and rewrite sprite
selection as a sequential scan.
