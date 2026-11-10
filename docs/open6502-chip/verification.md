# Open6502 Verification Plan

## 1. Levels

| Level | Object | Method |
| --- | --- | --- |
| L0 | package/decode/counters | SV unit tests |
| L1 | CPU/PPU/APU blocks | directed tests + vectors |
| L2 | CPU + bus | Klaus, nestest, DiffTest |
| L3 | PPU + CPU | PPU ROMs, framebuffer hash |
| L4 | APU | apu_test, DMC, sample hash |
| L5 | mapper | bank/IRQ test ROMs |
| L6 | system | open-source games, hardware |
| L7 | FPGA | board video/audio/input |

## 2. Required tests

- Klaus 6502 functional test
- nestest
- instr_test-v5
- cpu_timing_test6
- cpu_interrupts_v2
- ppu_vbl_nmi
- sprite_hit_tests
- sprite_overflow_tests
- ppu_open_bus
- apu_test / apu_reset / DMC
- 240p Test Suite
- open-source ROMs: Falling, Galaxy Patrol, Hanoi, Table Tennis, File Fixers,
  Stallar

## 3. DiffTest gates

- CPU-only: Klaus + hello with zero mismatches
- frame-only: NROM game 60/300-frame hashes match
- scanline: PPU test ROMs match
- audio: APU sample hash/tolerance matches
- mapper: bank/IRQ state matches

## 4. FPGA validation

- stable 256x240 RGB output
- 60 Hz frame rate
- clean audio
- 1P/2P controllers
- ROM load/reset
- at least one hour of continuous operation

## 5. Hardware alignment

Requires:

- logic analyzer/bus captures
- PPU dot/scanline events
- APU channel output
- DMC DMA

No hardware is available in the current environment; deliver the capture format
and comparison tooling first.

## 6. Release gates

v0 FPGA prototype:

- CPU Klaus
- PPU vblank/NMI/sprite 0
- APU four channels
- mappers 0/1/2/3/4/7
- six open-source ROMs playable
- CPU + frame DiffTest passes

ASIC additionally requires DFT, timing closure, power, package and hardware
alignment.
