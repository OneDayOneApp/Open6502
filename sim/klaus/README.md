# RTL Klaus functional test harness

This directory contains a Verilator C++ harness and an Icarus testbench for
running the Klaus 6502 functional test against `rtl/cpu/open6502_cpu.sv`.

Expected pass PC: `0x3469`.

Current RTL status: fails at `PC=0x37E2` after the `CMP #$CD` fix; the
remaining failure is in the stack/PLA test area (`A=$D0`, expected `$D1`).

## Verilator

```sh
verilator --cc --exe -Wno-fatal --top-module open6502_cpu \
  -Mdir obj_dir_klaus rtl/cpu/open6502_cpu.sv sim/klaus/main.cpp
make -C obj_dir_klaus -f Vopen6502_cpu.mk \
  CXX=clang++ LINK=clang++ \
  CXXFLAGS='-isysroot <SDK>' LDFLAGS='-isysroot <SDK>' \
  LDLIBS='-lc++'
./obj_dir_klaus/Vopen6502_cpu klaus.bin
```

On the current macOS toolchain the Verilator-generated link fails to resolve
the C++ runtime symbols, so this path is not yet part of the regression suite.

## Icarus

`tb/open6502_klaus_tb.sv` runs the same test at one CPU cycle per loop
iteration. It is functionally equivalent but too slow for routine CI.

The NEMU reference already passes Klaus; this harness is for validating the RTL
CPU once the Verilator link issue is resolved.
