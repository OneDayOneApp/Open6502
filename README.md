# Open6502

**A 6502/NES-compatible SoC that runs games in software today, and a
SystemVerilog chip design that renders the same game in RTL.**

Open6502 is two things in one repository:

- **`6502EMU`** — a NEMU-based 6502/NES emulator with a playable SDL frontend.
- **Open6502 RTL** — a SystemVerilog SoC with CPU, PPU, APU, bus, WRAM,
  cartridge mapper, DMA, controller input and VGA output.

The RTL is verified against the emulator: the CPU passes Klaus, and the RTL
title frame is byte-identical to the NEMU title frame.

---

## See it run

### RTL title screen

![Open6502 RTL title screen](docs/images/rtl-title.png)

### RTL gameplay after START

![Open6502 RTL gameplay](docs/images/rtl-gameplay.png)

### NEMU reference gameplay

![NEMU gameplay](docs/images/nemu-gameplay.png)

---

## What works today

| Capability | Status |
|---|---|
| Play open-source NES games in `6502EMU` | **Yes** |
| RTL CPU passes Klaus | **Yes** |
| RTL renders `falling.nes` title | **Yes**, NEMU-identical |
| RTL enters gameplay after START | **Yes** |
| RTL chiptune audio path | Partial |
| RTL FPGA bitstream | Not yet |
| Full NES game compatibility | Partial |

Bundled playable ROMs:

```text
falling          galaxypatrol     hanoi
table_tennis     file-fixers      stallar
```

All are open-source ROMs. See [roms/README.md](roms/README.md).

---

## Quick start: play now with NEMU

```sh
cd /Users/tongxiaojun/github/Open6502

# build once
cd NEMU
make m6502_defconfig
make -j8
cd ..

# play
./roms/play.sh falling
```

Or directly:

```sh
./NEMU/build/6502EMU roms/falling.nes
```

Keyboard:

| Key | Function |
|---|---|
| W/A/S/D | 1P direction |
| Space / Z | 1P A / jump |
| X | 1P B |
| C / V | 1P Select / Start |
| Esc | quit |

More ROMs, headless screenshots and replay options:
**[docs/game-testing.md](docs/game-testing.md)**.

---

## Quick start: run the RTL game

The RTL game runs in Verilator and writes a PPM frame.

```sh
cd /Users/tongxiaojun/github/Open6502

# 1. convert iNES to PRG/CHR binaries
python3 sim/game/extract_ines.py roms/falling.nes /tmp/falling falling

# 2. build the RTL model
rm -rf /tmp/obj_game
verilator --cc --exe -Wno-fatal --top-module open6502_top \
  -Mdir /tmp/obj_game \
  rtl/open6502_top.sv rtl/open6502_pkg.sv \
  rtl/cpu/open6502_cpu.sv rtl/bus/open6502_bus.sv rtl/bus/open6502_wram.sv \
  rtl/ppu/open6502_ppu.sv rtl/apu/open6502_apu.sv rtl/apu/open6502_dmc.sv \
  rtl/cart/open6502_mapper.sv rtl/clocks/open6502_clocks.sv \
  rtl/video/open6502_video_timing.sv rtl/input/open6502_controller.sv \
  rtl/dma/open6502_oam_dma.sv rtl/dbg/open6502_difftest_bridge.sv \
  sim/game/main.cpp

make -C /tmp/obj_game -f Vopen6502_top.mk \
  CXX=clang++ LINK=clang++ \
  CXXFLAGS='-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk' \
  LDFLAGS='-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -L/opt/homebrew/Cellar/llvm/21.1.8/lib/c++' \
  LDLIBS='-lc++' -j2

# 3. run and capture a frame
/tmp/obj_game/Vopen6502_top \
  /tmp/falling/prg.bin /tmp/falling/chr.bin /tmp/falling-rtl.ppm
```

Convert to PNG:

```sh
python3 -c 'from PIL import Image; Image.open("/tmp/falling-rtl.ppm").save("/tmp/falling-rtl.png")'
```

Full RTL game-test guide:
**[docs/game-testing.md](docs/game-testing.md)**.

---

## Verification

```sh
cd rtl
make lint
make sim
```

Current results:

```text
28 RTL tests PASS
RTL Klaus PASS at PC=0x3469
```

NEMU/RTL co-simulation:

```sh
./sim/difftest/run.sh
./sim/difftest/run_alu.sh
```

Golden title frame:

```text
RTL  SHA-256 41e8ebc137e3b8ede2f0adf915956fb9454346e9454e358c6dad2ee5a58f5a33
NEMU SHA-256 41e8ebc137e3b8ede2f0adf915956fb9454346e9454e358c6dad2ee5a58f5a33
```

---

## Synthesis and manufacturing cost

Yosys per-module area:

| Block | Generic cells |
|---|---:|
| CPU | 4,138 |
| APU + DMC | 2,683 |
| Mapper | 745 |
| OAM DMA | 154 |
| WRAM | 37,003 |
| Bus/clock/video/controller | 779 |
| PPU | 975,919 |
| **Total** | **1,021,421** |

55nm planning estimate:

| Scenario | Die area |
|---|---:|
| Current RTL, memories as flip-flops | **4–7 mm²** |
| SRAM + PPU refactor | **1.2–2.0 mm²** |
| Recommended planning target | **~1.5 mm²** |

At volume, a 1.5–2.0 mm² die at 55nm is roughly:

```text
unit cost: USD 0.3-0.7 (die + package + test, planning estimate)
mask/NRE: USD 150k-400k+
```

Full area breakdown:
**[docs/open6502-chip/synthesis-area.md](docs/open6502-chip/synthesis-area.md)**

Cost model and assumptions:
**[docs/synthesis-and-cost.md](docs/synthesis-and-cost.md)**

---

## Repository map

```text
NEMU/                 NEMU-based 6502/NES emulator and BASIC
roms/                 Playable open-source NES ROMs
rtl/                  SystemVerilog SoC
  cpu/                Open6502 CPU
  ppu/                NES PPU
  apu/                APU and DMC
  bus/                Bus and WRAM
  cart/               Mapper 0/1/2/3/4/7
  dma/                OAM DMA
  input/              Controller
  video/              VGA timing
  fpga/               FPGA wrapper
tb/                   SystemVerilog testbenches
sim/difftest/         NEMU/RTL DiffTest
sim/game/             RTL game bring-up
docs/                 Design, verification, area and cost documents
```

---

## Documentation

### Start here

- [Game testing: NEMU and RTL](docs/game-testing.md)
- [Synthesis and cost](docs/synthesis-and-cost.md)

### Chip design

- [Chip overview](docs/open6502-chip/README.md)
- [Architecture](docs/open6502-chip/architecture.md)
- [Verification](docs/open6502-chip/verification.md)
- [DiffTest](docs/open6502-chip/difftest.md)
- [Checklist](docs/open6502-chip/checklist.md)
- [Synthesis area](docs/open6502-chip/synthesis-area.md)
- [START recovery plan](docs/open6502-chip/startup-recovery-plan.md)

### Emulator and golden reference

- [6502EMU guide](NEMU/6502EMU.md)
- [NES golden reference design](docs/nes-golden-reference/design.md)
- [NES golden reference test plan](docs/nes-golden-reference/test-plan.md)

---

## Current limitations

- RTL PPU is functional but not cycle-exact.
- Sprite evaluation and MMC3 A12 timing are approximate.
- DMC bus arbitration and CPU DMA alignment are incomplete.
- RTL frame capture is Verilator-based; there is no SDL frontend for RTL yet.
- No FPGA bitstream or board validation yet.
- Full flattened synthesis and timing closure are not complete.

---

## Third-party licenses

Bundled ROM licenses are in [roms/LICENSES](roms/LICENSES). The repository
does not yet declare a root project license; add one before redistribution.
