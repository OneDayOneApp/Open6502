# xiangshan-chip-flow

把 Open6502 已经跑通的香山式芯片开发流程，提炼成可复用的 skill：

```text
参考模型 → 同一镜像 → 锁步 DiffTest → CI 门禁 → 检查点/回放 → FPGA → PDK/ASIC
```

## 怎么用

1. **新芯片立项**：从 `SKILL.md` 第 1 节“先填这张卡”开始；填不出不要写 RTL。
2. **搭验证体系**：读 `references/contracts.md` + `references/verification.md`，套 `assets/templates/difftest-protocol.md`。
3. **写 RTL/仓库**：读 `references/rtl-norms.md`，套 `assets/templates/Makefile.snippet` 与 `ci.yml`。
4. **评审/上板/流片**：读 `references/operating-model.md` 的 S0–S6 与 `references/backend.md`，用 `gate-checklist.md` 逐项打勾。
5. **踩坑排查**：先查 `references/anti-patterns.md`。

## 目录

```text
SKILL.md                        主文件：触发、七道门、内/中/外环、硬规则、迁移矩阵
install.sh                      安装到 ~/.dsh/skills 或 ./.dsh/skills
references/
  operating-model.md            三个环、S0–S6、九维评分、12 周节奏、证据台账
  contracts.md                  镜像/REF-DUT/事件/同步点/检查点/manifest/精度标签
  verification.md               L0–L7、必需套件、DiffTest 阶梯、CI、发布门禁
  rtl-norms.md                  时钟复位/总线/CPU/存储/综合保护/可观测/TB/Make/PR
  backend.md                    Yosys 口径、面积归因、FPGA/ASIC 门禁、PPA、生成化
  anti-patterns.md              22 个反模式：症状/根因/替代/证据
assets/templates/
  chip-spec.md                  芯片 brief
  gate-checklist.md             S0–S6 门禁清单
  difftest-protocol.md          DiffTest 协议
  pr-checklist.md               PR 检查单 + 证据块
  adr.md                        架构决策记录
  mismatch-report.md            首个差异报告
  experiment-manifest.json      实验 manifest
  Makefile.snippet              lint/sim/image/difftest/test/determinism/synth
  ci.yml                        GitHub Actions 门禁
```

## 安装为 DSH skill（可选）

```sh
cd skills/xiangshan-chip-flow

# 方式一：用户级（所有项目可见）
./install.sh

# 方式二：仓库级（跟着仓库走）
./install.sh --repo
```

也可以手动复制：`cp -R skills/xiangshan-chip-flow ~/.dsh/skills/`。

之后在会话里说“用 xiangshan-chip-flow …”即可触发。可直接复制的说法：

- “用 xiangshan-chip-flow 给这颗 RV32I SoC 立项，先填 chip brief 和 S0 清单。”
- “用 xiangshan-chip-flow 检查当前 RTL 的锁步 DiffTest 缺口，给出下一步。”
- “用 xiangshan-chip-flow 评审这版能不能宣称 tapeout-ready。”

## 来源与边界

- 事实来源：`docs/learning-path.zh-CN.md`、`docs/open6502-chip/*`、`docs/55nm-tapeout-roadmap.md`、`labs/am6502/*`、`GEM6502/docs/*`、`rtl/`、`tb/`、`sim/difftest/`、`.github/workflows/am6502.yml`、`site/learn.zh.html`。
- 本 skill 只复用 **流程与接口传统**（三个环、门禁、镜像契约、DiffTest 协议、检查单），不复制第三方源码。
- `[事实]` 标记 Open6502 已验证做法；`[迁移]` 是新芯片推广，落地前按目标芯片重新评审。
- NES/6502 是教练机：能带走方法，带不走微架构品味；换载体后要重建领域测试集。
