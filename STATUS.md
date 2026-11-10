# Open6502 Status

## Verified

- 23 RTL unit/integration tests pass.
- `make lint` passes.
- NEMU/RTL final-state DiffTest passes.
- NEMU/RTL instruction PC trace passes.
- NEMU/RTL ALU/stack DiffTest passes.
- NEMU 6502 Klaus passes.
- Repository Markdown documentation is English-only.

## RTL blocks present

- 6502 CPU with most official addressing modes and instructions
- system bus and 2 KiB WRAM
- PPU registers, CHR/VRAM/OAM/palette
- PPU background fetch, scroll, sprites, 8x16, overflow
- OAM DMA with `$4014` and CPU stall
- APU pulse/triangle/noise/frame IRQ/sweep
- DMC core with sample output and DMA stall
- mapper 0/1/2/3/4/7 and top-level mapper integration
- VGA/sigma-delta FPGA wrapper
- NEMU/RTL DiffTest harness
- Verilator/Icarus RTL Klaus harness

## Not complete

- RTL Klaus fails at `PC=0x09DC` (BRK/PLP flag/stack interaction).
- No cycle-level online bus/PPU/APU/mapper DiffTest.
- DMC bus arbitration/cycle alignment incomplete.
- PPU fetch/shift and sprite evaluation are not cycle-exact.
- MMC3 A12 filtering is approximate.
- Full Yosys synthesis/timing closure has not completed.
- No FPGA board validation or hardware capture alignment.
- Remaining CPU timing/DMA details are approximate.

## Next gates

1. Pass RTL Klaus at `PC=0x3469`.
2. Add online bus/PPU/APU/mapper DiffTest.
3. Complete DMC arbitration and PPU shift/evaluation timing.
4. Complete Yosys synthesis and FPGA timing/board validation.

This repository is not yet a complete, verified Open6502 chip.
