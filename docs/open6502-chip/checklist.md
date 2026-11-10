# Open6502 Chip CheckList

状态：`[ ]` 未开始，`[~]` 进行中，`[x]` 完成。

## P0 规格与接口

- [x] v0 范围：NTSC、mapper 0/1/2/3/4/7、标准手柄。
- [x] 时钟/复位策略。
- [x] 地址空间。
- [x] 顶层端口。
- [x] DiffTest 拓扑。
- [ ] 冻结寄存器/内存映射 v0。
- [ ] 冻结视频/音频电气接口。

## P1 CPU + 总线

- [ ] 6502 RTL 核。
- [ ] 2A03 profile（无 decimal）。
- [ ] 总线周期/dummy read/RMW。
- [ ] NMI/IRQ。
- [ ] OAM DMA halt/steal。
- [ ] 2KB WRAM + 镜像。
- [ ] UART trace。
- [ ] Klaus/nestest RTL 仿真。
- [ ] CPU-only NEMU DiffTest。

## P2 PPU

- [~] 341×262 dot 计数器：RTL 视频时序骨架 + smoke test。
- [ ] vblank/NMI。
- [ ] 背景 fetch/shift。
- [ ] sprite evaluation。
- [ ] sprite 0 hit。
- [ ] overflow。
- [ ] OAM DMA。
- [ ] palette/mirroring。
- [~] RGB444/VGA timing：时序计数器有；像素/编码未接。
- [ ] PPU 测试 ROM。
- [ ] frame hash DiffTest。

## P3 APU

- [ ] Pulse ×2。
- [ ] Triangle。
- [ ] Noise。
- [ ] DMC。
- [ ] frame sequencer。
- [ ] mixer。
- [ ] 44.1kHz 输出。
- [ ] apu_test/DMC。
- [ ] audio hash DiffTest。

## P4 卡带/mapper/输入

- [ ] 卡带接口。
- [ ] mapper 0/1/2/3/4/7。
- [ ] PRG RAM/battery。
- [ ] MMC3 IRQ。
- [ ] 手柄 1P/2P。
- [ ] iNES 加载。
- [ ] 6 个开源 ROM 回归。

## P5 DiffTest/FPGA

- [ ] `open6502_difftest_bridge` RTL。
- [ ] Verilator DPI 桥。
- [ ] 失配报告/replay。
- [ ] FPGA 综合/时序。
- [ ] VGA/HDMI 输出。
- [ ] I2S/sigma-delta 音频。
- [ ] 1 小时稳定性。

## P6 后续

- [ ] PAL。
- [ ] T2/T3 mapper。
- [ ] Zapper/Four Score。
- [ ] 实机采集对齐。
- [ ] ASIC DFT/时序/功耗。
