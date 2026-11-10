# Open6502 RTL scaffold

This is the SystemVerilog scaffold for the Open6502 SoC.  It currently contains:

- `open6502_pkg.sv`: parameters/types
- `open6502_top.sv`: top-level interface
- `video/open6502_video_timing.sv`: 341x262 NTSC timing
- `input/open6502_controller.sv`: controller serial shift register
- `dbg/open6502_difftest_bridge.sv`: event bridge skeleton
- `bus/open6502_bus.sv`: address decode/select
- `bus/open6502_wram.sv`: 2KB WRAM with mirroring
- `clocks/open6502_clocks.sv`: reset sync, CPU/PPU enables, 44.1 kHz audio enable
- `apu/open6502_apu.sv`: pulse 1/2, triangle, noise, length/envelope, frame
  sequencer, frame IRQ, `$4015` status and 16-bit sample output
- `cart/open6502_mapper.sv`: mapper 0/2/3/7 bank registers
- `dma/open6502_oam_dma.sv`: 256-byte OAM DMA engine, integrated with `$4014`
  and CPU stall
- `ppu/open6502_ppu.sv`: CPU-visible PPU registers, CHR/VRAM/OAM/palette,
  vblank/NMI, `$2007` auto-increment, background pixel fetch and simple
  sprite overlay/priority/sprite-0 hit, and coarse/fine X/Y scroll (shift
  registers still pending)
- `cpu/open6502_cpu.sv`: reset-vector/fetch front end with a directed
  immediate/zero-page/absolute/indexed/indirect load-store, basic ALU,
  branches, transfers, flags, accumulator/memory shifts, JSR/RTS,
  PHA/PLA/PHP/PLP, BRK/RTI, NMI/IRQ vectoring, INC/DEC/BIT (not the full
  6502 yet)
- `../tb/open6502_tb.sv`: video/top smoke testbench
- `../tb/open6502_bus_tb.sv`: bus/WRAM testbench
- `../tb/open6502_clocks_tb.sv`: clock-enable testbench
- `../tb/open6502_cpu_tb.sv`: CPU subset testbench
- `../tb/open6502_soc_tb.sv`: CPU + bus + WRAM SoC integration testbench

Not yet implemented: 6502 CPU, 2C02 PPU, 2A03 APU, bus decoder, cartridge
mapper, OAM DMA and audio mixer.  The top-level ports and clock/timing scaffold
are intentionally stable so those blocks can be added without changing the
board/debug interface.

## Run

```sh
cd rtl
make lint   # Verilator lint
make sim    # Icarus Verilog smoke test
```

Expected smoke output:

```text
PASS: Open6502 video timing
```

`make sim-verilator` requires a working Verilator C++ toolchain; on this host
the Verilator C++ link uses an SDK path that the installed toolchain does not
provide, so `make sim` uses Icarus Verilog.
