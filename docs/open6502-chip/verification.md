# Open6502 验证计划

## 1. 分层

| 层级 | 对象 | 方法 |
| --- | --- | --- |
| L0 | package/地址译码/计数器 | SV 单测 |
| L1 | CPU/PPU/APU 单元 | 定向测试 + 参考向量 |
| L2 | CPU+bus | Klaus、nestest、DiffTest |
| L3 | PPU+CPU | PPU 测试 ROM、framebuffer hash |
| L4 | APU | apu_test、DMC、采样 hash |
| L5 | mapper | bank/IRQ 测试 ROM |
| L6 | 系统 | 开源游戏、实机采集 |
| L7 | FPGA | 板级视频/音频/手柄 |

## 2. 必跑测试

- Klaus 6502 functional test
- nestest
- instr_test-v5
- cpu_timing_test6
- cpu_interrupts_v2
- ppu_vbl_nmi
- sprite_hit_tests
- sprite_overflow_tests
- ppu_open_bus
- apu_test / apu_reset / DMC
- 240p Test Suite
- Open6502 开源游戏 ROM：Falling、Galaxy Patrol、Hanoi、Table Tennis、File Fixers、Stallar

## 3. DiffTest 门禁

- CPU-only：Klaus + hello 零失配
- frame-only：NROM 游戏 60/300 帧 hash 一致
- scanline：PPU 测试 ROM 零失配
- audio：APU 采样 hash/容差通过
- mapper：bank/IRQ 状态一致

## 4. FPGA 验证

- 256×240 RGB 输出稳定
- 60Hz 帧率
- 音频无爆音
- 手柄 1P/2P
- ROM 加载/复位
- 长时间运行 ≥1 小时

## 5. 实机对齐

需要：

- 逻辑分析仪/总线采集
- PPU dot/scanline 事件
- APU 通道输出
- DMC DMA

当前环境没有实机，先交付采集格式和比对脚本。

## 6. 发布门禁

v0 FPGA 原型：

- CPU Klaus
- PPU vblank/NMI/sprite0
- APU 四通道
- mapper 0/1/2/3/4/7
- 6 个开源 ROM 可玩
- DiffTest CPU+frame 通过

ASIC 就绪另需：DFT、时序收敛、功耗、封装、实机对齐。
