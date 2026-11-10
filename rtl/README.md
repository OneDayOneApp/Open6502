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
- `cpu/open6502_cpu.sv`: reset-vector/fetch front end with a directed
  immediate/zero-page/absolute and indexed load/store, basic ALU, branches,
  transfers, flags, accumulator shifts (not the full 6502 yet)
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
