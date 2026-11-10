# NES/Famicom Golden Reference Test Plan

## 1. Goals

Validate more than "the game runs":

- functional CPU/PPU/APU/mapper behavior
- CPU bus, PPU dot, APU sample and mapper IRQ timing
- deterministic frame/audio/state hashes
- save/load and replay equivalence
- instruction/scanline/frame/audio DiffTest
- actionable mismatch reports

## 2. Build matrix

REF:

```sh
cd NEMU
make m6502-nesref-defconfig
make -j8
```

REF shared object:

```sh
make m6502-nesref-so-defconfig
make -j8
```

DUT:

- RTL via Verilator
- another NEMU build
- external emulator

REF and DUT must not be the same binary. Record ROM hash, mapper, input script
and initial state.

## 3. Test levels

| Level | Object | Method |
| --- | --- | --- |
| L0 | clock, hash, serialization | unit tests |
| L1 | CPU/PPU/APU/mapper | directed vectors |
| L2 | CPU + bus + DMA | CPU test ROMs |
| L3 | PPU + CPU | PPU ROMs, framebuffer hash |
| L4 | system | open-source/commercial ROMs |
| L5 | DiffTest | DUT vs REF |
| L6 | hardware | logic analyzer/capture |

## 4. CPU tests

Required:

- Klaus 6502 functional test
- nestest
- instr_test-v5
- cpu_timing_test6
- cpu_interrupts_v2
- dummy read/RMW/bus-conflict tests
- OAM DMA tests

Acceptance:

- official instructions pass
- target undocumented instructions pass
- per-instruction bus sequence matches
- NMI/IRQ/DMA cycles match
- PC/A/X/Y/SP/P/cycles match at sync points

## 5. PPU tests

Required:

- ppu_vbl_nmi
- sprite_hit_tests
- sprite_overflow_tests
- ppu_open_bus
- palette tests
- 240p Test Suite
- scroll/split screen tests

Acceptance:

- vblank/NMI at exact dots
- `$2002` read behavior
- odd-frame skip
- background fetch/shift and scroll
- sprite 0 hit at exact dot
- sprite overflow behavior
- framebuffer hash matches reference

## 6. APU tests

Required:

- apu_test / apu_reset
- pulse/triangle/noise directed tests
- frame sequencer tests
- DMC address/length/rate/loop/IRQ/DMA tests
- mixer tests

Acceptance:

- frame sequencer at exact CPU cycles
- length/envelope/sweep boundaries
- DMC DMA stall and IRQ
- deterministic sample output with defined tolerance

## 7. Mapper tests

Per mapper:

- reset state
- PRG/CHR bank boundaries
- mirroring
- PRG RAM enable/protect
- IRQ latch/reload/ack
- save/load state

T1 matrix: 0 NROM, 1 MMC1, 2 UxROM, 3 CNROM, 4 MMC3, 7 AxROM.

## 8. System/ROM tests

Free/open ROMs:

- Falling
- Galaxy Patrol
- Tower of Hanoi
- 8-Bit Table Tennis
- File Fixers
- Stallar

Commercial ROMs: metadata/hash/scripts only, local test data.

Acceptance:

- boots
- runs at least N frames without abort
- key frame hashes match
- input changes state
- audio is non-silent
- save/load continues correctly

## 9. Determinism

- same ROM + input + seed => same hashes across repeated runs
- same hashes across optimization levels
- save -> load -> continue equals uninterrupted run
- input replay reproduces hashes
- trace on/off does not change state

## 10. DiffTest

Granularity:

- instruction: CPU registers/cycles/pending
- bus: addr/data/rw/tick/source
- scanline: PPU v/t/x/w/OAM/palette
- frame: framebuffer/VRAM/OAM hash
- audio: samples/hash/tolerance
- mapper: bank/IRQ/PRG RAM

Mismatch report must include tick, CPU cycle, scanline/dot, field, REF/DUT
values, recent events, state diff and replay command.

## 11. Hardware alignment

Capture:

- CPU bus and interrupts
- PPU vblank/sprite 0/dot events
- APU channel output/DMC DMA
- controller strobe/shift

Emulator-to-emulator comparison is supplemental, not the primary evidence.

## 12. CI gates

- Debug/Release, ASan/UBSan builds
- unit tests
- CPU/PPU/APU/mapper ROMs
- determinism/hash tests
- DiffTest self-comparison
- open-source game smoke tests

## 13. Release gate (T0+T1)

- CPU tests pass
- PPU tests pass
- APU tests pass
- mapper 0/1/2/3/4/7 pass
- snapshot determinism passes
- 10^8-cycle DiffTest self-comparison has no false positives
- at least 20 ROM regression cases
- hardware timing within tolerance
- known differences documented
