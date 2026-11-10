# Open6502 Chip Design

Open6502 is an FPGA-first, NES/Famicom-compatible 8-bit game SoC.

- DUT: SystemVerilog RTL
- REF: the NEMU NES golden-reference core
- Verification: Verilator/Icarus + FPGA + NEMU DiffTest
- First target: NTSC, mappers 0/1/2/3/4/7, standard controllers, RGB video,
  digital audio

## Documents

- [Architecture](architecture.md)
- [DiffTest protocol](difftest.md)
- [Verification plan](verification.md)
- [Checklist](checklist.md)

## RTL scaffold

`rtl/` currently contains a lint/simulation-clean SoC top, 341x262 video
timing, a controller shift register, a DiffTest bridge skeleton, and a
bus/WRAM decoder, a clock/reset/enable generator, and a 6502 reset/fetch front
end with a small directed opcode subset. The full CPU, PPU, APU and mappers are
not implemented yet.

```sh
cd rtl
make lint
make sim
```

## Principles

1. RTL is the only synthesizable implementation; NEMU is not synthesized.
2. NEMU is the REF, not the DUT.
3. Build CPU + bus first, then PPU/APU, then mappers/accessories.
4. FPGA prototype before ASIC.
5. Every external interface has an observable trace for DiffTest.

## v0 scope

| Block | v0 scope |
| --- | --- |
| CPU | 6502/2A03, official instructions + target stable undocumented ones |
| PPU | 2C02-compatible, dot pipeline, 256x240, RGB444 |
| APU | 2A03 pulse/triangle/noise/DMC, 44.1 kHz digital output |
| Bus | WRAM/PPU/APU/OAM DMA/controllers/PRG RAM |
| Cartridge | iNES mappers 0/1/2/3/4/7 |
| Input | two standard controllers |
| Platform | NTSC 21.477272 MHz |
| Debug | UART/JTAG + DiffTest trace |

PAL, T2/T3 mappers, Zapper/Four Score and analog video/audio are post-v0.
