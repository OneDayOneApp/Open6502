# Changelog

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
