# NES/Famicom 游戏机芯片必须功能点与现状

状态定义：

- **DONE**：代码存在且有自动化测试/构建证据
- **PARTIAL**：功能存在，但时序、覆盖或测试不足
- **MISSING**：尚未实现或没有可验证实现

本表回答两件事：

1. 一颗可运行 NES/Famicom 游戏的芯片至少需要哪些功能点；
2. 当前 NEMU REF / 6502EMU 已覆盖到哪里，v1.5 还缺什么。

## 0. 必要性分层（重要）

“芯片必须”没有单一答案，取决于目标是**新芯片**还是**NES/Famicom 兼容芯片**：

| 层次 | 内容 | 是否芯片功能必需 |
| --- | --- | --- |
| L1 芯片核心 | CPU、总线、内存、视频、音频、输入、中断、DMA、时钟/复位 | 是（任何游戏机芯片） |
| L2 NES 兼容核心 | 2A03、2C02 逐 dot、sprite 0、DMC、T1 mapper、NTSC | 是（完整 NES 兼容） |
| L3 生态覆盖 | T2/T3 mapper、Zapper、Four Score、PAL、NES2、扩展音频 | 否（按目标游戏/地区决定） |
| L4 验证基础设施 | DiffTest、实机采集、trace、golden reference | 不是芯片功能，但 golden reference 必需 |

因此：**当前 v1.5 清单是“完整 NES/Famicom 兼容 + 验证”的范围，不是最小芯片功能集。**
如果目标是新芯片，L3/L4 可按产品定义裁剪；如果目标是完整 NES/Famicom 兼容芯片，
L1+L2 是硬门槛，L3 按兼容范围决定，L4 是验收手段而非芯片功能。

## A. CPU：2A03/2A07

| ID | 必须功能点 | 现状 | 证据 / 缺口 |
| --- | --- | --- | --- |
| A1 | 官方 6502 指令与寻址 | DONE | REF CPU Klaus `PC=3469` |
| A2 | 2A03 无 decimal 模式 | PARTIAL | 当前 m6502 仍保留 decimal 逻辑；需 2A03 profile |
| A3 | 指令周期与总线周期 | PARTIAL | CPU 有 cycles；缺少逐总线事务 trace |
| A4 | dummy read / RMW double write | PARTIAL | 代码有部分行为；缺专项测试 |
| A5 | NMI/IRQ 采样与延迟 | PARTIAL | 有 pending 字段；缺少 cpu_interrupts_v2 |
| A6 | BRK/RTI/PHP/PLP 状态位 | PARTIAL | Klaus 覆盖一部分；需专项测试 |
| A7 | OAM DMA halt/steal/对齐 | PARTIAL | 前端/REF 加 513 cycles；缺精确测试 |
| A8 | DMC DMA halt/steal | PARTIAL | DMC 核心有；CPU stall/对齐未实现 |
| A9 | 复位向量与初始状态 | DONE | REF 从 `$FFFC/$FFFD` 启动 |
| A10 | 目标稳定非法指令 | PARTIAL | 部分 NOP/未文档化项；未冻结 profile |

## B. 总线、内存与输入

| ID | 必须功能点 | 现状 | 证据 / 缺口 |
| --- | --- | --- | --- |
| B1 | 2KB WRAM + 4 次镜像 | DONE | REF 与前端测试 |
| B2 | PPU 寄存器 8 字节镜像 | DONE | REF/前端总线 |
| B3 | APU/IO 地址 | PARTIAL | 基础通道有；DMC/IO 不完整 |
| B4 | open bus | PARTIAL | 有 open_bus 字段；未逐场景测试 |
| B5 | OAM DMA | PARTIAL | 有实现；缺总线冲突/周期测试 |
| B6 | 标准手柄 1P/2P | DONE | 键盘映射、录制/回放测试 |
| B7 | Zapper | MISSING | 未实现 |
| B8 | Four Score / 4P | MISSING | 未实现 |
| B9 | Famicom 扩展/麦克风 | MISSING | 未实现 |
| B10 | 输入录制/回放 | DONE | `NESINP01` + hash 回归 |

## C. PPU：2C02/2C07

| ID | 必须功能点 | 现状 | 证据 / 缺口 |
| --- | --- | --- | --- |
| C1 | 341 dot × 262/312 scanline | PARTIAL | 有 dot 循环与逐 dot 背景像素；精灵仍按 scanline 合成 |
| C2 | vblank/NMI 精确位置 | DONE | `tests/nes_ppu` |
| C3 | `$2002` 读清除/w 锁存 | DONE | `tests/nes_ppu` |
| C4 | v/t/x/w 与 fine/coarse scroll | PARTIAL | 有状态和基本滚动；缺逐 dot 测试 |
| C5 | 背景 fetch/shift 管线 | PARTIAL | 已逐 dot 计算/输出背景像素；缺 fetch/shift registers |
| C6 | 精灵 evaluation/8x16/flip/priority | PARTIAL | 有 scanline 精灵渲染；缺逐 dot evaluation |
| C7 | sprite 0 hit 精确 dot | PARTIAL | 新增逐 dot 检查与单测；依赖 scanline 背景 |
| C8 | sprite overflow | PARTIAL | 有标志；行为未精确验证 |
| C9 | OAM/`$2004`/OAM DMA | PARTIAL | 有实现；缺完整周期测试 |
| C10 | `$2007` read buffer/increment | PARTIAL | 有实现；缺 ppu_open_bus 测试 |
| C11 | palette/mirror/open bus | PARTIAL | 有 palette；缺完整 open bus |
| C12 | nametable mirroring/four-screen | DONE | mapper 状态与测试 |
| C13 | odd-frame skip | DONE | `tests/nes_ppu` |
| C14 | rendering 开关对 v 的影响 | PARTIAL | 有基本行为；缺边界测试 |
| C15 | frame/VRAM/OAM hash | DONE | REF/前端 frame parity |
| C16 | NMI/IRQ 接入 CPU | DONE | REF frame loop |
| C17 | ppu_vbl_nmi / sprite_hit / overflow ROM | MISSING | 未接入外部测试 ROM |

## D. APU：2A03/2A07

| ID | 必须功能点 | 现状 | 证据 / 缺口 |
| --- | --- | --- | --- |
| D1 | Pulse 1/2 | PARTIAL | 有实现；缺 app_test 专项 |
| D2 | Triangle | PARTIAL | 有实现；缺线性计数器边界测试 |
| D3 | Noise | PARTIAL | 有 LFSR；缺模式/周期测试 |
| D4 | DMC | PARTIAL | 有 address/length/rate/loop/IRQ/output 与单测；缺 CPU DMA stall/精确总线时序 |
| D5 | Frame sequencer 4/5-step | PARTIAL | 有序列；缺精确周期对照 |
| D6 | Frame IRQ / `$4015` 清除 | PARTIAL | 有实现；缺测试 |
| D7 | 非线性 mixer | PARTIAL | 当前为近似混合 |
| D8 | 44.1kHz 输出 | DONE | WAV/hash |
| D9 | 采样 hash/容差 | DONE | REF/前端 audio hash parity |
| D10 | APU 状态导出 | PARTIAL | REF 有 APU hash；未导出逐通道状态 |
| D11 | apu_test/apu_reset/DMC tests | PARTIAL | 有自建 DMC 单测；未接 apu_test ROM |

## E. 卡带、mapper 与扩展

| ID | 必须功能点 | 现状 | 证据 / 缺口 |
| --- | --- | --- | --- |
| E1 | iNES 解析 | DONE | REF/前端 |
| E2 | NES 2.0 | MISSING | 未实现 |
| E3 | mapper 0 NROM | DONE | 测试 |
| E4 | mapper 1 MMC1 | DONE | bank 测试 |
| E5 | mapper 2 UxROM | DONE | bank 测试 |
| E6 | mapper 3 CNROM | DONE | CHR bank 测试 |
| E7 | mapper 4 MMC3 | PARTIAL | PRG/CHR/IRQ 基础；缺 A12 精确 IRQ |
| E8 | mapper 7 AxROM | DONE | bank 测试 |
| E9 | PRG/CHR/CHR RAM/PRG RAM | DONE | REF 测试/状态 |
| E10 | battery save | MISSING | 无持久化 PRG RAM |
| E11 | T2 mapper：MMC5/VRC/FME-7/Namco 163 | MISSING | 未实现 |
| E12 | 扩展音频 | MISSING | 未实现 |
| E13 | mapper IRQ 总线/时序 | PARTIAL | MMC3 scanline API；未接 PPU dot |
| E14 | mapper 支持矩阵 | DONE | 本文档/design |

## F. 平台、附件与 golden-reference

| ID | 必须功能点 | 现状 | 证据 / 缺口 |
| --- | --- | --- | --- |
| F1 | NTSC 2A03/2C02 | DONE | 前端/REF |
| F2 | PAL 2A07/2C07 | PARTIAL | REF 有 PAL 时钟；前端/PPU 未完整 |
| F3 | CPU/PPU revision profile | MISSING | 未实现 |
| F4 | REF CPU+PPU+APU+mapper frame loop | DONE | 60 帧 hash parity |
| F5 | REF/frontend frame/audio hash 对齐 | DONE | `67667b1cbc4c1cf3` / `e78422afbcb92ac3` |
| F6 | REF 全状态 hash | DONE | `nes_ref_machine_hash()` |
| F7 | 前端状态快照/恢复 | DONE | `--save-state/--load-state` |
| F8 | NES 扩展 DiffTest | MISSING | 当前只有 CPU-only `.so` |
| F9 | PPU/APU/mapper 逐状态 DiffTest | MISSING | 需扩展协议 |
| F10 | 实机采集对齐 | MISSING | 无硬件采集数据 |
| F11 | 逐 dot trace / bus trace | MISSING | 未实现 |
| F12 | 外部测试 ROM 全集 | MISSING | 仅 Klaus/自带 ROM/单元测试 |

## v1.5 必须达成的功能点

按用户列出的 v1.5 目标，必须完成：

1. **PPU dot-level 背景/精灵管线**
   - 背景 shift registers、fetch、fine X
   - 精灵逐 dot evaluation/priority
   - 逐 dot 状态可观测
2. **sprite 0 hit 精确 dot**
   - OAM/CHR/背景 opacity 对齐
   - 测试覆盖 x=0..255 边界
3. **APU DMC**
   - sample address/length/rate/loop
   - DMA stall
   - IRQ
   - output level
4. **NES 扩展 DiffTest**
   - REF `.so` 导出 ROM/PPU/APU/mapper state
   - DUT 侧 attach/同步
   - frame/audio/state hash 对比
5. **实机采集对齐**
   - CPU 总线、PPU dot 事件、APU 通道、DMC DMA
   - 容差表
6. **T2/T3 mapper、附件、PAL**
   - MMC5、VRC、FME-7、Namco 163
   - Zapper、Four Score、麦克风
   - PAL 2A07/2C07 profile

## 优先级建议

| 优先级 | 功能 |
| --- | --- |
| P0 | DMC、dot-level PPU、sprite0、扩展 DiffTest |
| P1 | 实机采集、PAL |
| P2 | T2 mapper、附件、扩展音频 |

P0 未完成前，不应把版本标记为“完整 golden reference”。


## 建议的版本切分

如果 v1.5 的目标是“能运行绝大多数 NES 游戏的兼容核心”，建议：

- v1.5 必须：L1 + L2 + NES DiffTest + 实机关键时序对齐
- v2.0 再做：L3 的 T2/T3 mapper、附件、PAL、NES2、扩展音频

如果 v1.5 的目标是“完整生态兼容”，则保留当前清单，但必须承认工作量是
多个版本级别，而不是单个增量。
