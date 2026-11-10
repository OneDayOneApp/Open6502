# NEMU-based NES/Famicom Golden Reference Design

## 1. Scope

Build a deterministic, observable NES/Famicom reference on top of NEMU:

- CPU bus-cycle, PPU dot, APU sample and mapper event observability
- state export and deterministic replay
- REF executable and `TARGET_SHARE` REF `.so`
- NTSC first, PAL later
- T0+T1 as the default "complete" scope:
  - T0: 2A03, 2C02, bus, standard controllers, NMI/IRQ/DMA
  - T1: mappers 0/1/2/3/4/7 and PRG RAM
  - T2: MMC5/VRC/Namco 163/FME-7, Zapper, Four Score, expansion audio
  - T3: PAL, chip revisions, clones

## 2. NEMU build profiles

### `m6502-nesref_defconfig`

REF executable:

```text
CONFIG_ISA_m6502=y
CONFIG_NES=y
CONFIG_NES_REF=y
CONFIG_MODE_SYSTEM=y
CONFIG_TARGET_NATIVE_ELF=y
CONFIG_DIFFTEST=y
```

### `m6502-nesref-so_defconfig`

REF shared object:

```text
CONFIG_ISA_m6502=y
CONFIG_NES=y
CONFIG_NES_REF=y
CONFIG_TARGET_SHARE=y
CONFIG_MSIZE=0x10000
CONFIG_MBASE=0x0
```

NES reset comes from the cartridge `$FFFC/$FFFD`, not NEMU's raw-image reset
offset.

## 3. Module layout

```text
NEMU/src/nes/
  ref/       REF entry, state, frame loop, hash
  bus/       address map, open bus, DMA
  ppu/       2C02 dot pipeline
  apu/       2A03 channels, frame sequencer, DMC
  cart/      iNES/NES2/UNIF
  mapper/    mapper implementations
  input/     controllers and accessories
  trace/     bus/PPU/APU trace
  state/     snapshot/hash
```

## 4. Clocking

| Platform | Master | CPU | PPU |
| --- | ---: | ---: | ---: |
| NTSC | 21.477272 MHz | /12 | /4 |
| PAL | 26.601712 MHz | /16 | /5 |

Use integer phases and a master tick. Do not accumulate floating-point time.
The CPU executes instructions through NEMU, while the NES REF adds bus-cycle
callbacks to drive PPU/APU/mapper logic in a fixed order.

## 5. Bus

```text
$0000-$1FFF  2KB WRAM, four mirrors
$2000-$3FFF  PPU registers, 8-byte mirrors
$4000-$4017  APU, OAM DMA, controllers
$4018-$401F  test/expansion
$4020-$5FFF  expansion
$6000-$7FFF  PRG RAM
$8000-$FFFF  cartridge PRG
```

Model open bus, register side effects, OAM/DMC DMA and mapper remapping.

## 6. CPU: 2A03 profile

- official 6502 instructions
- stable undocumented instructions used by software
- no decimal mode
- bus-cycle events: fetch, operands, dummy reads, RMW writes
- NMI/IRQ sampling
- OAM DMA and DMC DMA stalls
- NES reset vector

## 7. PPU: 2C02/2C07

- 341 dots x 262 lines NTSC; 312 lines PAL
- vblank/NMI, `$2002` clear, `$2007` buffer/increment
- `v/t/x/w`, fine/coarse scroll
- background fetch/shift
- sprite evaluation, 8x8/8x16, flip, priority, clipping
- sprite 0 hit and overflow
- OAM/`$2004` and OAM DMA
- palette, mirroring, open bus
- odd-frame skip
- framebuffer/VRAM/OAM hashes

## 8. APU: 2A03/2A07

- pulse x2, triangle, noise, DMC
- frame sequencer 4/5 step
- length/envelope/sweep
- DMC sample address/length/rate/loop/IRQ/DMA/output
- nonlinear mixer
- 44.1 kHz deterministic output

## 9. Cartridge/mappers

T1: NROM, MMC1, UxROM, CNROM, MMC3, AxROM.

Each mapper needs:

- PRG/CHR banking boundaries
- mirroring
- PRG RAM enable/protect
- IRQ trigger/ack
- reset state
- save/load round trip

T2/T3 mappers and accessories are separate acceptance levels.

## 10. State, trace and hashes

Snapshot:

- CPU registers/cycles/pending interrupts
- WRAM, PPU VRAM/OAM/palette, APU channels, mapper state
- master tick, PPU dot, frame
- controller state and RNG

Hashes:

- CPU, PPU, APU, mapper, frame, audio and full-system hash
- xxHash/FNV for runtime, SHA-256 for archived snapshots

Trace:

- level 0: frame/audio summary
- level 1: CPU bus + PPU/APU registers
- level 2: PPU dot/APU channel events
- level 3: full state snapshots

## 11. DiffTest

CPU:

- `difftest_regcpy` and `difftest_exec`
- `isa_difftest_checkregs`

NES extension exported by the REF `.so`:

```c
nes_difftest_load_rom()
nes_difftest_get_state()
nes_difftest_state_hash()
nes_difftest_frame_hash()
nes_difftest_get_video()
nes_difftest_get_audio()
nes_difftest_step_frame()
```

NEMU's stock `init_difftest()` copies a raw image to `RESET_VECTOR`; that is not
enough for iNES cartridges. A NES-specific attach/load path is required.

## 12. NEMU adaptation points

| File | Change |
| --- | --- |
| `include/difftest-def.h` | `DIFFTEST_REG_SIZE` must match `m6502_CPU_state` |
| `src/isa/m6502/reg.c` | `isa_difftest_checkregs`, register copy |
| `src/cpu/difftest/ref.c` | m6502 memcpy/regcpy/exec; NES exports |
| `src/cpu/difftest/dut.c` | NES load/attach and PPU/APU sync |
| `src/monitor/monitor.c` | NES REF init, no raw-image misuse |
| `src/memory/vaddr.c` | route to NES bus when active |
| `src/isa/m6502/inst.c` | 2A03 profile and bus-cycle callback |
| `src/nes/ref/` | REF entry, frame loop, state, hash, trace |
| `tests/nes/` | REF/DUT regression |

## 13. Milestones

1. NEMU REFs, bus and state/hash/trace.
2. 2A03 cycle-level CPU and CPU DiffTest.
3. 2C02 dot pipeline and frame hashes.
4. 2A03 APU and audio hash.
5. T1 mappers and state.
6. NES extended DiffTest and RTL/FPGA DUT.
7. hardware alignment, compatibility and release.

The first verifiable milestone is CPU-only Klaus/nestest against a separate
build, then REF/frontend frame parity, then extended PPU/APU/mapper DiffTest.

## 14. Current relationship to 6502EMU

- `6502EMU` is the platform frontend and smoke-test target.
- `src/nes/ref` is the reference core used by tests.
- `src/isa/m6502` supplies the CPU; the 2A03 profile is still being separated.
- All golden-reference decisions belong in `src/nes` plus NEMU DiffTest, not in
  the SDL window.
