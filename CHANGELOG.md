# Changelog

## v0.7.0 - REF PPU/APU frame loop

### Added

- PPU bus callbacks, so the same 2C02 core can be driven by the frontend cart or
  the REF mapper.
- REF PPU/APU instances and CPU bus routing for:
  - PPU registers
  - APU registers
  - OAM DMA
  - controller ports
- `nes_ref_machine_run_frame()`, frame hash and audio hash.
- REF frame test against `nesgame/game.nes`.
- Frontend/REF 60-frame hash comparison in regression.

### Verified

- REF and frontend produce identical 60-frame frame/audio hashes for the bundled
  NROM game:
  - frame `67667b1cbc4c1cf3`
  - audio `e78422afbcb92ac3`

### Verification

- default regression, `RUN_KLAUS=1`, and CPU-only NEMU DiffTest.


## v0.6.0 - REF MMC3 scanline IRQ

### Added

- REF `nes_ref_machine_scanline()` for mapper 4.
- MMC3 IRQ latch, reload, enable, acknowledge and IRQ flag.
- CPU `irq_pending` propagation from the mapper IRQ line.
- Unit test covering latch/reload/enable/ack.

### Verification

- default regression, `RUN_KLAUS=1`, and CPU-only NEMU DiffTest.


## v0.5.0 - REF mapper state

### Added

- NEMU REF iNES mapper support for:
  - NROM (0)
  - MMC1 (1)
  - UxROM (2)
  - CNROM (3)
  - MMC3 (4)
  - AxROM (7)
- PRG/CHR bank switching, mirroring state, PRG RAM and mapper state hash.
- REF unit tests for UxROM, CNROM, MMC1, MMC3 and AxROM bank selection.

### Verification

- default regression, `RUN_KLAUS=1`, and CPU-only NEMU DiffTest.


## v0.4.0 - deterministic input replay

### Added

- NES input recording:
  - `--record-input=FILE`
  - binary format: `NESINP01` + 2 bytes per frame (1P, 2P)
- NES input replay:
  - `--replay-input=FILE`
  - replay drives both controller ports deterministically
- Regression:
  - record 60 frames with `--hold=right`
  - replay the file
  - compare frame/audio hashes with the original run

### Fixed

- Corrected `--record-input` / `--replay-input` option prefix parsing.

### Verification

- default regression, `RUN_KLAUS=1`, and CPU-only NEMU DiffTest.


## v0.3.0 - PPU timing closure

### Added

- PPU timing unit tests:
  - reset state
  - vblank/NMI at scanline 241
  - `$2002` read clearing
  - pre-render frame boundary
  - odd-frame skip when rendering is enabled
  - no skip when rendering is disabled
- NES state snapshots are covered by the same PPU timing changes.

### Fixed

- Implemented odd-frame skip in the 2C02 state machine.
- Removed an unused PPU render parameter warning exposed by the unit-test build.

### Verification

- `tests/nes_ppu/run.sh`
- default regression, `RUN_KLAUS=1`, and CPU-only NEMU DiffTest.


## v0.2.0 - state snapshot closure

### Added

- NES state snapshot save/load:
  - CPU registers and cycles
  - 2 KB CPU RAM
  - PPU state and framebuffer
  - APU channels, frame sequencer and sample buffer
  - mapper registers, PRG RAM and CHR RAM
  - controller state and hash accumulator
- `--save-state=FILE` and `--load-state=FILE` for NES mode.
- Regression test: save at frame 60, resume 60 frames, compare frame/audio
  hashes with an uninterrupted 120-frame run.
- Headless mode no longer initializes SDL video/audio.

### Fixed

- Headless `--hold` now affects controller state.
- Clean-clone builds export `NEMU_HOME` for Kconfig sub-makes.

### Verification

- Default regression, `RUN_KLAUS=1`, and CPU-only NEMU DiffTest pass.

## v0.1.0 - first functional closure

### Emulator

- NEMU-based MOS 6502 core, 65C02 extension mode.
- Microsoft BASIC 1.1 on the raw 6502 monitor path.
- NES/Famicom frontend:
  - 2C02 PPU rendering, NMI, OAM DMA, palette, scanline timing.
  - 2A03 APU pulse/triangle/noise, WAV recording.
  - iNES/NROM, MMC1, UxROM, CNROM, MMC3, AxROM mappers.
  - SDL2 video/audio and 1P/2P keyboard input.
- Bundled NROM paddle-ball game with static playfield and short collision beep.

### Verification

- CPU/65C02/BASIC/NES regression tests.
- Klaus 6502 functional test (NEMU frontend and REF CPU).
- Deterministic headless NES frame/audio hashes:
  - identical runs produce identical hashes
  - input changes the frame hash
- Clean-clone build verified.

### Golden-reference work

- NEMU `m6502-nesref-so_defconfig` REF shared object.
- NEMU `m6502-nes-dut_defconfig` CPU-only DUT.
- `isa_difftest_checkregs` and `difftest_*` for m6502.
- CPU-only DiffTest through `tests/difftest/run.sh`.
- REF clock/ROM/state/NROM bus/CPU modules and unit tests.

### Known limitations

- NES PPU is scanline-level, not dot-exact.
- APU DMC and expansion audio are not implemented.
- NES DiffTest does not yet compare PPU/APU/mapper state.
- Mapper support is limited to 0/1/2/3/4/7.
- Golden reference is not yet a complete full-chip reference.
