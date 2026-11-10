# NES/Famicom Golden Reference 测试方案

## 1. 测试目标

验证 NEMU NES REF 构建不只是“能运行游戏”，而是满足以下可度量目标：

1. **功能正确**：全部 CPU/PPU/APU/mapper 行为符合规范或已记录 profile。
2. **时序正确**：CPU 总线周期、PPU dot、APU 采样、mapper IRQ 在约定容差内一致。
3. **确定性**：相同 ROM + 相同输入 + 相同初始状态 → 相同输出 hash。
4. **可恢复**：save/load 后执行轨迹与不中断执行一致。
5. **可差分**：能以指令/scanline/frame/audio block 粒度与 DUT 对比。
6. **可回归**：失败可定位到 cycle、事件和状态字段。
7. **性能可用**：测试模式能跑完整回归，正常模式达到实时或更快。

## 2. 测试分层

| 层级 | 对象 | 依赖 | 输出 |
| --- | --- | --- | --- |
| L0 单元测试 | 每个函数/模块 | 无 ROM | 通过/失败 |
| L1 组件测试 | CPU/PPU/APU/mapper | 小型合成 ROM/向量 | 状态/周期/采样 |
| L2 指令级集成 | CPU+bus+DMA | CPU test ROM | PC/寄存器/总线 trace |
| L3 帧级集成 | PPU+CPU+mapper | PPU/游戏 ROM | framebuffer/状态 hash |
| L4 系统级兼容 | 完整 ROM | 商业/自由 ROM 集 | 启动/画面/音频/可玩性 |
| L5 差分测试 | DUT vs REF | 参考模型/RTL | 首个失配点 |
| L6 硬件对齐 | REF vs 实机采集 | 逻辑分析仪/采集卡 | 时序容差报告 |

任何层级失败都必须可以向下追溯到 L0/L1。

## 2.1 NEMU REF/DUT 构建

REF 必须从 NEMU 的专用配置构建，不能使用当前 `6502EMU` 演示配置直接充当：

```sh
cd NEMU
make m6502-nesref_defconfig
make -j8
# 生成 NEMU/build/nes-ref（命名以实际 Makefile 为准）

make m6502-nesref-so_defconfig
make -j8
# 生成 TARGET_SHARE 的 REF .so
```

测试时必须明确记录：

- REF 构建：`m6502-nesref*`
- DUT 构建：RTL / 另一 NEMU 配置 / 外部模拟器
- REF 与 DUT 不得是同一二进制
- ROM hash、mapper、输入脚本、初始状态

CPU-only 阶段可以先用 NEMU 默认 DiffTest；PPU/APU/mapper 阶段必须启用
NES 扩展 DiffTest 协议（见第 10 节）。

## 3. L0 单元测试

### 3.1 时钟/调度

- [ ] NTSC master tick → CPU/PPU/APU 分频比完全正确。
- [ ] PAL 分频比正确。
- [ ] `run_master_ticks` 与逐 tick 调用结果一致。
- [ ] 事件队列在同 tick 的排序稳定。
- [ ] 长时间运行（≥10^9 tick）无整数溢出/漂移。

### 3.2 状态/hash

- [ ] 同一状态哈希确定。
- [ ] 任一状态字段变化会导致对应 hash 变化。
- [ ] snapshot serialize→deserialize 后 hash 完全一致。
- [ ] 版本不匹配拒绝加载。
- [ ] trace 开启/关闭不改变核心状态。

### 3.3 工具函数

- [ ] 大小端、位域、回绕、镜像函数边界测试。
- [ ] 饱和/截断/符号扩展测试。
- [ ] PPU/APU 采样比率计算无累积误差。

## 4. L1/L2 CPU 测试

### 4.1 必跑 ROM

| 测试 | 目的 | 通过标准 |
| --- | --- | --- |
| Klaus Dormann `6502_functional_test` | 官方指令/寻址/标志 | 到达 success loop |
| `nestest.nes` | 指令级 PC/寄存器/周期 | 与 reference log 全匹配 |
| `instr_test-v5` | 指令行为 | 全部子测试 pass |
| `cpu_timing_test6` | 指令周期 | 周期 trace 匹配 |
| `cpu_interrupts_v2` | NMI/IRQ/BRK/RTI 时序 | 全部子测试 pass |
| `6502_decimal_test` | decimal 行为 | 仅用于 6502/65C02 profile，不用于 2A03 |
| `dummy_reads` / bus conflict tests | dummy read、RMW 写序 | 总线 trace 匹配 |
| `cpu_dma` / OAM DMA tests | halt/steal/对齐 | 周期和总线事件匹配 |

### 4.2 CPU 验收指标

- [ ] 官方 151 条指令全部通过。
- [ ] 目标 profile 的稳定非法指令全部通过。
- [ ] 每条指令的总线周期序列与硬件/参考一致。
- [ ] NMI/IRQ 的采样点和延迟一致。
- [ ] OAM DMA 的 halt/steal 周期、奇偶对齐一致。
- [ ] DMC DMA 与 CPU 的竞争行为一致（如果启用 DMC）。
- [ ] 复位、BRK、RTI、PHP/PLP 的状态位行为一致。
- [ ] `PC`、`A/X/Y/SP/P`、`cycles` 在每个同步点一致。

### 4.3 CPU trace 对比格式

```text
master_tick cpu_cycle scanline dot pc opcode a x y sp p rw addr data source
```

对比顺序：

1. PC / 寄存器 / cycles
2. 总线读写序列
3. dummy read / RMW double write
4. 中断/异常入口

## 5. L3 PPU 测试

### 5.1 必跑 ROM/测试

| 测试 | 目的 | 通过标准 |
| --- | --- | --- |
| `ppu_vbl_nmi` | vblank/NMI 时序 | 全部子测试 pass |
| `sprite_hit_tests` | sprite 0 hit | dot/x 位置符合 |
| `sprite_overflow_tests` | sprite overflow | 行为符合规范/测试 |
| `ppu_open_bus` | open bus | 数值符合 |
| `ppu_palette` / palette tests | palette mirror/读写 | 全部 pass |
| `240p_test_suite` | 综合画面/时序 | 关键模式稳定 |
| `blargg_ppu_tests` | PPU 综合 | 全部 pass |
| 自定义 scroll/split ROM | 滚动、mid-frame 变更 | 帧 hash 匹配 |

### 5.2 PPU 验收指标

- [ ] 341 dot × 262/312 scanline 时序正确。
- [ ] vblank 起始 dot 精确。
- [ ] NMI 边沿和 `$2002` 读清除时序正确。
- [ ] odd frame skip 正确。
- [ ] 背景 fetch/shift、fine X/Y、coarse X/Y 正确。
- [ ] sprite evaluation、8x16、priority、flip、left clip 正确。
- [ ] sprite 0 hit 的 dot/x 位置正确。
- [ ] sprite overflow 行为正确。
- [ ] `$2007` read buffer、palette mirror、open bus 正确。
- [ ] nametable mirroring / four-screen 正确。
- [ ] framebuffer hash 与金样一致。
- [ ] PAL profile 的 312 行/时序通过独立金样。

### 5.3 画面回归

对每个测试 ROM 保存：

- 第 1/30/60/120 帧 framebuffer hash。
- OAM/VRAM/palette hash。
- 关键 scanline 的 v/t/x/w 状态。
- 关键模式截图（PPM/PNG）。

## 6. L3 APU 测试

### 6.1 必跑测试

| 测试 | 目的 | 通过标准 |
| --- | --- | --- |
| `apu_test` / `apu_reset` | 通道、frame IRQ、复位 | 全部 pass |
| `dmc` tests | DMC DMA/loop/IRQ | 全部 pass |
| 脉冲/三角/噪声专项 | 波形/长度/包络 | 采样/通道状态匹配 |
| frame sequencer tests | 4/5-step 时序 | CPU 周期精确 |
| sweep/envelope tests | 扫频/包络边界 | 状态和输出符合 |
| mixer tests | 非线性混音 | 与金样容差内 |

### 6.2 APU 验收指标

- [ ] 帧序列在正确 CPU 周期触发。
- [ ] 长度计数器 halt/非 halt 正确。
- [ ] envelope/sweep 边界正确。
- [ ] DMC DMA 的 CPU stall、地址/长度、IRQ 正确。
- [ ] `$4015` 读清 frame IRQ 正确。
- [ ] 通道静音/启用状态正确。
- [ ] 采样输出确定，重复运行 hash 一致。
- [ ] 与金样的采样误差在定义容差内（例如 ±1 LSB/固定窗口）。

## 7. Cartridge/Mapper 测试

### 7.1 每个 mapper 的测试模板

- [ ] 上电/复位状态。
- [ ] PRG bank 切换边界（首/末/回绕）。
- [ ] CHR bank 切换边界。
- [ ] mirroring 切换。
- [ ] PRG RAM enable/protect。
- [ ] IRQ latch/reload/acknowledge。
- [ ] 8KB/16KB/32KB bank 模式。
- [ ] 8x16 sprite + CHR bank 组合。
- [ ] save/load 后 bank/IRQ 状态保持。
- [ ] 运行至少一个 mapper 专属测试 ROM。

### 7.2 支持矩阵

| Mapper | 名称 | T1/T2 | 测试 ROM/游戏 | 状态 |
| --- | --- | --- | --- | --- |
| 0 | NROM | T1 | 自带 game、测试 ROM | 待完整回归 |
| 1 | MMC1 | T1 | 银行/IRQ 测试 | 待完整回归 |
| 2 | UxROM | T1 | 银行测试 | 待完整回归 |
| 3 | CNROM | T1 | CHR bank 测试 | 待完整回归 |
| 4 | MMC3 | T1 | IRQ/银行测试 | 待完整回归 |
| 7 | AxROM | T1 | 单屏测试 | 待完整回归 |
| 9/10 | MMC2/4 | T2 | latch 测试 | 未开始 |
| 69 | FME-7 | T2 | IRQ/audio | 未开始 |
| 5 | MMC5 | T2 | 银行/IRQ/audio | 未开始 |
| 21/25 | VRC2/4 | T2 | 银行/IRQ | 未开始 |
| 85 | VRC7 | T2 | FM audio | 未开始 |
| 19 | Namco 163 | T2 | audio | 未开始 |

## 8. 系统级兼容测试

### 8.1 ROM 集合

分三组：

1. **自由/开源 ROM**：可纳入仓库或 CI 缓存。
2. **测试 ROM**：CPU/PPU/APU/mapper 专项。
3. **商业 ROM**：只保留元数据/hash/测试脚本，由用户本地提供，避免版权问题。

每个 ROM 记录：

```yaml
name:
sha256:
mapper:
mirroring:
prg_size:
chr_size:
expected:
  boot: true
  frame_hashes: [...]
  audio_hash:
  playable_until: frame N
```

### 8.2 兼容性验收

- [ ] 启动到标题画面。
- [ ] 进入游戏并运行 ≥N 帧无 abort/非法指令。
- [ ] 关键画面帧 hash 匹配金样。
- [ ] 输入脚本可改变游戏状态。
- [ ] 音频非静音且无爆音/长时间卡死。
- [ ] 保存/加载后继续运行正常。

## 9. 确定性/回放测试

- [ ] 同一 ROM、同一输入、同一 seed，连续运行 100 次 hash 相同。
- [ ] 不同主机、不同编译器优化等级 hash 相同（整型核心）。
- [ ] 运行 10^7/10^8 master ticks 无溢出/漂移。
- [ ] 任意时刻 save→load→继续，最终 hash 与不中断一致。
- [ ] 输入回放脚本可逐帧复现。
- [ ] trace 开关不改变最终 hash。

## 10. DiffTest 测试

### 10.0 NEMU 接入方式

CPU 部分复用 NEMU：

- `difftest_regcpy` 同步 CPU 寄存器；
- `difftest_exec(1)` 执行一条指令；
- DUT 侧实现 `isa_difftest_checkregs`；
- `difftest_memcpy` 同步可写内存。

NES 扩展部分通过 REF `.so` 导出：

```c
nes_difftest_load_rom()
nes_difftest_get_state()
nes_difftest_state_hash()
nes_difftest_frame_hash()
nes_difftest_get_video()
nes_difftest_get_audio()
nes_difftest_step_frame()
```

注意：NEMU 原生 `init_difftest()` 只把 raw image 复制到 `RESET_VECTOR`，不能直接
用于 NES 卡带。必须新增 NES 专用 attach/load 路径。

### 10.1 同步粒度

- 指令级：CPU 寄存器、cycles、中断状态。
- 总线级：每个读/写事件的地址、数据、tick、source。
- scanline 级：PPU v/t/x/w、OAM、nametable、palette。
- frame 级：framebuffer/VRAM/OAM hash。
- audio block：采样 hash/容差。
- mapper 级：bank/IRQ/PRG RAM。

### 10.2 失配报告

必须包含：

```text
ROM:
frame:
master_tick:
cpu_cycle:
scanline/dot:
sync_point:
field:
ref:
dut:
last_20_bus_events:
last_20_ppu_events:
last_20_apu_events:
state_diff:
replay_command:
```

### 10.3 DiffTest 验收

- [ ] 相同实现自比较 10^8 cycles 零误报。
- [ ] 人为注入 1 个寄存器/总线错误能被捕获。
- [ ] 人为注入 1 个 PPU dot 偏移能被捕获。
- [ ] 人为注入 1 个 APU 采样错误能被捕获。
- [ ] 失配可在 5 分钟内定位到模块和首个事件。

## 11. 硬件对齐测试

Golden reference 最终必须与实机对齐，不能只与另一个模拟器对齐。

建议采集：

- CPU 总线：地址/数据/RW、NMI/IRQ、OAM DMA。
- PPU：vblank、sprite 0、`$2007` 访问、关键 dot 事件。
- APU：通道输出、frame IRQ、DMC DMA。
- 手柄：strobe/shift 时序。

验收：

- [ ] NTSC 实机关键事件与 REF 在约定 tick 容差内。
- [ ] PAL 实机独立采集。
- [ ] 至少 3 种 revision/主机的差异被记录为 profile。
- [ ] 模拟器间交叉验证只作为补充，不作为唯一金标准。

## 12. 性能测试

| 模式 | 目标 |
| --- | --- |
| 正常运行 | ≥ 60 FPS，最好 ≥ 300 FPS |
| 关闭 trace | 锁定内存/无持续分配 |
| 全 trace | 可慢，但不能改变行为 |
| DiffTest | 可 lockstep 运行 ≥ 10^6 cycles/s |
| 状态 hash | 每帧计算不影响正常帧率 |

- [ ] 无内存泄漏（ASan/Valgrind）。
- [ ] 无未定义行为（UBSan）。
- [ ] 无数据竞争（如使用多线程）。
- [ ] 长时间运行稳定。

## 13. CI 门禁

每个 PR 必须通过：

1. 编译 REF/DUT 两套 NEMU 配置（Debug/Release、ASan/UBSan）。
2. L0 单元测试。
3. CPU 指令/周期测试。
4. PPU/APU 小型测试。
5. 确定性 hash 测试。
6. 自带游戏/自由 ROM 的 smoke test。
7. 代码格式/静态分析。

 nightly 必须通过：

- 全部 CPU/PPU/APU/mapper 测试 ROM。
- 自由 ROM 兼容集。
- 长时间确定性/性能测试。
- DiffTest 自比较。

## 14. 发布验收标准（T0+T1）

Golden reference 可标记为 **T0+T1 reference** 必须同时满足：

- [ ] CPU 官方指令 + 目标非法指令 + 周期 + 中断 + DMA 全部通过。
- [ ] PPU vblank/NMI/sprite 0/overflow/scroll/palette/open bus 测试全部通过。
- [ ] APU 四通道 + frame sequencer + DMC + IRQ 测试全部通过。
- [ ] mapper 0/1/2/3/4/7 银行/IRQ/mirroring 测试全部通过。
- [ ] 任意状态 save/load 后 hash 一致。
- [ ] 10^8 cycles 确定性测试通过。
- [ ] 至少 20 个自由 ROM + 测试 ROM 的帧/音频 hash 回归通过。
- [ ] 与实机采集的关键时序在容差内。
- [ ] REF 由 NEMU `m6502-nesref*` 配置构建，DUT 为独立构建。
- [ ] NES 扩展 DiffTest 协议通过。
- [ ] 文档记录所有已知差异和 profile。

不满足上述条件时，只能称为“功能原型”，不能称为完整 golden reference。
