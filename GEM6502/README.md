# GEM6502

GEM6502 is a **fast C cycle model** for the Open6502 SoC. It borrows the useful
parts of GEM5's structure without trying to be GEM5:

- a global event queue,
- explicit clock/cycle accounting,
- SimObject-like CPU/PPU/APU/bus separation,
- statistics and trace hooks,
- deterministic cycle counts for 6502 instructions.

It is intended for:

- fast boot/regression of 6502 programs without an RTL simulator,
- cycle-budget exploration before RTL changes,
- NEMU/RTL model comparison,
- CI smoke tests that need a programmable 6502 SoC model.

It is **not** a replacement for NEMU, Verilator, or signoff simulation.

## Build

```sh
cd GEM6502
make
make test
```

The test binary is `tests/test_gem6502`; the CLI is `./gem6502`.

## Run

```sh
./gem6502 run <prg.bin> [max_cycles] [--trace]
./gem6502 stats <prg.bin>
```

`prg.bin` is a raw NROM PRG image loaded at `$8000`. The reset vector is read
from `$FFFC/$FFFD`, so a 32 KiB NES PRG can be used directly if it is NROM.

Example:

```sh
./gem6502 run /tmp/gem-demo.bin 1000
```

Output:

```text
GEM6502 0.1.0
pc=8037 a=15 x=00 y=00 sp=FD p=24
cycles=101 inst=49 reads=57 writes=1 frames=0 nmi=0 illegal=0
stop=cycle limit
```

## GEM5 mapping

| GEM5 concept | GEM6502 implementation |
|---|---|
| SimObject | `GemCpu`, `GemPpu`, `GemApu`, `GemBus` |
| Event queue | `GemEventQueue` min-heap |
| Clock domain | CPU cycles drive PPU/APU advancement |
| Statistics | `GemStats` |
| Trace | CPU trace flag / bus counters |
| Memory object | RAM, PRG, CHR, VRAM, OAM, palette arrays |
| Checkpointing | not implemented yet |

## Architecture

```text
main.c
  gem_create / gem_load_prg / gem_run

gem6502.c
  cpu_step       6502 instruction + cycle count
  gem_bus_read   RAM / PPU / APU / cartridge decode
  gem_bus_write  PPU/APU/$4014/OAM DMA/controller
  gem_ppu_advance  CPU cycles -> PPU dots -> vblank/NMI
  gem_apu_advance  APU cycle counter
  gem_run_events  event queue dispatch
```

The PPU is advanced lazily by `cpu_cycles * 3`, not by one event per dot. This
is the key speed tradeoff: the model is fast, but it does not model per-dot
PPU/CPU interleaving exactly.

## Instruction coverage

Implemented NMOS 6502 official instructions and common NOPs:

- loads/stores, all documented addressing modes
- ADC/SBC with binary and decimal mode
- AND/ORA/EOR, CMP/CPX/CPY, BIT
- INC/DEC, ASL/LSR/ROL/ROR
- branches with page-cross cycles
- JMP/JSR/RTS, BRK/RTI
- PHA/PLA/PHP/PLP
- transfers and flag instructions
- NOP

The opcode table is in `src/gem6502.c`. Undocumented opcodes and most 65C02
extensions are intentionally illegal.

## Current limitations

- No IRQ model; NMI from PPU vblank only.
- No full PPU background/sprite renderer, no framebuffer output.
- No APU audio synthesis; register file only.
- No mapper banking; NROM only in v0.1.
- No cycle-exact bus stalls for every PPU register access.
- No checkpoint/restore.
- No C++/Python bindings.

These are deliberate v0.1 boundaries.

## Tests

```sh
make test
```

Current unit tests:

```text
PASS: GEM6502 ALU/branch/cycle count
PASS: GEM6502 JSR/RTS stack
PASS: GEM6502 PPU vblank/NMI
ALL GEM6502 TESTS PASS
```

### NEMU golden-state diff

```sh
make difftest
```

This runs directed programs in both NEMU and GEM6502, then compares the full
state trace: PC, A, X, Y, SP, P (B/U masked), and zero-page checkpoints.

Current cases:

```text
alu       PASS
decimal   PASS
stack     PASS
branch    PASS
rmw       PASS
brk       PASS
```

Each case currently matches for 25-35 instructions.

## Roadmap

1. Add IRQ and NMI/IRQ priority tests.
2. Add mapper 0/1/2/3/4/7 banking.
3. Add PPU background/sprite rasterizer and frame hash.
4. Add APU frame sequencer and audio sample output.
5. Add checkpoint/restore.
6. Add cocotb/C API bindings.
7. Add NEMU/RTL/GEM6502 three-way trace comparison.
8. Feed GEM6502 timing data back into RTL verification.
