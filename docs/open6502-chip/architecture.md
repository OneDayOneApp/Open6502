# Open6502 SoC Architecture

## 1. Block diagram

```text
21.477272 MHz -> clock/reset (/12 CPU, /4 PPU, /24 APU)
                     |
+-------------+  +---v---------+  +----------+  +-----------+
| 6502 CPU    |<>| system bus  |<>| 2C02 PPU |->| video     |
| 2A03 profile|  | decoder/    |  | dot      |  | timing    |
+-------------+  | arbitration |  +----+-----+  | RGB444    |
                 +--+---+---+--+       |        +-----------+
                    |   |   |          |
           +--------v+  |   +----------v+
           | 2KB WRAM |  |   | VRAM/OAM/palette SRAM
           +----------+  |   +----------+
                         |
                    +----v-----+   +----------+
                    | 2A03 APU |   | cart/map |
                    | 4ch + DMC|   | PRG/CHR  |
                    +----+-----+   +----+-----+
                         |              |
                    audio mixer     cartridge bus
```

## 2. Clocking and reset

| Clock | Frequency | Use |
| --- | ---: | --- |
| `clk_21m` | 21.477272 MHz | master |
| `cpu_en` | 1.789773 MHz | CPU |
| `ppu_en` | 5.369318 MHz | PPU dot |
| `aud_en` | 44.1 kHz | audio sample |
| `vid_pclk` | 5.369318 MHz | pixel |

All sequential logic runs on `clk_21m` with enable phases, avoiding multi-clock
CDC. Reset is asynchronous assert, synchronous release.

## 3. Address map

```text
$0000-$1FFF  2KB WRAM, four mirrors
$2000-$3FFF  PPU registers, 8-byte mirrors
$4000-$4017  APU, OAM DMA, controllers
$4018-$401F  test/expansion
$4020-$5FFF  expansion
$6000-$7FFF  PRG RAM
$8000-$FFFF  cartridge PRG
```

## 4. Modules

```text
rtl/
  open6502_pkg.sv
  open6502_top.sv
  cpu/open6502_cpu.sv
  bus/open6502_bus.sv
  ppu/open6502_ppu.sv
  apu/open6502_apu.sv
  cart/open6502_cart_if.sv
  input/open6502_controller.sv
  video/open6502_video_timing.sv
  audio/open6502_audio_mixer.sv
  dbg/open6502_difftest_bridge.sv
```

## 5. CPU

- official 6502 instructions plus target stable undocumented ones
- 2A03 profile without decimal mode
- cycle-level bus: fetch, operands, dummy reads, RMW double write, DMA stalls
- NMI edge, IRQ level, APU/mapper IRQ

## 6. PPU

- 341 dots x 262 lines NTSC
- background fetch/shift, sprite evaluation, sprite 0, overflow
- `vid_x/vid_y/vid_pixel/vid_de/vid_hsync/vid_vsync`
- internal VRAM/OAM/palette SRAM

## 7. APU

- pulse x2, triangle, noise, DMC
- frame sequencer, length/envelope/sweep, DMC DMA/IRQ
- 16-bit PCM or 1-bit sigma-delta

## 8. Cartridge/mappers

- RTL mappers 0/1/2/3/4/7
- PRG/CHR banks, mirroring, PRG RAM, IRQ
- optional physical connector; FPGA prototypes can load iNES into BRAM/SRAM

## 9. Top-level ports

```systemverilog
module open6502_top (
  input  logic        clk_21m,
  input  logic        rst_n,
  output logic        vid_pclk,
  output logic        vid_de,
  output logic        vid_hsync,
  output logic        vid_vsync,
  output logic [11:0] vid_rgb,
  output logic        aud_valid,
  output logic signed [15:0] aud_sample,
  input  logic [7:0]  joy1,
  input  logic [7:0]  joy2,
  output logic [15:0] cart_addr,
  output logic [7:0]  cart_wdata,
  input  logic [7:0]  cart_rdata,
  output logic        cart_we,
  input  logic        cart_irq,
  output logic        dbg_valid,
  output logic [63:0] dbg_payload
);
```

## 10. FPGA prototype

- Artix-7/Nexys A7 class board
- RGB444 to VGA/HDMI, 256x240 upscaled
- I2S or sigma-delta audio
- SPI Flash/SD card loading iNES into SRAM/BRAM
- UART trace; optional JTAG

## 11. ASIC constraints

- synthesizable SystemVerilog subset only
- no dynamic arrays/classes/DPI in synthesis
- single clock plus enables; no uncontrolled CDC
- SRAM macros; DFT/scan ports reserved
- digital video/audio interfaces; external DACs

## 12. Relationship to NEMU

NEMU provides the functional/state reference. The RTL emits DiffTest events; a
Verilator C++ model is co-simulated with NEMU. NEMU is never synthesized.

## 13. Phases

```text
P0 spec/interfaces/package
P1 CPU + bus + UART
P2 PPU + video timing
P3 APU + audio
P4 cart/mapper + controllers
P5 DiffTest/Verilator/FPGA
P6 PAL/T2/T3/accessories
```
