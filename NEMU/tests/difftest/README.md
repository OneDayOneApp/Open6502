# NEMU CPU-only DiffTest

This test builds two NEMU configurations:

- REF: `m6502-nesref-so_defconfig` -> `build/6502EMU-so`
- DUT: `m6502-nes-dut_defconfig` -> `build/6502EMU`

It then runs the same raw 6502 image with `--diff=...` and checks:

- the DUT reaches `HIT GOOD TRAP`
- guest output appears exactly once (REF host I/O is suppressed)
- CPU registers and cycles match at every instruction

Run explicitly because it rebuilds the NEMU configuration:

```sh
./tests/difftest/run.sh
```

The initial milestone is CPU-only. NES cartridge/PPU/APU state synchronization is
the next DiffTest layer.
