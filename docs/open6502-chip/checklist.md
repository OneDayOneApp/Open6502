# Open6502 Chip Checklist

`[ ]` not started, `[~]` partial, `[x]` done.

## P0 specification

- [x] v0 scope: NTSC, mappers 0/1/2/3/4/7, standard controllers.
- [x] clock/reset strategy and RTL clock/enable generator.
- [x] address map.
- [x] top-level ports.
- [x] DiffTest topology.
- [ ] freeze register/memory map v0.
- [ ] freeze video/audio electrical interfaces.

## P1 CPU + bus

- [~] 6502 RTL core: immediate/zero-page/absolute/indexed/indirect load-store, ADC/SBC/AND/ORA/EOR/CMP, branches/transfers/flags, accumulator/memory shifts, JSR/RTS, PHA/PLA/PHP/PLP, BRK/RTI, NMI/IRQ vectoring, INC/DEC/BIT, integrated in top.
- [ ] 2A03 profile (no decimal).
- [~] bus address decode/WRAM in RTL; CPU cycle semantics pending.
- [ ] NMI/IRQ.
- [ ] OAM DMA stall/steal.
- [x] 2KB WRAM + mirrors (RTL bus test).
- [ ] UART trace.
- [ ] Klaus/nestest RTL simulation.
- [ ] CPU-only NEMU DiffTest.

## P2 PPU

- [~] 341x262 dot counter (RTL video timing scaffold + smoke test).
- [~] vblank/NMI register block; pixel pipeline pending.
- [~] background pixel fetch + coarse/fine scroll; fetch/shift registers pending.
- [~] simple per-pixel sprite overlay; evaluation/8x16/overflow pending.
- [~] sprite-0 hit flag in simple overlay; exact dot/scroll behavior pending.
- [ ] overflow.
- [x] OAM DMA engine + `$4014` CPU stall/top integration.
- [~] palette/VRAM/OAM blocks; mirroring/render pending.
- [~] RGB444/VGA timing plus PPU palette-index output; real palette encoder pending.
- [ ] PPU test ROMs.
- [ ] frame hash DiffTest.

## P3 APU

- [~] pulse x2/triangle/noise RTL + status test; sweep and DMC pending.
- [ ] triangle.
- [ ] noise.
- [ ] DMC.
- [ ] frame sequencer.
- [ ] mixer.
- [~] 16-bit sample output wired to `aud_sample`; audio-rate strobe exists.
- [ ] apu_test/DMC.
- [ ] audio hash DiffTest.

## P4 cartridge/mapper/input

- [~] cartridge/mapper interface integrated for mapper 0; banked top tests pending.
- [~] mappers 0/1/2/3/4/7 RTL + top hookup; MMC3 A12 filter and banked top tests pending.
- [ ] PRG RAM/battery.
- [ ] MMC3 IRQ.
- [ ] 1P/2P controllers.
- [ ] iNES loader.
- [ ] six open-source ROM regression.

## P5 DiffTest/FPGA

- [ ] `open6502_difftest_bridge` RTL.
- [ ] Verilator DPI bridge.
- [ ] mismatch report/replay.
- [ ] FPGA synthesis/timing.
- [ ] VGA/HDMI output.
- [ ] I2S/sigma-delta audio.
- [ ] one-hour stability.

## P6 later

- [ ] PAL.
- [ ] T2/T3 mappers.
- [ ] Zapper/Four Score.
- [ ] hardware capture alignment.
- [ ] ASIC DFT/timing/power.
