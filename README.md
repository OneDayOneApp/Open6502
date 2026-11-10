# Open6502

Open6502 is a NEMU-based MOS 6502/65C02 emulator and NES/Famicom frontend, plus
an FPGA-first SystemVerilog SoC design for a NES-compatible chip.

## Repository layout

- `NEMU/` - NEMU framework, 6502/65C02 ISA port, NES frontend, Microsoft BASIC
- `NEMU/6502EMU.md` - emulator build and usage guide
- `NEMU/programs/` - sample 6502 programs
- `NEMU/tests/` - regression tests
- `NEMU/basic/` - Microsoft BASIC 1.1 port
- `NEMU/nesgame/` - bundled NROM demo game
- `roms/` - open-source NES test ROMs and launcher
- `docs/nes-golden-reference/` - NES golden-reference design and test plan
- `docs/open6502-chip/` - Open6502 SoC chip architecture and verification plan
- `rtl/` - SystemVerilog SoC scaffold

## Build and run

```sh
cd NEMU
make m6502_defconfig
make -j8

./build/6502EMU -b programs/hello.bin
./basic/run-demo.sh
./nesgame/build.sh && ./nesgame/run.sh
./build/6502EMU ../roms/falling.nes
```

Run tests:

```sh
./tests/run.sh
RUN_KLAUS=1 ./tests/run.sh
./tests/difftest/run.sh
```

## Documentation

- Emulator: [NEMU/6502EMU.md](NEMU/6502EMU.md)
- Golden reference: [design](docs/nes-golden-reference/design.md),
  [test plan](docs/nes-golden-reference/test-plan.md),
  [checklist](docs/nes-golden-reference/checklist.md),
  [feature matrix](docs/nes-golden-reference/feature-matrix.md)
- Chip design: [architecture](docs/open6502-chip/architecture.md),
  [DiffTest](docs/open6502-chip/difftest.md),
  [verification](docs/open6502-chip/verification.md),
  [checklist](docs/open6502-chip/checklist.md)
- Changelog: [CHANGELOG.md](CHANGELOG.md)
