# NES/Famicom Golden Reference CheckList（基于 NEMU）

状态：`[ ]` 未开始，`[~]` 进行中/部分实现，`[x]` 完成并有证据。

完整功能点、状态说明和缺口见 [feature-matrix.md](feature-matrix.md)。

## A. 范围与 NEMU 构建策略（Phase 0）

- [~] 目标平台：NTSC 已运行；PAL 仅有 REF 时钟，前端/PPU 未完整。
- [ ] 芯片 revision profile：2A03G/H、2A07、2C02G/H、2C07。
- [x] T0/T1/T2/T3 验收边界已定义。
- [~] mapper 支持矩阵：T1 已列；T2/unlicensed 未冻结。
- [ ] 附件范围：Zapper、Four Score、麦克风。
- [x] 音频/视频验收口径（RGB、palette、采样率、容差）已记录。
- [x] REF/DUT 分离策略已定义：REF 用 NEMU 构建，DUT 独立。
- [~] NEMU 配置：已有 `m6502-nesref-so_defconfig`、`m6502-nes-dut_defconfig`；
      缺 `m6502-nesref_defconfig` 和 `CONFIG_NES_REF`。
- [~] SDB 命令、状态格式、trace v1：状态格式有；trace 未实现。
- [~] 测试 ROM/自由 ROM 元数据：Klaus/自带 ROM；缺完整清单。
- [x] 版权策略：不提交无授权 ROM。

## B. NEMU REF 工程骨架（Phase 1）

- [ ] `configs/m6502-nesref_defconfig`（REF 可执行文件）。
- [x] `configs/m6502-nesref-so_defconfig`（`CONFIG_TARGET_SHARE`）。
- [~] `CONFIG_NES_REF` / `CONFIG_NES_DIFFTEST` Kconfig。
- [ ] `make m6502-nesref_defconfig && make`。
- [x] `make m6502-nesref-so_defconfig && make` 生成 REF `.so`。
- [~] REF 入口：`src/nes/ref` API 已可加载/运行；未接 NEMU `init_monitor()`。
- [x] `src/nes/ref`：ROM、复位、CPU/PPU/APU 帧循环、状态 hash。
- [x] master tick 调度器（NTSC/PAL 整数分频）。
- [x] CPU/PPU/APU 相位与帧循环接入。
- [~] 地址映射/open bus 在 `src/nes/ref/machine.c`；无独立 bus/trace 模块。
- [x] 状态 snapshot/hash：前端 snapshot + REF state hash。
- [ ] `src/nes/trace`：bus/PPU/APU trace。
- [ ] SDB `nes info/frame/audio/state/hash`。
- [ ] REF batch/headless runner。
- [~] ASan/UBSan：REF 单元测试曾通过；未纳入固定 CI。
- [ ] CI 骨架和构建矩阵。

## C. CPU 2A03（Phase 2，复用 NEMU）

- [~] `src/isa/m6502`：有 m6502 核；2A03 profile 未独立（decimal 仍存在）。
- [x] 官方 151 条指令；Klaus `PC=3469`。
- [~] 目标稳定非法指令：部分 legacy NOP；未冻结 profile。
- [ ] 取指/操作数/dummy read 总线事件。
- [~] RMW double write：代码行为存在，缺专项测试。
- [~] 分支/跨页/零页回绕：Klaus 覆盖，缺独立 trace 测试。
- [~] JMP 间接页缺陷：代码按 6502/65C02 区分；缺测试。
- [~] NMI 边沿/IRQ 电平采样：字段/采样存在；缺 cpu_interrupts_v2。
- [~] BRK/RTI/PHP/PLP 状态位：Klaus 覆盖部分。
- [~] OAM DMA halt/steal/对齐：有 513 cycle 近似；缺精确测试。
- [ ] DMC DMA steal/halt。
- [x] 复位状态与 NES 复位向量。
- [x] `isa_difftest_checkregs` 实现 m6502 对比。
- [x] CPU 状态/cycles/pending 纳入前端 snapshot 和 REF hash。
- [x] Klaus（REF CPU，PC=3469）。
- [ ] nestest / instr_test-v5。
- [ ] cpu_timing_test6。
- [ ] cpu_interrupts_v2。
- [ ] 总线 trace 与 REF/硬件对齐。

## D. PPU 2C02（Phase 3，NEMU `src/nes/ppu`）

- [~] 逐 dot 调度：有 341×262 循环；背景/精灵不是完整 dot 管线。
- [x] vblank/NMI dot 位置：`tests/nes_ppu`。
- [x] `$2002` 读清除/w 锁存：`tests/nes_ppu`。
- [~] v/t/x/w 与 fine/coarse scroll：有状态；缺逐 dot 测试。
- [~] 背景 fetch/shift：当前 scanline 渲染；缺 shift registers。
- [~] sprite evaluation/8x16/flip/priority/left clip：scanline 实现。
- [~] sprite 0 hit：新增逐 dot 检查与单测；依赖 scanline 背景。
- [~] sprite overflow：有标志；行为未精确验证。
- [~] OAM DMA/`$2004`：有实现；缺周期测试。
- [~] `$2007` read buffer/increment：有实现；缺 open bus 测试。
- [~] palette RAM/mirror/open bus：palette 有；open bus 不完整。
- [x] nametable mirroring/four-screen：mapper 状态/测试。
- [x] odd frame skip：`tests/nes_ppu`。
- [~] rendering 开关对 v 的影响：基本行为有；缺边界测试。
- [x] framebuffer/VRAM/OAM hash：REF state/frame hash。
- [x] NMI/IRQ 接入 CPU：REF frame loop。
- [~] ppu_vbl_nmi / sprite_hit / overflow ROM：只有自建单测。
- [ ] 240p test suite 关键模式。

## E. APU 2A03（Phase 4，NEMU `src/nes/apu`）

- [~] Pulse 1/2：有实现；缺 apu_test。
- [~] Triangle：有实现；缺 linear counter 边界测试。
- [~] Noise：有 LFSR；缺模式/周期测试。
- [ ] DMC：address/length/rate/loop/IRQ/DMA/output。
- [~] Frame sequencer 4/5-step：有序列；缺精确周期对照。
- [~] Frame IRQ + `$4015` 清除：有实现；缺测试。
- [~] 非线性 mixer：近似混合。
- [x] 44.1kHz 输出/采样器：WAV。
- [x] 采样 hash/容差：REF/前端 audio hash parity。
- [ ] apu_test/apu_reset。
- [ ] DMC 专项测试。
- [~] 通道状态导出：只有 hash；无逐通道导出。
- [ ] 长时间运行无爆音/漂移。

## F. 总线/输入（Phase 5）

- [x] CPU RAM 2KB + 4 次镜像。
- [x] PPU 寄存器 8 字节镜像。
- [~] APU/IO 地址：基础寄存器有；DMC/IO 不完整。
- [~] open bus：有字段；未逐场景测试。
- [~] OAM DMA 总线冲突：有实现；缺精确测试。
- [x] 标准手柄 strobe/shift。
- [x] 2P 端口。
- [ ] Zapper（T2）。
- [ ] Four Score（T2）。
- [ ] Famicom 扩展/麦克风。
- [x] 输入脚本/回放。
- [x] 输入状态 save/load（前端）。

## G. 卡带/mapper（Phase 5）

- [x] iNES 解析。
- [ ] NES 2.0 解析。
- [~] 文件头校验/mapper 编号。
- [x] PRG ROM/CHR ROM/CHR RAM。
- [x] PRG RAM（含状态）。
- [~] battery save：有 battery 标志；无持久化。
- [x] four-screen/镜像状态。
- [x] mapper 0 NROM。
- [x] mapper 1 MMC1。
- [x] mapper 2 UxROM。
- [x] mapper 3 CNROM。
- [~] mapper 4 MMC3：bank/IRQ API；A12 dot 时序不精确。
- [x] mapper 7 AxROM。
- [ ] mapper 9/10（T2）。
- [ ] mapper 69 FME-7（T2）。
- [ ] mapper 5 MMC5（T2）。
- [ ] VRC2/4/6/7（T2）。
- [ ] Namco 163（T2）。
- [ ] 扩展音频（T2）。
- [x] mapper bank boundary/IRQ/save-state 测试（0/1/2/3/4/7 基础）。
- [x] mapper 支持矩阵文档。

## H. 状态/确定性/Trace（Phase 1/7）

- [x] 全系统 snapshot v1（前端 NES）。
- [x] mapper/APU/PPU/CPU 全部纳入 snapshot。
- [x] snapshot 版本校验。
- [~] NEMU SDB `save/load`：只有 CLI 选项；SDB 未接。
- [x] save→load→继续 hash 一致。
- [ ] 100 次重复运行 hash 一致。
- [ ] 跨优化等级 hash 一致。
- [ ] trace level 0/1/2/3。
- [ ] trace 可流式写、可回放。
- [ ] 失配窗口导出。
- [~] 不依赖墙钟/随机源：核心 hash 路径是；SDL 前端有帧节奏。

## I. NEMU DiffTest（Phase 7）

- [x] NEMU CPU 常规 DiffTest：`difftest_regcpy` / `difftest_exec`。
- [x] `isa_difftest_checkregs`。
- [~] NES 专用 ROM 加载到 REF：REF API 可加载；`.so` 未接。
- [ ] `nes_difftest_load_rom()` 导出。
- [ ] `nes_difftest_get_state()/state_hash()`：REF API 有 state hash；`.so` 未导出。
- [ ] `nes_difftest_frame_hash()`：REF API 有 frame hash；`.so` 未导出。
- [ ] `nes_difftest_get_video()`。
- [ ] `nes_difftest_get_audio()`。
- [x] `nes_ref_machine_run_frame()`。
- [ ] DUT 侧 `nes_difftest_attach()`。
- [~] 指令级同步：CPU-only DiffTest 已通。
- [ ] 总线周期同步。
- [ ] scanline/frame 同步。
- [ ] audio block 同步。
- [ ] mapper 状态同步。
- [ ] 失配报告格式。
- [x] replay 脚本（输入回放，不是失配 replay）。
- [ ] 自比较零误报（10^8 cycles）。
- [ ] 人为注入错误可检出。
- [ ] RTL adapter。
- [ ] 与独立 REF/实机交叉验证。

## J. 验证与兼容性（Phase 8）

- [x] Klaus 6502 functional test。
- [ ] nestest。
- [ ] instr_test-v5。
- [ ] cpu_timing_test6。
- [ ] cpu_interrupts_v2。
- [~] PPU vblank/NMI/odd-frame：自建单测；缺 ppu_vbl_nmi。
- [~] sprite 0 hit：自建 dot 单测；缺 sprite_hit_tests ROM。
- [ ] sprite_overflow_tests。
- [ ] ppu_open_bus。
- [ ] ppu palette tests。
- [ ] apu_test/apu_reset。
- [ ] DMC tests。
- [~] mapper tests：0/1/2/3/4/7 bank；缺完整 IRQ/ROM。
- [ ] 240p test suite。
- [ ] ≥20 个自由 ROM/测试 ROM 回归。
- [ ] 商业 ROM 本地兼容性矩阵。
- [ ] PAL 独立回归。
- [ ] 实机采集对齐报告。

## K. 性能/质量（Phase 8）

- [~] REF 正常模式 ≥60 FPS：帧循环可跑，未基准化。
- [ ] DiffTest 模式 ≥10^6 cycles/s。
- [~] ASan/UBSan 干净：REF 单测曾通过；未固定。
- [ ] Valgrind 无泄漏。
- [ ] 10^9 tick 长跑。
- [ ] 静态分析/格式检查。
- [x] 文档/发布说明。

## L. v1.5 发布门禁

只有以下全部勾选，才可发布 v1.5：

- [ ] PPU 逐 dot 背景/精灵管线。
- [ ] sprite 0 hit 精确 dot 并有边界测试。
- [ ] APU DMC（DMA/IRQ/output）完整。
- [ ] NES PPU/APU/mapper 扩展 DiffTest。
- [ ] 实机采集对齐报告。
- [ ] T2/T3 mapper、附件、PAL 至少按既定范围完成。
- [ ] CPU/PPU/APU/mapper 外部测试 ROM 集通过。
- [ ] 状态/trace/回放/失配报告闭环。
- [ ] 版本 tag、NEMU 配置、状态格式冻结。

## M. 待决策项

- [x] REF 优先 `.so`：当前已实现 CPU-only `.so`；NES 扩展待做。
- [x] 总线继续走 `vaddr`/REF 独立 machine：当前前端走 vaddr，REF 走 machine。
- [x] 精度目标：数字域 cycle/dot/sample 对齐，模拟域定义容差。
- [x] PAL 作为 T3 独立门槛。
- [x] 商业 ROM 只存 hash/脚本，用户本地提供。
- [x] T2 mapper 按目标游戏库反推，不追求无限列表。
- [x] REF/DUT 不同源，硬件采集优先。
