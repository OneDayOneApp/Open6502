# Contributing to Open6502

Open6502 is an open-source 6502/NES-compatible SoC project. The long-term goal
is a 55nm tapeout. The project is currently a **functional prototype**, not a
tapeout-ready chip.

We welcome contributors who can help with RTL, verification, FPGA bring-up,
ASIC flow, documentation and testing.

## Before you start

- Read [README.md](README.md) and [docs/55nm-tapeout-roadmap.md](docs/55nm-tapeout-roadmap.md).
- Check open issues and the workstreams below.
- Keep changes small and test-driven.
- Use English commit messages and English repository documentation.
- Do not add copyrighted ROMs, BIOS files, or proprietary PDK data.

## Licensing and provenance

The repository does not yet declare a root project license. **Do not contribute
code until the maintainers publish the root license and contribution terms.**
Bundled ROM licenses are listed in [roms/LICENSES](roms/LICENSES).

For ASIC contributions, do not upload foundry PDK files, SRAM compiler outputs
or proprietary standard-cell libraries. Contribute scripts and interfaces that
can be run against a PDK the user obtains separately.

## Development rules

1. Add or update a test before changing RTL behaviour.
2. Run the regression before opening a pull request:
   ```sh
   cd rtl
   make lint
   make sim
   ```
3. Run the DiffTest smoke tests:
   ```sh
   ./sim/difftest/run.sh
   ./sim/difftest/run_alu.sh
   ```
4. Keep existing golden hashes unchanged unless the change is intentionally
   reviewed and documented.
5. Do not delete tests to make a change pass.
6. Prefer forward fixes over reverts.
7. For RTL changes, state the verification evidence in the pull request.

## Workstreams

### 1. CPU and bus

- Cycle-accurate CPU timing
- NMI/IRQ and DMA interaction
- Bus transaction uniqueness (`bus_strobe`)
- Klaus/nestest coverage
- Illegal/undocumented opcode policy

### 2. PPU

- Background fetch/shift pipeline
- Exact sprite evaluation and sprite-0 hit
- PPU open bus and palette reads
- Nametable mirroring
- Per-scanline scroll behavior
- Frame comparison against NEMU

### 3. APU and DMC

- Frame sequencer accuracy
- Sweep and envelope edge cases
- DMC bus arbitration and CPU stall alignment
- Audio sample-rate and mixer verification

### 4. Cartridge and mappers

- Mapper 0/1/2/3/4/7 timing
- MMC3 A12 filtering and IRQ
- CHR banking and CHR RAM
- iNES / NES 2.0 loading

### 5. FPGA

- Yosys synthesis cleanup
- BRAM inference
- Timing constraints and Fmax
- PLL, VGA/HDMI, audio DAC, controller
- Board bring-up

### 6. ASIC / 55nm

- 55nm standard-cell and SRAM integration
- Synthesis and area/timing reports
- DFT/scan and test coverage
- Clock/reset/power intent
- Package and pad ring
- Tapeout checklist and review

### 7. Software and CI

- NEMU/RTL lockstep DiffTest
- Frame-index golden comparison
- CI for lint, tests and synthesis
- Reproduction scripts
- Documentation and onboarding

## Good first issues

These are intentionally scoped:

- Add a unit test for PPU `$2007` palette read behaviour.
- Add a frame-index assertion to `sim/game`.
- Add a controller protocol test for A/B/Select/Start ordering.
- Document the mapper 0 CHR loading path.
- Add a `make synth-<module>` target for one block.
- Add a reproducible NEMU screenshot script.
- Improve a README section without changing claims.

## Pull request checklist

```text
[ ] Change is small and scoped
[ ] Test added or updated
[ ] rtl/make lint passes
[ ] rtl/make sim passes
[ ] DiffTest smoke tests pass
[ ] Golden hashes unchanged or intentionally updated
[ ] English commit message
[ ] No copyrighted ROM or proprietary PDK data
[ ] Verification evidence included
```

## Communication

Use GitHub issues for bugs and proposals. For larger work, open an issue first
so the design can be reviewed before implementation.

## Current non-goals

- Claiming tapeout readiness before timing/DFT/DRC/LVS are complete.
- Shipping commercial ROMs.
- Hiding failures behind test removals or golden updates.
