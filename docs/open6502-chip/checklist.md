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
- [ ] vblank/NMI.
- [ ] background fetch/shift.
- [ ] sprite evaluation.
- [ ] sprite 0 hit.
- [ ] overflow.
- [ ] OAM DMA.
- [ ] palette/mirroring.
- [~] RGB444/VGA timing (counter only; pixel path missing).
- [ ] PPU test ROMs.
- [ ] frame hash DiffTest.

## P3 APU

- [ ] pulse x2.
- [ ] triangle.
- [ ] noise.
- [ ] DMC.
- [ ] frame sequencer.
- [ ] mixer.
- [ ] 44.1kHz output.
- [ ] apu_test/DMC.
- [ ] audio hash DiffTest.

## P4 cartridge/mapper/input

- [ ] cartridge interface.
- [ ] mappers 0/1/2/3/4/7.
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
