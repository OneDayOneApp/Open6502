# Playing Games on Open6502

Open6502 has two game-running paths: the NEMU-based `6502EMU` frontend and the
SystemVerilog RTL SoC.

## What is playable today

| Path | Status | How |
|---|---|---|
| `6502EMU` NEMU frontend | Playable now | SDL window, keyboard, audio |
| RTL SoC simulation | Title + gameplay frame | Verilator frame capture |
| RTL on FPGA | Not yet | Needs synthesis/board bring-up |

## NEMU: play now

Build once:

```sh
cd NEMU
make m6502_defconfig
make -j8
```

Play a bundled open-source ROM:

```sh
cd /Users/tongxiaojun/github/Open6502
./roms/play.sh falling
./roms/play.sh galaxypatrol
./roms/play.sh hanoi
./roms/play.sh table_tennis
./roms/play.sh file-fixers
./roms/play.sh stallar
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
| Arrow keys | 2P direction |
| `,` `.` | 2P A/B |
| `/` `'` | 2P Select/Start |
| Esc | quit |

## NEMU golden screenshots

Title:

```sh
./NEMU/build/6502EMU --headless --no-audio --frames=300 \
  --screenshot=/tmp/nemu-falling.ppm \
  roms/falling.nes
```

Gameplay after START:

```sh
./NEMU/build/6502EMU --headless --no-audio --hold=start --frames=20 \
  --screenshot=/tmp/nemu-start.ppm \
  roms/falling.nes
```

## RTL: simulate the same game

Extract iNES data:

```sh
python3 sim/game/extract_ines.py roms/falling.nes /tmp/falling falling
```

Build the Verilator SoC model:

```sh
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
```

Run the RTL game:

```sh
/tmp/obj_game/Vopen6502_top \
  /tmp/falling/prg.bin /tmp/falling/chr.bin /tmp/falling-rtl.ppm
```

Convert to PNG:

```sh
python3 - <<'PY'
from PIL import Image
Image.open('/tmp/falling-rtl.ppm').save('/tmp/falling-rtl.png')
PY
```

The harness currently presses START at frame 5, so the captured frame is the
in-game FALLING level.

## Verification

| Check | Result |
|---|---|
| RTL CPU Klaus | PASS at `PC=0x3469` |
| RTL unit/integration tests | 28 PASS |
| NEMU/RTL final-state DiffTest | PASS |
| NEMU/RTL PC trace | PASS |
| RTL title frame vs NEMU | byte-identical SHA-256 |
| RTL START entry | `$00=1` stable, palette retained |
| RTL gameplay frame | rendered |

Title SHA-256:

```text
41e8ebc137e3b8ede2f0adf915956fb9454346e9454e358c6dad2ee5a58f5a33
```

## Known limits

- RTL is a functional SoC prototype, not a cycle-exact NES.
- PPU is per-pixel rather than a full fetch/shift pipeline.
- Sprite evaluation and MMC3 A12 are approximate.
- No FPGA bitstream or board validation yet.
- RTL frame capture is Verilator-only; there is no SDL frontend for RTL yet.
