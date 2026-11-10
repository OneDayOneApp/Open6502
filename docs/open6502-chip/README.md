# Open6502 Chip Design

Open6502 是一颗 **FPGA-first、NES/Famicom 兼容** 的 8-bit 游戏 SoC：

- DUT：SystemVerilog RTL
- REF：当前 NEMU NES golden reference
- 验证：Verilator/FPGA + NEMU DiffTest
- 首发目标：NTSC、mapper 0/1/2/3/4/7、标准手柄、RGB 视频、数字音频

## RTL 骨架

当前 `rtl/` 已包含可 lint/仿真的 SoC 顶层、341×262 视频时序、手柄移位寄存器和
DiffTest 桥骨架；CPU/PPU/APU/mapper 尚未实现。

```sh
cd rtl
make lint
make sim
```

## 文档

- [架构设计](architecture.md)
- [DiffTest 协议](difftest.md)
- [验证计划](verification.md)
- [CheckList](checklist.md)

## 设计原则

1. **RTL 是可综合的唯一实现**，NEMU 不参与综合。
2. **NEMU 是 REF，不是 DUT**；REF 与 RTL 不共享实现。
3. **先 CPU + 总线，再 PPU/APU，再 mapper/附件**。
4. **FPGA 原型优先于 ASIC**；ASIC 约束从第一天就遵守。
5. **所有对外接口都有可观测 trace**，便于 DiffTest 定位。

## v0 范围

| 模块 | v0 范围 |
| --- | --- |
| CPU | 6502/2A03，官方指令 + 目标稳定非法指令 |
| PPU | 2C02 兼容，逐 dot，256×240，RGB444 |
| APU | 2A03 四通道 + DMC，44.1kHz 数字输出 |
| 总线 | WRAM/PPU/APU/OAM DMA/手柄/PRG RAM |
| 卡带 | iNES mapper 0/1/2/3/4/7 |
| 输入 | 2 个标准手柄 |
| 平台 | NTSC 21.477272MHz |
| 调试 | UART/JTAG + DiffTest trace |

PAL、T2/T3 mapper、Zapper/Four Score、模拟视频/音频属于 v1 以后。
