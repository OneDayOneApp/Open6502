# NES/Famicom Golden Reference Checklist (NEMU-based)

`[ ]` not started, `[~]` partial, `[x]` done with evidence.

## A. Scope and NEMU strategy

- [~] Target: NTSC runs; PAL clock only.
- [ ] Chip revision profiles (2A03G/H, 2A07, 2C02G/H, 2C07).
- [x] T0/T1/T2/T3 boundaries documented.
- [~] Mapper matrix: T1 listed; T2/unlicensed not frozen.
- [ ] Accessories: Zapper, Four Score, microphone.
- [x] Audio/video acceptance criteria documented.
- [x] REF/DUT separation strategy documented.
- [~] NEMU configs: `m6502-nesref-so_defconfig`, `m6502-nes-dut_defconfig`;
      missing `m6502-nesref_defconfig` and `CONFIG_NES_REF`.
- [~] SDB/state/trace v1: state format exists; trace missing.
- [~] Test ROM metadata: Klaus/open games; full matrix missing.
- [x] Copyright policy: no unauthorized ROMs.

## B. NEMU REF scaffold

- [ ] `configs/m6502-nesref_defconfig` executable.
- [x] `configs/m6502-nesref-so_defconfig`.
- [~] `CONFIG_NES_REF` / `CONFIG_NES_DIFFTEST`.
- [ ] `make m6502-nesref_defconfig && make`.
- [x] `make m6502-nesref-so_defconfig && make`.
- [~] REF entry: `src/nes/ref` API; not wired to `init_monitor()`.
- [x] `src/nes/ref`: ROM, reset, CPU/PPU/APU frame loop, state hash.
- [x] master tick (NTSC/PAL integer division).
- [x] CPU/PPU/APU phases in frame loop.
- [~] Bus/open bus in `machine.c`; no separate bus/trace module.
- [x] Snapshot/hash: frontend snapshot + REF state hash.
- [ ] `src/nes/trace`.
- [ ] SDB `nes info/frame/audio/state/hash`.
- [ ] REF headless runner.
- [~] ASan/UBSan: REF tests passed manually; no fixed CI.
- [ ] CI.

## C. CPU 2A03

- [~] m6502 core; separate 2A03 profile missing (decimal still exists).
- [x] official 151 instructions; Klaus PC=3469.
- [~] undocumented instruction profile.
- [ ] bus-cycle fetch/operand/dummy read events.
- [~] RMW double write; no dedicated test.
- [~] branch/page-cross/zero-page wrap.
- [~] JMP indirect page bug; no test.
- [~] NMI/IRQ sampling.
- [~] BRK/RTI/PHP/PLP flags.
- [~] OAM DMA stall/alignment.
- [~] DMC DMA stall.
- [x] reset vector/state.
- [x] `isa_difftest_checkregs`.
- [x] CPU state/cycles/pending in snapshot and REF hash.
- [x] Klaus.
- [ ] nestest/instr_test-v5.
- [ ] cpu_timing_test6.
- [ ] cpu_interrupts_v2.
- [ ] bus trace alignment.

## D. PPU 2C02

- [~] dot loop: 341x262; per-dot background; sprites scanline-composed.
- [x] vblank/NMI dot position.
- [x] `$2002` clear/w latch.
- [~] v/t/x/w and fine/coarse scroll.
- [~] background fetch/shift.
- [~] sprite evaluation/8x16/flip/priority/clipping.
- [~] sprite 0 hit: dot check + unit test; background still scanline-derived.
- [~] sprite overflow.
- [~] OAM/`$2004`/OAM DMA.
- [~] `$2007` buffer/increment.
- [~] palette/mirror/open bus.
- [x] nametable mirroring/four-screen.
- [x] odd-frame skip.
- [~] rendering enable effects.
- [x] framebuffer/VRAM/OAM hash.
- [x] NMI/IRQ to CPU.
- [~] external PPU ROMs.
- [ ] 240p Test Suite.

## E. APU 2A03

- [~] pulse 1/2.
- [~] triangle.
- [~] noise.
- [~] DMC: address/length/rate/loop/IRQ/output; no CPU DMA stall.
- [~] frame sequencer 4/5-step.
- [~] frame IRQ/`$4015`.
- [~] nonlinear mixer.
- [x] 44.1kHz output.
- [x] audio hash/tolerance.
- [~] apu_test/apu_reset.
- [~] DMC self-test; external DMC ROMs missing.
- [~] channel state export.
- [ ] long-run audio stability.

## F. Bus/input

- [x] 2KB WRAM + four mirrors.
- [x] PPU register 8-byte mirrors.
- [~] APU/IO map.
- [~] open bus.
- [~] OAM DMA bus conflicts.
- [x] standard controller strobe/shift.
- [x] 2P port.
- [ ] Zapper.
- [ ] Four Score.
- [ ] Famicom expansion/microphone.
- [x] input replay.
- [x] input save/load (frontend).

## G. Cartridge/mappers

- [x] iNES.
- [ ] NES 2.0.
- [~] header validation/mapper number.
- [x] PRG/CHR/CHR RAM.
- [x] PRG RAM.
- [~] battery save.
- [x] four-screen/mirroring state.
- [x] mapper 0/1/2/3/4/7.
- [~] MMC3 A12 IRQ.
- [ ] T2/T3 mappers.
- [ ] expansion audio.
- [x] mapper bank/IRQ tests for T1 basics.
- [x] mapper matrix.

## H. State/determinism/trace

- [x] full-system snapshot v1 (frontend).
- [x] CPU/PPU/APU/mapper in snapshot.
- [x] snapshot version check.
- [~] SDB save/load: CLI only.
- [x] save/load hash equality.
- [ ] 100-run determinism.
- [ ] cross-optimization hash equality.
- [ ] trace levels 0/1/2/3.
- [ ] stream/replay trace.
- [ ] mismatch window export.
- [~] no wall clock in core; SDL frontend has pacing.

## I. NEMU DiffTest

- [x] CPU DiffTest (`regcpy`/`exec`).
- [x] `isa_difftest_checkregs`.
- [~] NES ROM load into REF: API exists; `.so` not exported.
- [ ] `nes_difftest_load_rom`.
- [ ] `nes_difftest_get_state/state_hash`.
- [ ] `nes_difftest_frame_hash`.
- [ ] `nes_difftest_get_video`.
- [ ] `nes_difftest_get_audio`.
- [x] `nes_ref_machine_run_frame`.
- [ ] DUT `nes_difftest_attach`.
- [~] instruction-level sync (CPU-only works).
- [ ] bus-cycle sync.
- [ ] scanline/frame sync.
- [ ] audio block sync.
- [ ] mapper state sync.
- [ ] mismatch report.
- [~] replay script (input replay only).
- [ ] 10^8-cycle self-comparison.
- [ ] injected-error detection.
- [ ] RTL adapter.
- [ ] independent REF/hardware cross-check.

## J. Verification

- [x] Klaus 6502.
- [ ] nestest.
- [ ] instr_test-v5.
- [ ] cpu_timing_test6.
- [ ] cpu_interrupts_v2.
- [~] PPU vblank/NMI/odd-frame self-tests.
- [~] sprite 0 self-test.
- [ ] sprite overflow ROM.
- [ ] ppu_open_bus.
- [ ] palette tests.
- [ ] apu_test/apu_reset.
- [ ] DMC tests.
- [~] mapper tests.
- [ ] 240p Test Suite.
- [ ] 20+ open/commercial ROM regression.
- [ ] PAL regression.
- [ ] hardware capture report.

## K. Performance/quality

- [~] REF >=60 FPS: runs; not benchmarked.
- [ ] DiffTest >=10^6 cycles/s.
- [~] ASan/UBSan.
- [ ] Valgrind.
- [ ] 10^9-tick run.
- [ ] static analysis.
- [x] documentation.

## L. v1.5 gate

v1.5 is defined as "full NES compatibility + verification". If the target is a
compatibility core, T2/T3/accessories/PAL can move to v2.0; L1/L2, extended
DiffTest and hardware timing remain mandatory.

- [ ] dot-level background/sprite pipeline.
- [ ] exact sprite 0 hit and boundary tests.
- [ ] DMC DMA/IRQ/output complete.
- [ ] NES PPU/APU/mapper DiffTest.
- [ ] hardware capture report.
- [ ] T2/T3 mappers/accessories/PAL as scoped.
- [ ] external test ROMs pass.
- [ ] state/trace/replay/mismatch closed.
- [ ] version/config/state frozen.

## M. Decisions

- [x] Prefer REF `.so`; NES extension still pending.
- [x] Frontend uses vaddr hooks; REF uses its own machine.
- [x] Digital cycle/dot/sample alignment; analog tolerance documented.
- [x] PAL is a T3 gate.
- [x] Commercial ROMs: hashes/scripts only.
- [x] T2 mapper scope driven by target library.
- [x] REF/DUT separation and hardware evidence priority.
