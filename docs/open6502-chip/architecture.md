# Open6502 芯片架构

## 1. 总体框图

```text
                       +-----------------------------+
21.477272 MHz --->     |        clock/reset          |
                       |  /12 CPU  /4 PPU  /24 APU   |
                       +------+------+------+--------+
                              |      |      |
+-------------+        +------v------+  +---v----+  +----------+
| 6502 CPU    |<------>| system bus  |  |  PPU   |->| video    |
| 2A03 profile|<------>| decoder/    |  | 2C02   |  | timing   |
+-------------+        | arbitration |  | dot    |  | RGB444   |
                       +--+---+---+--+  +---+----+  +----------+
                          |   |   |         |
                          |   |   +-------->| VRAM/OAM/palette
                          |   |             |
          +---------------v+  v----------+  v
          | 2KB WRAM       |  | APU      |  | cartridge/mapper
          | PRG RAM        |  | 4ch+DMC  |  | PRG/CHR/IRQ
          +----------------+  +----+-----+  +------+-----------+
                                  |                |
                             +----v----+      +----v----+
                             | audio   |      | cart    |
                             | mixer   |      | connector|
                             +----+----+      +---------+
                                  |
                             I2S/sigma-delta
```

## 2. 时钟与复位

| 时钟 | 频率 | 用途 |
| --- | ---: | --- |
| `clk_21m` | 21.477272 MHz | 主时钟 |
| `cpu_en` | 1.789773 MHz | PPU/CPU 相位使能 |
| `ppu_en` | 5.369318 MHz | PPU dot |
| `aud_en` | 44.1 kHz | 音频采样 |
| `vid_pclk` | 5.369318 MHz | 像素输出 |

- 所有时序逻辑使用 `clk_21m`，通过 enable 分频，避免多时钟域 CDC。
- `rst_n` 异步置位、同步释放。
- CPU/PPU/APU 共享主时钟，DiffTest 可按主时钟对齐。

## 3. 地址空间

```text
$0000-$1FFF  2KB WRAM，4 次镜像
$2000-$3FFF  PPU 寄存器，8 字节镜像
$4000-$4017  APU、OAM DMA、手柄
$4018-$401F  test/expansion
$4020-$5FFF  expansion
$6000-$7FFF  PRG RAM（mapper 控制）
$8000-$FFFF  cartridge PRG
```

- 内部 WRAM 用双端口 SRAM（CPU + DMA）。
- OAM DMA 需要 CPU halt/steal 和总线仲裁。
- 卡带接口支持 PRG/CHR/IRQ/PRG RAM。

## 4. 模块划分

```text
rtl/
  open6502_pkg.sv              参数/类型/地址常量
  open6502_top.sv              SoC 顶层
  cpu/open6502_cpu.sv          6502 核
  bus/open6502_bus.sv          地址译码/仲裁
  ppu/open6502_ppu.sv          2C02 逐 dot
  apu/open6502_apu.sv          2A03 + DMC
  cart/open6502_cart_if.sv     卡带接口/mapper 选择
  input/open6502_controller.sv 手柄串行
  video/open6502_video_timing.sv
  audio/open6502_audio_mixer.sv
  dbg/open6502_difftest_bridge.sv
```

### 4.1 CPU

- 官方 6502 指令 + 目标稳定非法指令。
- 2A03 profile：无 decimal。
- 周期级总线：取指、操作数、dummy read、RMW double write、DMA stall。
- 中断：NMI 边沿、IRQ 电平、APU/mapper IRQ。

### 4.2 PPU

- 341 dot × 262 scanline，NTSC。
- 背景 fetch/shift、sprite evaluation、sprite 0、overflow。
- 输出 `vid_x/vid_y/vid_pixel/vid_de/vid_hsync/vid_vsync`。
- 内部 VRAM/OAM/palette SRAM。

### 4.3 APU

- Pulse ×2、Triangle、Noise、DMC。
- frame sequencer、length/envelope/sweep、DMC DMA/IRQ。
- 输出 16-bit PCM 或 1-bit sigma-delta。

### 4.4 卡带与 mapper

- mapper 0/1/2/3/4/7 由 RTL 实现。
- PRG/CHR bank、mirroring、PRG RAM、IRQ。
- 卡带连接器可选；FPGA 原型可用片上 BRAM/外部 SRAM 加载 iNES。

## 5. 顶层接口

```systemverilog
module open6502_top (
  input  logic        clk_21m,
  input  logic        rst_n,

  // video
  output logic        vid_pclk,
  output logic        vid_de,
  output logic        vid_hsync,
  output logic        vid_vsync,
  output logic [11:0] vid_rgb,

  // audio
  output logic        aud_valid,
  output logic signed [15:0] aud_sample,

  // controllers
  input  logic [7:0]  joy1,
  input  logic [7:0]  joy2,

  // cartridge
  output logic [15:0] cart_addr,
  output logic [7:0]  cart_wdata,
  input  logic [7:0]  cart_rdata,
  output logic        cart_we,
  input  logic        cart_irq,

  // debug/DiffTest
  output logic        dbg_valid,
  output logic [63:0] dbg_payload
);
```

## 6. FPGA 原型

- 目标板：Artix-7/Nexys A7 或同级。
- 视频：RGB444 经 VGA/HDMI 编码器输出 256×240 放大。
- 音频：I2S 或 sigma-delta。
- ROM：SPI Flash/SD 卡加载 iNES 到外部 SRAM/BRAM。
- 调试：UART 输出 trace；JTAG 可选。

## 7. ASIC 约束

- 只使用可综合 SystemVerilog 子集：`logic`、`always_ff`、`always_comb`、package。
- 无动态数组、class、DPI 在综合路径。
- 单时钟 + enable 策略，避免 CDC。
- SRAM 使用工艺宏；FPGA 用 BRAM。
- 所有寄存器可复位/可扫描；预留 DFT 端口。
- 视频/音频输出为数字接口，模拟 DAC 外置。

## 8. 与 NEMU 的关系

- NEMU REF 提供 CPU/PPU/APU/mapper 的功能与状态参考。
- RTL 通过 `open6502_difftest_bridge` 输出事务和状态。
- Verilator 将 RTL 编译为 C++，与 NEMU REF 做 lockstep。
- NEMU 不参与综合，也不直接生成 RTL。

## 9. 交付阶段

```text
P0 规格/接口/包
P1 CPU + bus + UART
P2 PPU + video timing
P3 APU + audio
P4 cart/mapper + controller
P5 DiffTest/Verilator/FPGA
P6 PAL/T2/T3/附件
```
