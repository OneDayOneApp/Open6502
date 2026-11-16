# Changelog

## Unreleased v1.5 (in progress)
- Added `site/chip-lab.html`, a 16-slide Chinese classroom deck for primary-school students: what a chip is, sand-to-wafer, SoC modules, RTL, simulation, verification, tapeout, the Open6502 case, a quiz and a teacher guide. It is a dependency-free web presentation with speaker notes, overview/fullscreen navigation, deep links, a quiz bank and a teacher guide; linked from the Chinese navigation and sitemap.
- Made the nesearch static pages usable on phones. The catalog turns its sidebar into a drawer, its detail panel into a bottom sheet (a right-hand sheet in landscape), keeps filters and tables scrollable, and sizes controls for touch. The browser player gained a multi-touch thumb pad (8-way D-pad with diagonals, A/B, Select/Start), a tap-to-start overlay, a landscape layout that parks the controls in the side margins, auto-pause on tab switch, wake lock, a safe fullscreen fallback for iOS and canvas sizing that fits any viewport. Keyboard and gamepad input are unchanged.
- Added a share row to every site page footer: native share sheet, X, Facebook, Reddit, Telegram, Hacker News and copy link. Links are real anchors built from each page's canonical URL (so they work without JavaScript), keep the current `#section`, and fall back to the canonical URL on localhost.
- Added `skills/xiangshan-chip-flow`, a reusable XiangShan-style chip-design playbook distilled from Open6502: three loops, S0–S6 gates, one-image/multi-backend contract, lockstep DiffTest, CI/checkpoint discipline, FPGA/PDK checklists, 22 anti-patterns and copy-ready templates. Linked from both READMEs, `docs/README.md` and the site docs/learning pages.
- Refreshed the XiangShan alignment scorecard on the site (34 → 46.8/100) after am6502 M1–M3, and added a score-history curve plus a per-update log (`docs/scorecard-history.md`). The chart is generated as static inline SVG by `site/tools/render-scorecard.js`, so it needs no page script and cannot be hidden by a stale cache. Corrected the stale "root licence unpublished" claim in `CONTRIBUTING.md` and the README gap summary.
- Added `labs/am6502` design documents: an AM-style (nexus-am-inspired) image contract, `ARCH=mos6502-{nemu,nes}` matrix and TRM+IOE-only scope for 6502/NES, with M0–M3 milestones. Design stage only; no code yet.
- Implemented `labs/am6502` M1: `mos6502-nemu` platform (crt0, TRM, ld65 flat layout), `hello` and a Klaus wrapper, `tools/mkimage.py` + `tools/run.py`, and `make test`. Images are byte-reproducible (same SHA-256 across builds) and Klaus passes at `PC=$3469` with the build's own reset vector; `_io_*` and `am_mainargs` are stubs.
- Implemented `labs/am6502` M2: `mos6502-nes` platform (NROM iNES cartridge, `$4018` putc / `$4019` halt), `tools/ines.py`, and NEMU + GEM6502 host-console decode. The same `hello.nes` now halts with code 0 and renders a full white frame on NEMU, GEM6502 and the new `tb/open6502_am6502_tb.sv` RTL bench.
- Implemented `labs/am6502` M3: per-instruction lockstep DiffTest (`tools/difftest.py` + `tools/difftrace.py`, `tb/open6502_am6502_flat_tb.sv`) compares NEMU and RTL on the same image and reports the first diverging instruction with context; added the `.github/workflows/am6502.yml` gate (flat, cartridge on three backends, DiffTest).
- Added the root MIT license, `THIRD_PARTY_NOTICES.md` and `NEMU/NOTICE`; `NEMU/` stays under Mulan PSL v2.
- Added `nesearch/scripts/package-release.sh` (bundles MIT/NEMU/SDL2/license texts and re-signs the Mach-O binaries) and tracked the missing `NEMU/LICENSE` (Mulan PSL v2).
- Added social sharing (X, Facebook, Reddit, Telegram, Hacker News, copy link, native share) to the catalog details and the browser player, plus `?game=<id>` deep links and social preview meta tags.
- Share links now point at per-game landing pages (`game/<id>.html`) with that game's own title-screen `og:image`; the player's share row moved into the title bar.

- Added Space as a 1P A/jump alias (Z remains an alias).
- Added dot-level sprite 0 hit check and unit test.
- Added APU DMC core: address/length/rate/loop/IRQ/output, memory callback and tests.
- Moved background pixel generation into the PPU dot loop; sprites are composited at the end of the scanline.
- Added `feature-matrix.md` with must-have NES/Famicom chip functions and status.
- Updated checklist with evidence-based DONE/PARTIAL/MISSING states.

Remaining v1.5 blockers:

- full dot-level background/sprite pipeline
- APU DMC CPU DMA stall / exact bus timing
- NES extended DiffTest
- hardware capture alignment
- T2/T3 mapper/accessories/PAL


## v1.0.0 - functional NES/Famicom golden reference baseline

### Added

- REF PPU bus callbacks so the frontend and REF share one 2C02 core.
- REF C API:
  - `nes_ref_machine_run_frame()`
  - frame hash / audio hash / full state hash
  - controller input
  - PPU/APU register routing and OAM DMA
- REF CPU + PPU + APU + mapper frame loop.
- REF/frontend parity test for the bundled NROM game after 60 frames:
  - frame hash `67667b1cbc4c1cf3`
  - audio hash `e78422afbcb92ac3`
- REF state hash determinism and input-sensitivity tests.

### Scope of v1.0

The v1.0 reference is a **functional** NEMU-based golden-reference baseline:
- iNES/NROM through mapper 0/1/2/3/4/7
- 2A03 CPU, 2C02 PPU scanline renderer, 2A03 APU
- deterministic frame/audio/state hashes
- frontend parity for the bundled game
- CPU/PPU/APU/mapper/state/input replay tests

It is not a cycle-exact or dot-exact full-chip model. Known gaps are listed in
`docs/nes-golden-reference/checklist.md`.

### Verification

- default regression
- `RUN_KLAUS=1`
- CPU-only NEMU DiffTest


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
