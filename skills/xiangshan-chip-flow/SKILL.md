---
name: xiangshan-chip-flow
description: 用香山式流程设计、联调或评审数字 SoC/ASIC：先冻结芯片契约与独立参考模型，再建立同一镜像的锁步 DiffTest、CI 门禁与检查点回放，最后才做 FPGA 与 PDK 后端。适用于新芯片立项、RTL 验证体系搭建、上板/流片前评审。
---

# 香山式芯片设计流程（Open6502 提炼版）

> **一句话**：改一行 RTL → 一条命令 → 分钟级看到 PASS 或首个差异。
>
> 本 skill 把 Open6502 已经跑通的香山式做法（参考模型、锁步差分、CI 门禁、检查点、PPA/流片门禁）提炼成可复用流程。换芯片时**替换领域内容，保留环与门禁**：把 6502/NES 换成你的 ISA、协议和外设即可。
>
> 约定：`[事实]` 是 Open6502 仓库里已存在并有证据的做法；`[迁移]` 是为新芯片做的推广，落地前要按你的芯片重新评审。

---

## 0. 何时使用 / 何时不用

**使用**：

- 新数字芯片/SoC 从 0 立项，需要决定先做什么、每步交什么证据。
- 已有 RTL，但验证靠手工跑 waveform、回归靠人看日志。
- 要补 DiffTest、CI、检查点、确定性回放、覆盖率或后端门禁。
- 评审“能不能上板 / 能不能流片 / 能不能宣称 cycle-exact”。

**不使用**：

- 纯模拟、射频、电源、封装等非数字流程。
- 只改驱动/软件，不涉及 RTL 或芯片契约。
- 需要具体商业 PDK、DFT 签核细节：本 skill 只给流程、接口和检查单，不给 PDK 数据。

---

## 1. 先填这张卡，填不出就别写 RTL

| 项 | 必须回答 | 不合格的例子 |
|---|---|---|
| 目标芯片 | 一句话：什么芯片，给谁用 | “做个 CPU” |
| ISA/协议 | 指令集、总线、外设协议及版本 | “兼容 6502”但不说 2A03 profile |
| Profile | 时钟、复位、地址图、电气可见行为 | 地址图还在变 |
| 范围 | v0 必须有什么；明确非目标 | 把 PAL、扩展外设混进 v0 |
| 外部接口 | 视频/音频/存储/调试/封装引脚 | 接口没冻结就开始写 RTL |
| 参考模型 | 用哪个独立实现做 golden | “RTL 自己就是参考” |
| 精度目标 | functional / approximate / calibrated | 口头说“cycle-exact” |
| 后端 | FPGA 型号、PDK、SRAM、DFT 要求 | 先做后端再补验证 |
| 团队/周期 | 谁维护 REF、CI、后端 | 一个人同时追 5 个环 |
| 非目标 | 明确不做什么 | 不写，导致范围蔓延 |

**门禁**：接口、地址图、时钟/复位、镜像契约没有冻结，不进入 RTL 编码。冻结内容写入 `docs/spec/` 和 ADR；后续变更必须走 ADR，而不是“实现成什么样就算什么样”。

---

## 2. 三个环：先跑短，再跑大

| 环 | 时间尺度 | 你要练/交付的 | 退出标准 |
|---|---|---|---|
| **内环** | 分钟 | 参考模型 + 锁步 DiffTest + 确定性回放 + 首个差异定位 | 任意 RTL 改动，一条命令 2 分钟内给出 PASS/FAIL；失败能定位到首个周期/指令 |
| **中环** | 每次提交 | CI 回归、golden 哈希、失败 trace、required check | PR 不绿不能合；同种子两次结果一致；golden 变更需评审 |
| **外环** | 每次迭代 | 综合、FPGA、开放 PDK 后端、PPA 报告 | 小模块能到面积/时序报告；上板稳定；流片门禁逐项有证据 |

> `[事实]` Open6502 把这三环写成学习路径，并用八维评分跟踪；核心不是环的尺寸，而是**环与环之间的门禁纪律**。
>
> `[迁移]` 大芯片把内环从“分钟”放宽到“十分钟”可以，但不能取消；外环可以从“流片”降级为“开放 PDK PPA 演练”，但不能用“能综合”代替“正确”。

### 2.1 香山八件套 → 本 skill 的落点

| 香山模式 | 本 skill 在哪里 | 最小交付 |
|---|---|---|
| 生成化描述（Chisel/FIRRTL） | §6.4 + §9 | 一个叶子模块生成化并与 SV 等价 |
| 参考模型（NEMU/ISS） | §3 S1 + `references/contracts.md` | 独立 REF、可校准、可快照 |
| 锁步 DiffTest | §4 + `references/verification.md` | retire 级零差异 + 首差异报告 |
| 敏捷验证/CI | §5 + `assets/templates/ci.yml` | required check + 双跑 + 失败产物 |
| 检查点/确定性回放 | §4 + `references/contracts.md` | restore == 连续运行 |
| 性能采样/改动评估 | §10 + `assets/templates/experiment-manifest.json` | 计数器 + 阻塞归因报告 |
| 后端闭环（FPGA→ASIC） | §6 + `references/backend.md` | bitstream 或 PDK PPA + 时序 |
| 开源协作/许可 | §7 + §12 | 许可证 + PR 证据 + 第三方声明 |

**顺序不能反**：先把参考模型和 DiffTest 跑通，再谈生成化；先把内环跑短，再谈外环。

---

## 3. 七道门（S0–S6）

按顺序过。**后一道门不许掩盖前一道门的失败**。

> `[迁移]` S0–S6 是本 skill 对 Open6502 的 P0–P6 芯片清单与 GEM6502 的 G0–G6 模型门禁的合并表述，不是仓库原有编号；用的时候可以改名，但不要减少“证据”这一维。

| 门 | 名称 | 必须产出 | 可执行证据 | 退出条件 |
|---|---|---|---|---|
| **S0** | 契约冻结 | chip brief、地址图、时钟/复位、top ports、镜像/ABI 契约、ADR-001..003 | `make spec-check` 或等价的 schema/地址冲突检查 | 接口无 TBD；REF/DUT 边界写清 |
| **S1** | 参考模型 + 镜像 | 独立 REF、确定性镜像、host 端口、每程序成功判据 | 同一镜像在 REF 跑通；构建两次字节相同 | REF 通过 ISA/功能套件；镜像可复现 |
| **S2** | 单元 + 总线 | L0/L1 定向测试、总线单点提交、地址译码、SoC 冒烟 | `make lint` + `make sim` | 单元/集成测试全绿；总线事务唯一 |
| **S3** | 锁步 DiffTest | 同一镜像的 retire 级比对、首个差异报告 | `make difftest` | 规定指令/周期数零差异；预算达标 |
| **S4** | CI + 确定性 + 检查点 | required checks、双跑、失败产物、drained checkpoint | CI 运行记录；`make checkpoint-test` | PR 红即阻断；restore == 连续运行 |
| **S5** | FPGA | 全综合、BRAM、约束/Fmax、板级 bring-up | bitstream + 板上运行记录 | 真实板卡稳定跑目标负载 ≥1 小时 |
| **S6** | ASIC/PDK | SRAM、DFT、STA、PnR、DRC/LVS、pad/package | 签核报告 + checklist 评审 | 全清；无“口头流片就绪” |

**门禁写法**：每道门都必须有 `命令 + 产物 + 已知限制`。没有命令的目标视为不存在；永远返回成功的占位目标视为作弊。

---

## 4. 内环操作手册（最值钱的一块）

### 4.1 一张镜像，所有后端

`[事实]` Open6502 的 am6502 用一层抽象机解决“同一程序怎么进 NEMU、周期模型和 RTL”：

```text
make ARCH=<isa>-<platform> APP=<app>        # 产出确定性的镜像
make run  ARCH=... APP=... DUT=nemu|gem|rtl # 同一镜像，多后端
make difftest ARCH=... APP=...              # REF vs RTL 逐条 retire
```

镜像契约必须写清：容器格式、装载地址、入口/reset 向量、host I/O 端口、成功判据、派生调试产物（`.map/.lst/.sym`）。

**四条不可妥协**：

1. 镜像由**构建**产出，harness 不得改写 reset 向量。
2. 同一镜像**原样**跑过所有后端，不允许每个后端各打一份补丁。
3. 构建两次**字节相同**（无时间戳、无 `$RANDOM`、符号排序稳定）。
4. 每个程序有**自己的成功判据**（`halt` / `pc:<addr>`），超时是失败，不是通过。

### 4.2 REF 与 DUT 必须独立

- REF 是 golden，DUT 是待验证实现；**REF 不是 DUT，也不被综合**。
- 两边不能共享同一份核心实现；共享部分的一致只能算“非独立证据”。
- REF 必须用外部测试、规范或硬件捕获校准；不能用“和另一个仿真器一样”当唯一证据。
- 每次运行记录：REF 版本/commit、配置、状态版本、镜像哈希、输入脚本、种子。

### 4.3 观测记录：每个外部接口都要可观测

最小事件结构（按需扩展）：

```systemverilog
typedef struct packed {
  logic        valid;
  logic [1:0]  kind;    // 0=cpu 1=bus 2=ppu/device 3=apu/other
  logic [63:0] tick;    // master tick，整数，不用浮点
  logic [15:0] addr;
  logic [7:0]  data;
  logic        we;
  logic [7:0]  source;  // 发起者/来源
} dbg_event;
```

高频设备事件用 DPI/共享内存；FPGA 只导出摘要/哈希。**不要为了省事只比最终寄存器**。

### 4.4 同步点（从便宜到贵，逐层加）

| 粒度 | RTL 侧 | REF 侧 | 比较字段 |
|---|---|---|---|
| 指令 retire | `cpu_retire` | `cpu_step` | PC/A/X/Y/SP/P/cycles |
| 总线事务 | `bus_strobe` | bus trace | addr/data/we/tick/source |
| 扫描线/设备 | `device_scanline_end` | device state | v/t/x/w/OAM/status |
| 帧 | `frame_end` | framebuffer | frame hash |
| 音频 | `aud_valid` | samples | sample hash/容差 |
| mapper/外设 | `cart_event` | mapper state | bank/IRQ/PRG RAM |

`[事实]` Open6502 的 DiffTest 协议把粒度分成 cpu-only / frame-only / scanline / audio / full，并规定 Verilator + REF ≥ 10^6 cycles/s。

### 4.5 比较规则

- 首个差异**立即停**，dump 前后窗口；不要跑完再猜。
- 掩码必须逐字段有硬件依据（例如 6502 的 B/U 位），不能为了让测试变绿而掩码。
- 拒绝空 trace、格式错误、异常退出、截断、长度不等。
- 不做任意周期偏移；需要对齐时在 adapter 里显式转换并写理由。
- 超时、预算耗尽、无 `halt` 都是失败。

### 4.6 首个差异报告（必须包含）

```text
sync_point:
tick:
cpu_cycle:
scanline/dot:
field:
dut:
ref:
last_bus_events:
last_device_events:
state_hash_dut:
state_hash_ref:
replay:
```

`[事实]` 报告字段来自 `docs/open6502-chip/difftest.md`；am6502 的实现会打印分歧指令号、双方 `PC/A/X/Y/SP/P` 和上下文行。

### 4.7 调试阶梯：从下往上，不要跳

```text
L4  golden workload/frame    RTL == REF，字节/哈希一致
L3  SoC state                NMI、寄存器写序列、关键状态变量
L2  CPU bus transaction      每个访问恰好一个 bus_strobe
L1  device protocol          $4016/$2006/$2007/NMI 等时序契约
L0  unit blocks              CPU/PPU/APU/mapper/controller 单元
```

`[事实]` Open6502 的 START 恢复计划正是按 L0→L4 分层，先冻结 golden、再补 CPU 周期、控制器协议、NMI 事务、post-START 状态，最后才比第 20 帧。

---

## 5. 中环操作手册（每次提交）

### 5.1 CI 流水线（按依赖顺序）

```text
1. lint / 编译（-Wall，warning 可升级）
2. L0/L1 单元 + SoC 冒烟
3. 镜像构建 + 可复现性（双构建 SHA-256）
4. 同一镜像在 REF / 周期模型 / RTL 三后端跑通
5. retire 级 DiffTest（先小镜像，再 Klaus/长测试）
6. 帧/音频哈希（有设备时）
7. 确定性双跑（同种子、同输入）
8. 失败上传 trace / waveform / manifest
```

`[事实]` `am6502.yml` 的实际顺序：构建 NEMU → 构建 GEM6502 → 平坦镜像跑 NEMU → 卡带跑 NEMU/GEM6502/RTL → 逐指令 DiffTest。其中 `Flat images on NEMU` 这一步“无 RTL、无 iverilog 也能绿”，是刻意的：让只改 C 的贡献者也能过门禁。

### 5.2 Golden 治理

- golden 哈希变更必须**单独评审**，写明原因、影响范围、重跑证据。
- 不允许为了变绿更新 golden；不允许删除/弱化测试。
- golden 文件带生成命令、输入哈希、REF 版本。
- 变更后重跑所有下游门禁，而不是只跑失败的那条。

### 5.3 确定性

- 同镜像 + 同输入 + 同种子 + 同配置 ⇒ 同事件流、同状态、同哈希。
- trace 开/关不改变状态。
- 优化前后规范化 trace 一致；精度下降必须是显式配置，不是隐藏快速路径。
- 多实例并行、复位重跑结果一致。

---

## 6. 外环操作手册（每次迭代）

### 6.1 综合/面积（先做“可解释”，再做“好看”）

`[事实]` Open6502 的 Yosys 入口（脚本跑全 top；逐模块面积报告把 `-top` 换成目标模块）：

```sh
yosys -s rtl/scripts/synth_open6502.ys
# read_verilog -sv -DSYNTHESIS
# hierarchy -check -top open6502_top
# synth -top open6502_top -flatten -noabc
# stat -tech xilinx
```

必须标注口径：

- generic cells **不是** FPGA LUT，**不是** ASIC µm²。
- 逐模块之和 **不是** 全芯片 flatten 结果。
- `SYNTHESIS` 宏可能跳过仿真初始化、限制运行时扫描；必须列出差异。
- 内存（CHR/VRAM/OAM/palette）通常是面积主因；先换 BRAM/SRAM、再做顺序扫描，而不是先优化组合逻辑。

`[事实]` Open6502 的 PPU 占 ~976k/1.02M generic cells，CPU 仅 ~4.1k；结论是“先做存储宏和 sprite 顺序扫描”，不是“CPU 太大”。

### 6.2 FPGA 门禁

全综合 → BRAM 推断 → 时序约束/Fmax → PLL/复位 → 视频/音频/输入 → ROM 装载 → 板级 bring-up → 连续运行 ≥1 小时。没有 bitstream 和板上记录，不宣称 FPGA 原型完成。

### 6.3 ASIC/PDK 门禁

SRAM 宏替换 → DFT/scan + 覆盖率 → clock/reset/power intent → 标准单元综合 → STA → PnR → DRC/LVS → pad ring/ESD/package → shuttle。**timing/DFT/DRC/LVS 未清，不宣称 tapeout-ready。**

### 6.4 生成化（Open6502 的最大缺口，新芯片应补）

`[事实]` Open6502 全仓没有 `.scala`，纯手写 SV；这是它自评 0 分的维度，也是新芯片最该补的能力。

`[迁移]` 不要一上来全盘 Chisel。正确顺序：

1. 先有 REF + DiffTest + CI（否则生成器和手写版都不可信）。
2. 选一个叶子模块（ALU/译码/PPU 子模块）用 Chisel/FIRRTL 重写。
3. 与 SV 版做 co-sim 等价对比。
4. 等价后再扩大生成范围；参数化接口先冻结。

---

## 7. 十二条硬规则

1. **REF ≠ DUT**；共享实现只能算非独立证据。
2. **一张镜像，原样跑所有后端**；harness 不改 reset 向量。
3. **镜像构建必须字节可复现**；golden 才有意义。
4. **后门不许掩盖前门**；上板/流片不能洗白未验证的 RTL。
5. **先写测试，再改 RTL**；先复现失败，再做最小架构修复。
6. **不删测试、不弱化测试、不为变绿改 golden**。
7. **超时是失败**；没有成功判据的运行不算通过。
8. **禁止隐藏回退**：timing 不得静默退回 functional，不得事后补周期。
9. **每个外部接口都要有 trace**；不可观测的接口不可验证。
10. **单时钟 + enable**，无不受控 CDC；综合子集内编码。
11. **没有命令和产物的声明不算证据**；占位目标 = 造假。
12. **没有 proprietary PDK/SRAM/ROM**；许可证和来源可追溯。

---

## 8. RTL/仓库规范（摘要，详见 `references/rtl-norms.md`）

- **时钟/复位**：master clock + `cpu_en/ppu_en/aud_en` 相位使能；异步 assert、同步 release；不在模块内部造第二时钟。
- **常量**：频率、分频、分辨率、dot/line 数放 package，不散落 magic number。
- **总线**：`bus_strobe = cpu_ce && bus_valid` 是外设唯一提交点；禁止电平保持多周期重复提交。
- **CPU**：显式状态机逐周期推进；覆盖 fetch/操作数/dummy read/RMW 双写/栈/中断/DMA stall；不允许“整条指令执行完再累加周期”。
- **存储**：RAM/ROM/MMIO 分对象；地址镜像用显式 decoder；CPU RAM 与 PRG RAM 不别名。
- **综合保护**：`ifdef SYNTHESIS` 只包仿真专用初始化/打印/断言；差异写进文档。
- **可观测**：top 保留 `dbg_*` 端口；事件带 `kind/tick/addr/data/we/source`。
- **命名/布局**：`<chip>_<block>.sv`，一模块一文件；`rtl/<block>/`、`tb/`、`sim/`、`docs/` 分层。
- **Testbench**：plusargs `+IMAGE +TRACE +MAX_INST +STOP_PC`；每条 retire 打一行标准格式；`$fatal` 失败退出。
- **Make**：`lint`、`sim`、`difftest`、`test`、`synth-<module>`、`clean`；一条命令一个程序。

---

## 9. 新芯片适配矩阵

| 类别 | Open6502 的做法 | 新芯片怎么改 |
|---|---|---|
| **保留** | 三环、S0–S6、REF/DUT 独立、一镜像多后端、CI 门禁、检查点、证据制 | 原样保留；这是方法论，不是 NES 细节 |
| **替换** | 6502 ISA、2C02 PPU、2A03 APU、iNES mapper、NES 地址图 | 换成你的 ISA/协议/地址图/外设 |
| **替换** | NEMU 作为 REF | 换成你的 ISS/行为模型/黄金 C++ 模型 |
| **替换** | Yosys/55nm 估算 | 换成你的 FPGA 器件/PDK/标准单元库 |
| **新增** | 无生成化、无形式验证、覆盖率弱 | 大芯片补 Chisel/FIRRTL、形式属性、覆盖率、功耗意图 |
| **删除** | NES 游戏兼容、mapper 矩阵、PAL/T2/T3 | 与目标芯片无关的兼容性不要继承 |
| **缩放** | 单人 12 周、小芯片 | 大芯片按团队拆 REF/CI/后端 owner；门禁不变，周期变长 |

**迁移边界**：6502 能带走的是“参考模型 + 差分验证 + CI + 检查点 + 后端检查单”的心智；带不走的是 OoO、cache 一致性、多核、NoC 和高性能微架构品味。换载体后要重新建立领域测试集，不能把 NES 的通过当成新芯片的通过。

### 9.1 示例：把 6502 映射到 RV32I

| 层 | Open6502 | RV32I 新芯片（示例） |
|---|---|---|
| REF | NEMU 6502 | Spike/NEMU RISC-V，独立构建 |
| 镜像 | flat 64 KiB / iNES | ELF/raw bin + 可选 DTB |
| 入口 | `$FFFC` reset 向量 | `_start`/ELF entry，由链接脚本写 |
| host I/O | `$F001-$F003` / `$4018-$4019` | UART MMIO 或 semihosting |
| DiffTest | PC/A/X/Y/SP/P | PC/x0-x31/CSR，逐 retire |
| 设备 | PPU/APU/mapper | UART/CLINT/PLIC/外设 |
| 权威套件 | Klaus/nestest | riscv-tests/arch-test |
| 后端 | Yosys/55nm | 你的 FPGA/PDK |

保留的是环和门禁；替换的是 ISA、镜像、设备、测试集。**不要**把 NES 的 mapper/PPU 兼容性带进 RISC-V 芯片。

---

## 10. 八维自评（只认可复现证据）

| 维度 | 权重 | 0 分 | 5 分 | 10 分 |
|---|---:|---|---|---|
| 参考模型 | 15% | 没有独立 REF | REF 能跑功能测试 | REF 可校准、可快照、可回放 |
| 锁步差分 | 25% | 只比最终状态 | 逐指令 retire + 首差异 | 总线/设备/帧/音频全进协议 |
| 生成化 | 15% | 纯手写 | 一个模块生成化并等价 | 参数化生成 + 等价回归 |
| CI 门禁 | 15% | 无 CI | lint+sim+difftest | required check + 双跑 + 失败产物 |
| 检查点/回放 | 10% | 无 | REF 侧有 | RTL/SoC 可 snapshot→restore→续跑 |
| 性能评估 | 10% | 只有墙钟 FPS | 有计数器 | 改动→cycles/阻塞归因报告 |
| 后端闭环 | 7% | 无综合 | 逐模块面积 | bitstream 或 PDK PPA + 时序 |
| 开源协作 | 3% | 无许可证/流程 | README+CONTRIBUTING | 可复现贡献 + 第三方声明 |

评分是工程判断，不是认证；每次复核留日期、理由、证据链接。不要用“游戏能跑”“综合能过”“计划完成百分比”抬分。

> Open6502 另设**形式化验证加分（上限 +5）**：Lean 4 精化证明不计入八维权重，避免与差分验证重复计分；做法与边界见 `formal/README.md`，评分记录见 `docs/scorecard-history.md`。

---

## 11. Top 反模式（看到就停）

| 症状 | 为什么错 | 替代做法 |
|---|---|---|
| “游戏能跑，所以 CPU 对了” | 兼容性不是正确性 | Klaus/nestest + retire DiffTest |
| 用 NEMU 行为当硬件规范 | REF 可能有 bug/共享实现 | 版本化 profile + 规范/硬件捕获校准 |
| TB 改写 reset 向量 | 镜像不再可信 | 构建产出向量；harness 只加载 |
| golden 变了就更新 | 掩盖回归 | 单独评审 + 原因 + 重跑 |
| 删测试让 PR 变绿 | 破坏门禁 | 修实现或明确标记 known-fail |
| `make test` 永远成功 | 假门禁 | 失败必须非零退出；超时失败 |
| 513/514 周期当实现 | 事后补周期 | 从协议/相位自然产生 |
| `functional_peek` 绕过总线 | 隐藏竞争 | 所有访问走端口/仲裁 |
| 只比最终寄存器 | 定位不到首个差异 | retire + bus + 设备分层比对 |
| 先做后端再补验证 | 把“能综合”当“正确” | 内环绿了再进外环 |
| 墙钟 FPS 当芯片性能 | 主机速度污染结论 | 确定性计数器 + manifest |
| 宣称 tapeout-ready | 未过 timing/DFT/DRC/LVS | 按 S6 逐项给证据 |

---

## 12. 新芯片交付物清单

立项时一次建齐，后续只增不减：

```text
docs/spec/chip-brief.md            目标/范围/非目标/profile
docs/spec/architecture.md          框图/时钟复位/地址图/top ports
docs/spec/image-contract.md        镜像/ABI/入口/host 端口/成功判据
docs/spec/difftest.md              REF/DUT/事件/同步点/报告格式
docs/spec/verification.md          L0–L7 测试层级与必需套件
docs/spec/checklist.md             S0–S6 门禁清单
docs/adr/ADR-00x-*.md              每个不可逆决策
docs/scorecard.md                  八维评分与证据
CONTRIBUTING.md                    PR 规则/证据要求
.github/workflows/ci.yml           lint/sim/difftest/determinism
```

模板见 `assets/templates/`。最小可用集合：`chip-spec.md`、`gate-checklist.md`、`difftest-protocol.md`、`pr-checklist.md`、`adr.md`、`mismatch-report.md`、`experiment-manifest.json`。

`experiment-manifest.json` 规则：没有 manifest 的运行不可复现，不能作为门禁证据；墙钟指标只做主机吞吐参考，不是芯片性能。

---

## 13. 快速开始（新仓库）

```sh
# 0. 在 skill 根目录执行，设定新芯片仓库路径
SKILL_DIR=$(pwd)
NEW=../my-chip
mkdir -p "$NEW"/docs/spec "$NEW"/docs/adr "$NEW"/rtl "$NEW"/tb \
         "$NEW"/sim/difftest "$NEW"/.github/workflows

# 1. 建契约（模板来自本 skill）
cp "$SKILL_DIR"/assets/templates/chip-spec.md            "$NEW"/docs/spec/chip-brief.md
cp "$SKILL_DIR"/assets/templates/difftest-protocol.md    "$NEW"/docs/spec/difftest.md
cp "$SKILL_DIR"/assets/templates/gate-checklist.md       "$NEW"/docs/spec/checklist.md
cp "$SKILL_DIR"/assets/templates/adr.md                  "$NEW"/docs/adr/ADR-001-profile.md
cp "$SKILL_DIR"/assets/templates/Makefile.snippet        "$NEW"/Makefile
cp "$SKILL_DIR"/assets/templates/ci.yml                  "$NEW"/.github/workflows/ci.yml

cd "$NEW"

# 2. 先让“空芯片”可 lint、可 sim、可失败
#    lint 必须能报错；sim 必须先有一个会 FAIL 的断言，再让它 PASS
make lint
make sim

# 3. 建 REF + 镜像 + 一条 difftest
#    镜像构建两次哈希必须相同（macOS: shasum -a 256；Linux: sha256sum）
make image && shasum -a 256 build/*.bin
make difftest

# 4. 接 CI：lint → sim → image → 多后端 → difftest → determinism
# 5. 再做 FPGA/PDK；内环没绿不碰外环
```

**第一周验收**：`make lint && make sim && make difftest` 三条命令可重复；失败时能看到首个差异；CI 对 PR 生效。

---

## 14. 事实来源（Open6502）

- `docs/learning-path.zh-CN.md`：三个环、八维、12 周路线、迁移边界。
- `docs/open6502-chip/architecture.md`：时钟/地址图/top ports/阶段 P0–P6。
- `docs/open6502-chip/difftest.md`：事件结构、同步点、报告字段、预算。
- `docs/open6502-chip/verification.md`：L0–L7、必需测试、发布门禁。
- `docs/open6502-chip/checklist.md`：P0–P6 芯片级检查单。
- `docs/open6502-chip/synthesis-area.md`：Yosys 逐模块面积与口径警告。
- `docs/55nm-tapeout-roadmap.md`：NEMU→RTL→FPGA→ASIC 阶段门禁。
- `docs/scorecard-history.md` 与 `site/learn.zh.html`：八维权重与评分口径。
- `labs/am6502/docs/design.md`：一镜像多后端、镜像契约、成功判据。
- `labs/am6502/README.zh-CN.md`：M1–M3 实际命令与首个差异输出。
- `rtl/`、`tb/`、`sim/difftest/`：RTL 规范、testbench plusargs、冒烟比对。
- `.github/workflows/am6502.yml`：CI 实际门禁顺序。
- `GEM6502/docs/requirements.md`、`coding-guide.md`：R01–R18、G0–G6、事件/端口/检查点纪律（更偏 C 模型，但规则可迁移）。

> 外部方法参考：香山官方文档（敏捷开发、半年迭代/流片）、一生一芯；本 skill 只复用**流程与接口传统**，不复制第三方源码。
