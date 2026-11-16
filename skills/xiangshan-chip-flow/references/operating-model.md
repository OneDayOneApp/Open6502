# 运行模型：三个环、七道门、八维评分

> 这份文档回答“为什么按这个顺序做、做到什么程度算过”。具体接口和命令见 SKILL.md 与其余 references。

## 1. 三个环的职责边界

| 环 | 触发 | 时间预算 | 负责人 | 失败时怎么办 |
|---|---|---|---|---|
| 内环 | 任何 RTL/REF 改动 | ≤ 2 分钟（小芯片） | RTL/验证工程师 | 停，定位首个差异；不许带着失败进中环 |
| 中环 | 每次 push/PR | ≤ 15 分钟 | CI owner | PR 红即阻断；失败产物自动上传 |
| 外环 | 每个迭代/里程碑 | 小时–天 | 后端/FPGA owner | 记录 PPA 与限制；不许回写“RTL 已正确” |

**环的尺寸可以变，顺序不能变。** 先做 Chisel 再补 DiffTest，会得到两个都不可信的实现；先做后端再补门禁，会把“能综合”误当成“正确”。

## 2. 七道门（S0–S6）的细化

### S0 契约冻结

产出：

- chip brief：目标、profile、范围、非目标、成功判据。
- 地址图：每个 region 的 base/size/权限/镜像/副作用。
- 时钟/复位：master、分频、相位、assert/release 方式、跨域规则。
- top ports：所有外部接口 + 调试观测口。
- 镜像/ABI：容器、装载地址、入口、host 端口、每程序判据。
- REF/DUT 边界：谁是 golden，谁被综合，共享代码如何标注。
- ADR-001..003：profile、端口/事务所有权、legacy/新模型共存。

退出：接口无 TBD；地址冲突检查通过；评审记录在案。

### S1 参考模型 + 镜像

产出：

- 独立 REF（ISS/行为模型/黄金 C++），可构建、可版本化。
- 同一镜像在 REF 上跑通；镜像构建两次字节相同。
- host 端口（putc/exit/输入/帧计数）语义写清。
- 每程序成功判据（`halt` / `pc:<addr>` / 帧哈希）。

退出：REF 通过 ISA/功能套件；镜像 SHA-256 可复现；超时判失败。

### S2 单元 + 总线

产出：

- L0 单元测试：package/decode/counter/ALU/寄存器块。
- L1 定向测试：CPU 指令子集、设备协议、mapper/外设。
- 总线单点提交：`bus_strobe = cpu_ce && bus_valid`；每个访问恰好一次。
- SoC 冒烟：CPU + bus + memory + 一个设备。

退出：`make lint` + `make sim` 全绿；总线事务唯一性有测试。

### S3 锁步 DiffTest

产出：

- retire 级比对：PC/A/X/Y/SP/P/cycles。
- 同一镜像、同一输入、同一初始状态。
- 首个差异报告 + 回放命令。
- 预算：Verilator + REF ≥ 10^6 cycles/s；超预算要解释。

退出：规定指令数/周期数零差异；失败可定位到首个同步点。

### S4 CI + 确定性 + 检查点

产出：

- required checks：lint、sim、image、多后端、difftest、determinism。
- 双跑：同种子两次结果一致；trace 开关不改变状态。
- drained checkpoint：snapshot → restore → 继续跑 == 连续跑。
- 失败产物：trace、waveform、manifest、最后事件窗口。

退出：PR 红不能合；恢复后事件流一致；失败报告能复现。

### S5 FPGA

产出：

- 全综合、BRAM 推断、时序约束、Fmax 报告。
- PLL/复位、视频/音频/输入、ROM/Flash/SD 装载。
- bitstream + 板上运行记录 + ≥1 小时稳定性。

退出：真实板卡稳定跑目标负载；无“仿真专用假设”。

### S6 ASIC/PDK

产出：

- SRAM 宏替换、DFT/scan + 覆盖率、clock/reset/power intent。
- 标准单元综合、STA、PnR、DRC/LVS、pad ring/ESD/package。
- tapeout checklist 逐项评审。

退出：timing/DFT/DRC/LVS 全清；shuttle/资金落实；无口头就绪。

## 3. 八维评分：怎么打分才不虚

口径：**总分 = Σ（维度分 0–10 × 权重）**。只认可复现证据：能跑、能查、能重复。计划、口号、百分比进度不计分。

| 维度 | 权重 | 10 分长什么样 | 常见虚高 |
|---|---:|---|---|
| 参考模型 | 15% | 独立、可校准、可快照/回放、有测试套件 | “能玩”就算 8 分 |
| 锁步差分 | 25% | 指令/总线/设备/帧/音频全进协议，首差异可回放 | 只比最终寄存器 |
| 生成化 | 15% | 参数化生成 + 与手写版等价回归 | 有一个 `.scala` 就满分 |
| CI 门禁 | 15% | required check + 双跑 + 失败产物 + 覆盖率 | 有 workflow 但从不失败 |
| 检查点/回放 | 10% | RTL/SoC snapshot→restore→续跑一致 | 只有 REF 侧有 |
| 性能评估 | 10% | 计数器 + 阻塞归因 + 改动收益报告 | 墙钟 FPS |
| 后端闭环 | 7% | bitstream 或 PDK PPA + 时序 | 逐模块 generic cells |
| 开源协作 | 3% | 许可证 + 贡献流程 + 第三方声明 | README 写得好 |

**复核纪律**：

1. 每次复核留日期、总分、每维变化、证据链接。
2. 分数只往上走；下降必须写原因（证据过期/回归）。
3. 不把“完成百分比”当分数；完成度不是证据。
4. 不用一个“全部通过”掩盖未覆盖区域；未覆盖就标未覆盖。

## 4. 12 周节奏（单人/小团队）

| 周 | 目标 | 产出 | 对应门 |
|---|---|---|---|
| 1–2 | 契约 + REF + 镜像 | chip brief、地址图、REF 跑通、镜像可复现 | S0–S1 |
| 3–4 | 单元 + 总线 + retire DiffTest | lint/sim 绿、`make difftest` 首差异 | S2–S3 |
| 5–6 | CI + 确定性 + 检查点 | required check、双跑、restore==连续 | S4 |
| 7–9 | 生成化一个模块 | Chisel/SV 等价 + 参数化 demo | S4 扩展 |
| 10–11 | 性能计数器 + 实验报告 | 改动→cycles/阻塞归因 | 性能维 |
| 12 | FPGA/开放 PDK PPA | bitstream 或面积/时序报告 | S5/S6 演练 |

节奏是建议，不是承诺；**前 4 周做完，方法论里最难自学的部分就拿到了**。

## 5. 评审与证据台账

每次评审只问四个问题：

1. 这条声明的**命令**是什么？
2. **产物**在哪里？
3. **已知限制**是什么？
4. 失败时能不能**复现首个差异**？

证据台账建议字段：

```text
date:
claim:
command:
artifact:
ref_version:
dut_version:
image_hash:
config_hash:
result: PASS/FAIL/PARTIAL
limits:
reviewer:
```

没有台账的声明，下一次评审视为不存在。

## 6. 角色最小配置

| 角色 | 职责 | 小团队可兼任 |
|---|---|---|
| REF owner | 参考模型、校准、快照 | 验证负责人 |
| RTL owner | 微架构、总线、设备 | 同上，但不要和 REF 同一人写同一模块 |
| CI owner | 门禁、双跑、失败产物 | RTL owner |
| 后端 owner | 综合、FPGA、PDK、DFT | 独立或外协 |

**独立性底线**：REF 和 DUT 的核心实现不能由同一份代码派生；如果必须共享，必须在报告里标“非独立证据”。

## 7. 已填样本：Open6502（截至 2026-11-16）

> 用途：让你知道“填好”长什么样。状态会变，以 `git log`/CI 实际输出为准。

| 门 | 状态 | 现有证据 | 缺口 |
|---|---|---|---|
| S0 契约 | 部分 | `architecture.md`、`difftest.md`、`checklist.md`、地址图、top ports | register/memory map v0 未冻结；视频/音频电气接口未冻结 |
| S1 REF + 镜像 | 基本可用 | NEMU 6502EMU 可玩；Klaus PASS；am6502 镜像由构建产出、实测双构建可复现 | PPU/APU/mapper 未全进 REF 校准；CI 未强制双构建 |
| S2 单元 + 总线 | 部分 | 28 RTL tests PASS；lint 通过；CPU/bus/WRAM SoC 冒烟 | 总线单点提交未全外设显式化；DMC 仲裁未完成 |
| S3 锁步 | 部分 | `sim/difftest` final-state/PC trace；am6502 逐指令 hello 1,216 条一致 | 总线/PPU/APU/mapper 事务未进协议；无帧级锁步 |
| S4 CI + 确定性 + 检查点 | 部分 | `am6502.yml` 跑平坦/卡带三后端/DiffTest | 未设 required check；无双跑；RTL 侧无检查点 |
| S5 FPGA | 未开始 | FPGA wrapper 存在 | 无 bitstream、无板级验证 |
| S6 ASIC | 规划 | Yosys 逐模块面积；55nm 估算 | 无 PDK/DFT/STA/DRC/LVS |

**读法**：这不是“失败清单”，而是“下一道门在哪”。Open6502 的价值在于把 S0–S4 的骨架跑通，S5/S6 留作演练。

---

## 8. 停止条件（什么时候该停）

- 内环 2 分钟给不出 PASS/FAIL：先修工具链，不要继续加 RTL。
- 首个差异无法复现：先修确定性（种子/初始化/时间），不要猜。
- 同一失败出现三次：升级为设计问题，开 ADR，不要继续打补丁。
- 外环失败但内环绿：优先怀疑约束/综合语义，不要改 golden。
- 团队开始用“游戏能跑”当验收：停下来，回到 S3。
