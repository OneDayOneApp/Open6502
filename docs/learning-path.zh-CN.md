# 学习路径：用 6502 练一遍香山式芯片开发流程

> 面向“想学香山模式、拿一颗小芯片练手”的读者。本文不介绍 NES，也不承诺流片；只回答一个问题：**在这个仓库里按什么顺序练，才能把香山的方法论变成肌肉记忆。**
>
> 最后更新：2026-11-15。状态数字会变，以 `git log`、`rtl/`、`sim/difftest/` 和 CI 实际输出为准。
>
> 配套网页：[site/learn.zh.html](../site/learn.zh.html)（三个环、八维评分、12 周路线）。

---

## 1. 核心故事与三个环

一句话：

> **改一行 RTL → 一条命令 → 分钟级看到 PASS 或首个差异。**

香山把这件小事做成了基础设施：NEMU 参考模型、Difftest 锁步比对、敏捷验证流程、约半年的微结构迭代与流片周期。这个仓库已经有一半——NEMU 参考、黄金帧、验证计划、门禁清单；缺的恰好是让它“快”的那一半——锁步 Difftest、测试 CI、生成化。

| 环 | 时间尺度 | 练什么 | 香山对应 |
|---|---|---|---|
| **内环** | 分钟 | 锁步 Difftest、确定性回放、首个差异定位 | 参考模型 + 差分验证 |
| **中环** | 每次提交 | CI 回归、golden 哈希、失败 trace、门禁清单 | 敏捷验证流程 |
| **外环** | 每次迭代 | 综合、FPGA、开放 PDK 后端、PPA 报告 | 半年迭代 + 流片 |

香山用半年跑一次外环；在这个仓库里，三个环可以压缩到一周。**练的是环与环之间的门禁纪律，不是环的尺寸。**

---

## 2. 香山模式拆成八件，仓库现状

| # | 香山模式的做法 | 在 6502 上怎么练 | 本仓库现状 |
|---|---|---|---|
| 1 | 生成化描述（Chisel/FIRRTL，参数化、可复用） | 用 Chisel 重写 CPU 或 PPU 的一个模块，与手写 SV 做等价对比 | ❌ 全仓 0 个 `.scala`，纯手写 SystemVerilog |
| 2 | 参考模型（NEMU 作为 golden） | 已有的 6502EMU 就是 | ✅ 有，质量可用 |
| 3 | 锁步 Difftest（逐指令/逐周期比对，首个差异定位） | 把 final-state 冒烟升级成 retire 级 + 总线事务级 | 🟡 只到最终状态，[sim/difftest/README.md](../sim/difftest/README.md) 自认 “not a cycle-level online DiffTest” |
| 4 | 检查点 / 确定性回放 | 任意周期 snapshot → restore → 继续比对 | 🟡 REF 侧有 snapshot/hash/replay，RTL 侧没有 |
| 5 | CI 回归门禁 | 每个 PR 跑 lint + sim + difftest + golden，失败上传 trace | ❌ 只有 [.github/workflows/pages.yml](../.github/workflows/pages.yml)，只部署网站、不跑测试 |
| 6 | 性能采样 / 改动评估 | GEM6502 加计数器，做“改一个参数 → cycles 变化多少”的实验报告 | 🟡 G0–G2 仍 partial，无计数器体系 |
| 7 | 后端闭环（FPGA → 综合 → DFT → 流片） | Yosys → OpenROAD/SKY130 跑一个小模块的 PPA | ❌ 无 bitstream、无 PDK；55nm 是商业 PDK 路线 |
| 8 | 开源协作与许可 | 根许可证 + 可复现的贡献流程 | 🟡 [CONTRIBUTING.md](../CONTRIBUTING.md) 很完整，但明写“发布许可证前请不要贡献” |

结论：**第 2 件做得最好；第 1、3、5 件是香山模式的三根支柱，也是这里最缺的三件。**

---

## 3. 12 周路线（建议节奏，不是承诺）

| 周 | 做什么 | 产出 | 对应能力 |
|---|---|---|---|
| 1–2 | Difftest 升级为锁步：逐指令 retire + 总线事务比对 | `make difftest` 一条命令；失败打印首个差异 | 验证内环 |
| 3–4 | CI 门禁：lint / sim / difftest / golden；同种子跑两遍验确定性；失败上传 trace | PR 不绿不能合 | 回归纪律 |
| 5–6 | 检查点：任意周期 snapshot → restore → 继续锁步 | 从第 N 周期复现同一分歧 | 可调试性 |
| 7–9 | **Chisel 重写一个模块**（建议 PPU 或 CPU 的 ALU/译码），与 SV 版本 co-sim 对比 | 两版等价 + 参数化 demo | 生成化（香山味最重） |
| 10–11 | GEM6502 加计数器，做一次“改总线延迟 → cycles / 阻塞归因”的实验报告 | 一页可复现报告 | 改动评估 |
| 12 | Yosys → OpenROAD/SKY130 跑小模块 PPA | 一页面积/时序报告 | 后端闭环 |

前三周做完，你就拥有了香山方法论里最难自学、也最值钱的部分；第 7–9 周才开始真正“像香山那样写硬件”。

---

## 4. 能迁移什么，不能迁移什么

| 能迁移（这正是选 6502 的理由） | 迁移不了（别指望） |
|---|---|
| 参考模型 + 差分验证的完整心智 | OoO、推测执行、分支预测 |
| 生成化 / 参数化的硬件构建方式 | Cache 一致性、多核、NoC |
| CI、回归、golden、确定性复现 | 真实 PPA 优化经验（6502 太小，面积/频率故事弱） |
| 检查点、回放、失败定位 | 高性能微架构的设计品味 |
| 后端流程与流片门禁的“检查单思维” | 商业 PDK / DFT / 签核的工程强度 |

**6502 是教练机，用来练仪表和检查单，不是战机。** 想练微架构，第二阶段应换 RISC-V 小核（果壳 NutShell 就是香山的前身），而不是继续给 NES 加外设。

---

## 5. 怎么算“练会了”

- **内环**：任意一次 RTL 改动，一条命令在 2 分钟内给出 PASS/FAIL，失败能定位到首个周期或指令。
- **中环**：PR 不绿不能合；同种子两次运行结果一致；golden 哈希变更需要评审并记录。
- **外环**：能把一个小模块从 RTL 推到 SKY130 的面积/时序报告，并解释主要瓶颈。

达不到这三条，说明还在“手工调试”，没进“敏捷开发”。

---

## 6. 当前差距（按优先级）

| 优先级 | 差距 | 现状 | 对应工作 |
|---|---|---|---|
| P0 | 锁步 Difftest | final-state 冒烟 | 逐指令 retire + 总线/PPU/APU/mapper 事务比对 |
| P0 | 测试 CI | 只有 `pages.yml` | GitHub Actions：lint、sim、difftest、golden、确定性双跑 |
| P1 | 生成化 | 纯手写 SV | Chisel 重写一个模块并做等价性对比 |
| P1 | RTL 侧检查点 | REF 有，RTL 无 | 状态导出/恢复 + 继续比对 |
| P2 | 性能计数 | GEM6502 仅有部分计数 | 请求/服务/等待/阻塞归因 |
| P2 | 开放 PDK 后端 | 无 bitstream、无 PDK | Yosys → OpenROAD/SKY130 小模块演练 |

---

## 7. 参考

- [香山官方文档](https://docs.xiangshan.cc/) — 敏捷开发流程、半年迭代与流片周期的原始表述。
- [一生一芯](https://ysyx.oscc.cc/project/intro.html) — 从零设计 RISC-V 处理器芯片的教学路径。
- [NES golden reference](../docs/nes-golden-reference/README.md) — 本仓库里最接近“香山式验证计划”的文档：73 项 feature matrix、test plan、checklist。
- [DiffTest 协议](open6502-chip/difftest.md) — 现有的比对协议，锁步改造从这里开始。
- [55nm 路线图](55nm-tapeout-roadmap.md) — 外环的长期目标；个人练手请把“流片”降级为流程演练。
