# Changelog

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
