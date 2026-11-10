# NES/Famicom Golden Reference CheckList（基于 NEMU）

状态：`[ ]` 未开始，`[~]` 进行中，`[x]` 完成。每项都必须有测试/证据，不以“能跑”代替。

## A. 范围与 NEMU 构建策略（Phase 0）

- [ ] 确认目标平台：NTSC NES/Famicom、PAL NES，或两者。
- [ ] 确认芯片 revision profile：2A03G/2A03H/2A07、2C02G/2C02H/2C07。
- [ ] 确认 T0/T1/T2/T3 验收边界。
- [ ] 确认 mapper 支持矩阵及 unlicensed 范围。
- [ ] 确认附件范围：标准手柄、Zapper、Four Score、麦克风。
- [ ] 确认音频/视频验收口径（RGB、palette、采样率、容差）。
- [ ] 确认 REF 与 DUT 分离策略：REF 用 NEMU `m6502-nesref*`，DUT 为 RTL/另一构建。
- [ ] 确认 NEMU 配置：`CONFIG_ISA_m6502`、`CONFIG_NES`、`CONFIG_NES_REF`、
      `CONFIG_TARGET_SHARE`、`CONFIG_DIFFTEST`。
- [ ] 冻结 NEMU SDB 命令、状态格式、trace 格式 v1。
- [ ] 建立测试 ROM/自由 ROM/商业 ROM 元数据清单。
- [ ] 建立版权策略：不提交无授权 ROM。

## B. NEMU REF 工程骨架（Phase 1）

- [ ] 新增 `configs/m6502-nesref_defconfig`。
- [x] 新增 `configs/m6502-nesref-so_defconfig`（`CONFIG_TARGET_SHARE`）。
- [ ] 新增 `CONFIG_NES`、`CONFIG_NES_REF`、`CONFIG_NES_DIFFTEST` Kconfig。
- [ ] `make m6502-nesref_defconfig && make` 生成 REF 可执行文件。
- [x] `make m6502-nesref-so_defconfig && make` 生成 REF `.so`。
- [ ] REF 入口不复用 raw image 的 `init_monitor()` 加载路径。
- [x] `src/nes/ref/`：ROM 加载、复位、运行、状态导出。
- [x] master tick 调度器（NTSC/PAL 整数分频）。
- [ ] CPU/PPU/APU 相位与 `cpu_exec` 接入。
- [ ] `src/nes/bus`：地址映射、open bus、trace。
- [ ] `src/nes/state`：snapshot/hash。
- [ ] `src/nes/trace`：bus/PPU/APU trace。
- [ ] SDB 扩展：`nes info/frame/audio/state/hash`。
- [ ] REF batch/headless runner。
- [ ] ASan/UBSan/Valgrind 构建配置。
- [ ] CI 骨架和构建矩阵。

## C. CPU 2A03（Phase 2，复用 NEMU）

- [~] `src/isa/m6502` 现有 m6502 核；2A03 profile 尚未独立。
- [x] 官方 151 条指令（基础 REF CPU，Klaus 通过）。
- [ ] 目标稳定非法指令。
- [ ] 取指/操作数/dummy read 总线事件。
- [ ] RMW double write。
- [ ] 分支/跨页/零页回绕。
- [ ] JMP 间接页缺陷（按 profile）。
- [ ] NMI 边沿/IRQ 电平采样。
- [ ] BRK/RTI/PHP/PLP 状态位。
- [ ] OAM DMA halt/steal/对齐。
- [ ] DMC DMA steal/halt。
- [ ] 复位状态与 NES 复位向量。
- [x] `isa_difftest_checkregs` 实现 m6502 对比。
- [ ] CPU 状态/cycles/pending 纳入 snapshot。
- [x] Klaus（REF CPU，PC=3469）；nestest/instr_test-v5 待接。
- [ ] cpu_timing_test6。
- [ ] cpu_interrupts_v2。
- [ ] 总线 trace 与 REF/硬件对齐。

## D. PPU 2C02（Phase 3，NEMU `src/nes/ppu`）

- [ ] 逐 dot 调度（341 × 262/312）。
- [ ] vblank/NMI dot 精确位置。
- [ ] `$2002` 读清除时序。
- [ ] v/t/x/w 与 fine/coarse scroll。
- [ ] 背景 fetch/shift。
- [ ] sprite evaluation、8x8/8x16、flip/priority/left clip。
- [ ] sprite 0 hit。
- [ ] sprite overflow。
- [ ] OAM DMA/`$2004`。
- [ ] `$2007` read buffer/increment。
- [ ] palette RAM/mirror/open bus。
- [ ] nametable mirroring/four-screen。
- [ ] odd frame skip。
- [ ] rendering 开关对 v 的影响。
- [ ] framebuffer/VRAM/OAM hash。
- [ ] NMI/IRQ 接入 NEMU CPU 状态。
- [ ] ppu_vbl_nmi。
- [ ] sprite_hit_tests。
- [ ] sprite_overflow_tests。
- [ ] ppu_open_bus。
- [ ] palette/scroll/split 专项。
- [ ] 240p test suite 关键模式。

## E. APU 2A03（Phase 4，NEMU `src/nes/apu`）

- [ ] Pulse 1/2：timer/duty/length/envelope/sweep。
- [ ] Triangle：timer/linear/length/sequence。
- [ ] Noise：LFSR/mode/period/envelope/length。
- [ ] DMC：address/length/rate/loop/IRQ/DMA/output。
- [ ] Frame sequencer 4/5-step。
- [ ] Frame IRQ + `$4015` 清除。
- [ ] 非线性 mixer。
- [ ] 44.1kHz 输出/采样器。
- [ ] 采样 hash/容差。
- [ ] apu_test/apu_reset。
- [ ] DMC 专项测试。
- [ ] 通道状态导出。
- [ ] 长时间运行无爆音/漂移。

## F. 总线/输入（Phase 5）

- [ ] CPU RAM 2KB + 4 次镜像。
- [ ] PPU 寄存器 8 字节镜像。
- [ ] APU/IO 地址。
- [ ] open bus。
- [ ] OAM DMA 总线冲突。
- [ ] 标准手柄 strobe/shift。
- [ ] 2P 端口。
- [ ] Zapper（T2）。
- [ ] Four Score（T2）。
- [ ] Famicom 扩展/麦克风（按 profile）。
- [ ] 输入脚本/回放。
- [ ] 输入状态 save/load。

## G. 卡带/mapper（Phase 5）

- [ ] iNES 解析。
- [ ] NES 2.0 解析。
- [ ] 文件头校验/mapper 编号。
- [ ] PRG ROM/CHR ROM/CHR RAM。
- [ ] PRG RAM + battery。
- [ ] four-screen/镜像。
- [ ] mapper 0 NROM。
- [ ] mapper 1 MMC1。
- [ ] mapper 2 UxROM。
- [ ] mapper 3 CNROM。
- [ ] mapper 4 MMC3 + A12 IRQ。
- [ ] mapper 7 AxROM。
- [ ] mapper 9/10（T2）。
- [ ] mapper 69 FME-7（T2）。
- [ ] mapper 5 MMC5（T2）。
- [ ] VRC2/4/6/7（T2）。
- [ ] Namco 163（T2）。
- [ ] 扩展音频（T2）。
- [ ] 每个 mapper 的 bank boundary/IRQ/save-state 测试。
- [ ] mapper 支持矩阵文档。

## H. 状态/确定性/Trace（Phase 1/7）

- [ ] 全系统 snapshot v1。
- [ ] mapper/APU/PPU/CPU 全部纳入 snapshot。
- [ ] snapshot 版本校验。
- [ ] NEMU SDB `save/load` 接入。
- [ ] save→load→继续 hash 一致。
- [ ] 100 次重复运行 hash 一致。
- [ ] 跨优化等级 hash 一致。
- [ ] trace level 0/1/2/3。
- [ ] trace 可流式写、可回放。
- [ ] 失配窗口导出。
- [ ] 不依赖墙钟/随机源。

## I. NEMU DiffTest（Phase 7）

- [x] NEMU CPU 常规 DiffTest：`difftest_regcpy` / `difftest_exec`。
- [ ] `isa_difftest_checkregs`。
- [ ] NES 专用 ROM 加载到 REF（不能只依赖 `RESET_VECTOR` memcpy）。
- [ ] `nes_difftest_load_rom()`。
- [ ] `nes_difftest_get_state()/state_hash()`。
- [ ] `nes_difftest_frame_hash()`。
- [ ] `nes_difftest_get_video()`。
- [ ] `nes_difftest_get_audio()`。
- [ ] `nes_difftest_step_frame()`。
- [ ] DUT 侧 `nes_difftest_attach()`。
- [ ] 指令级同步。
- [ ] 总线周期同步。
- [ ] scanline/frame 同步。
- [ ] audio block 同步。
- [ ] mapper 状态同步。
- [ ] 失配报告格式。
- [ ] replay 脚本。
- [ ] 自比较零误报（10^8 cycles）。
- [ ] 人为注入错误可检出。
- [ ] RTL adapter（如目标为 RTL）。
- [ ] 与至少一个独立 REF/实机交叉验证。

## J. 验证与兼容性（Phase 8）

- [ ] Klaus 6502 functional test。
- [ ] nestest。
- [ ] instr_test-v5。
- [ ] cpu_timing_test6。
- [ ] cpu_interrupts_v2。
- [ ] ppu_vbl_nmi。
- [ ] sprite_hit_tests。
- [ ] sprite_overflow_tests。
- [ ] ppu_open_bus。
- [ ] ppu palette tests。
- [ ] apu_test/apu_reset。
- [ ] DMC tests。
- [ ] mapper tests。
- [ ] 240p test suite。
- [ ] ≥20 个自由 ROM/测试 ROM 回归。
- [ ] 商业 ROM 本地兼容性矩阵（不入库）。
- [ ] PAL 独立回归。
- [ ] 实机采集对齐报告。

## K. 性能/质量（Phase 8）

- [ ] REF 正常模式 ≥60 FPS。
- [ ] DiffTest 模式 ≥10^6 cycles/s。
- [ ] ASan/UBSan 干净。
- [ ] Valgrind 无泄漏。
- [ ] 10^9 tick 长跑。
- [ ] 静态分析/格式检查。
- [ ] 文档/发布说明。

## L. 发布门禁（T0+T1 REF）

只有以下全部勾选，才可发布：

- [ ] REF 由 NEMU `m6502-nesref*` 构建，DUT 为独立构建。
- [ ] NEMU CPU DiffTest + NES 扩展协议均通过。
- [ ] CPU 全部指令/周期/中断/DMA 测试通过。
- [ ] PPU vblank/NMI/sprite/scroll/palette/open bus 测试通过。
- [ ] APU 四通道/frame/DMC/IRQ 测试通过。
- [ ] mapper 0/1/2/3/4/7 全部通过。
- [ ] snapshot 确定性通过。
- [ ] DiffTest 自比较 10^8 cycles 零误报。
- [ ] ≥20 个 ROM 帧/音频回归通过。
- [ ] 实机关键时序在容差内。
- [ ] 已知差异文档完成。
- [ ] 版本 tag、NEMU 配置、状态格式版本冻结。

## M. 待决策项

- [ ] REF 是 NEMU 可执行文件优先，还是 `.so` 优先？
  - 建议：同时支持；CI 以 `.so` 为 DiffTest 主入口。
- [ ] NES 总线继续走 `vaddr` 钩子，还是改成 paddr/device 映射？
  - 建议：先保留 `vaddr` 钩子，CPU 周期接口稳定后再评估。
- [ ] 是否要求 RTL 级 bit-exact？
  - 建议：数字域 cycle/dot/sample 对齐，模拟域定义容差。
- [ ] PAL 是否进入 T0？
  - 建议：T0 先 NTSC，PAL 作为 T3 独立门槛。
- [ ] 商业 ROM 测试数据如何合法管理？
  - 建议：只存 hash/脚本，用户本地提供。
- [ ] T2 mapper 范围如何冻结？
  - 建议：按目标游戏库反推，不追求无限 mapper 列表。
- [ ] REF 与 DUT 如何避免同源错误？
  - 建议：REF 以硬件采集/独立实现校验；DUT 不得直接链接 REF 代码。
