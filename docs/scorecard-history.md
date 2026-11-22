# 香山对齐评分：分数记录

> 网页版曲线与下表同源：[site/tools/scorecard-history.js](../site/tools/scorecard-history.js)
> 里的 `HISTORY` 数组，经 [site/tools/render-scorecard.js](../site/tools/render-scorecard.js)
> 生成**静态 SVG + HTML**，写进 [site/learn.zh.html](../site/learn.zh.html) 与
> [site/learn.html](../site/learn.html) 的 `score-history` 标记之间。
> **改分时：改数据 → 跑生成器 → 更新本文件 → 核对八维表。**

## 口径

- 总分 = Σ（维度分 0–10 × 权重），权重：参考模型 15%、锁步差分 25%、生成化 15%、
  CI 门禁 15%、检查点 10%、性能 10%、后端 7%、开源协作 3%。
- 只认可复现的证据：能跑、能查、能重复。计划、口号和百分比进度不计分。
- **加分项**：形式化验证（上限 +5），不计入八维权重；总分 = 八维加权 + 加分。
- 分数是工程判断，不是认证；每次复核留日期、理由与证据链接。

## 记录

| 日期 | 总分 | 等级 | 变化 | 这次改了什么 |
|---|---|---:|---:|---|
| 2026-11-15 | 34.4 | D+ | — | 首版评分（网站上线）：参考模型 8.5、锁步 4.0、生成化 0、CI 1.5、检查点 4.0、性能 2.5、后端 2.0、协作 5.0 |
| 2026-11-16 | 46.8 | C | +12.4 | am6502 M1–M3 落地 + 根 MIT 许可证（明细见下） |
| 2026-11-22 | 49.3 | C | +2.5 | Lean 4 形式化验证落地（八维核心 46.8 不变，形式化加分 +2.5） |

## 2026-11-16 的明细

| 维度 | 旧 → 新 | 证据 |
|---|---:|---|
| 锁步差分验证 | 4.0 → 6.5 | `make difftest`：同一 64 KiB 镜像在 NEMU 与 RTL 上逐条 retire 比对（`PC/A/X/Y/SP/P`），首个分歧带上下文；hello 1,216 条、Klaus 局部 20 万条一致 |
| CI 门禁 | 1.5 → 4.5 | [.github/workflows/am6502.yml](../.github/workflows/am6502.yml)：平坦镜像、卡带在 NEMU/GEM6502/RTL 三后端、逐指令 DiffTest |
| 参考模型 | 8.5 → 9.0 | [labs/am6502](../labs/am6502/README.zh-CN.md)：镜像由构建产出、reset 向量自带、逐字节可复现 |
| 开源协作 | 5.0 → 8.0 | 根 [LICENSE](../LICENSE)（MIT）与 `THIRD_PARTY_NOTICES.md` 已发布，CONTRIBUTING 解封 |

未变的维度及理由：生成化 0（仍无 `.scala`）；检查点 4.0（RTL 侧无状态导出/恢复）；
性能 2.5（无计数器与改动收益报告）；后端 2.0（无 bitstream/PDK）。

## 2026-11-22 的明细（形式化加分）

| 项目 | 旧 → 新 | 证据 |
|---|---:|---|
| 形式化验证（加分，上限 +5） | 0 → +2.5 | [formal/](../formal/README.md)：151 条官方指令译码与 RTL 分派一致（`dispatch_check` + `scripts/check_rtl_sync.py`）；ADC/SBC（二进制与 BCD）、CMP、移位、BIT 的全操作数规格（`bv_decide`）；19 条指令的 `mrun n m = step m` 精化；15 个内核执行程序测试；4 个程序 Lean↔RTL 协同仿真；`lake build` 约 15 秒、无 `sorry` |

**为什么是 2.5/5**：精化覆盖 19/151 条指令；IRQ/NMI 入口与复位序列未精化；周期数按当前
RTL（无页跨越惩罚、部分堆栈 dummy 周期缺失），不是 datasheet 时序；ALU 的 `bv_decide`
引理带一个证书校验信任点。这些都是可复核的边界，所以只给一半分。

**八维核心为什么不变**：生成化仍是 0，帧级/总线比对与后端闭环没有变化，checkpoints 与
性能评估也没有新增证据；形式化是八维之外的新能力，因此单列为加分项，避免双重计分。

**口径**：本次起总分 = 八维加权 + 形式化加分（上限 +5）。49.3 = 46.8 + 2.5，历史点仍是
八维口径，可直接比较。

## 下次复核要看的证据

- `cd formal && lake build && make check-rtl check-cosim` 的实际输出；
- 精化覆盖是否从 19 条继续推进（官方 151 条、IRQ/NMI 入口、复位与页跨越时序）；
- `make -C labs/am6502 test`、`make -C labs/am6502 difftest` 的实际输出；
- `.github/workflows/am6502.yml` 在 GitHub 上的运行记录（目前尚未设为 required check）；
- 帧哈希一致性、总线/PPU 事务比对是否进入协议（锁步维度的下一档）；
- 是否出现第一个 Chisel 模块（生成化维度的唯一开门条件）。

## 怎么加一个点

1. 在 `site/tools/scorecard-history.js` 的 `HISTORY.points` 末尾追加：
   `{ date, total, grade, summary: {zh,en}, changes: {zh:[...], en:[...]} }`；
2. 运行 `node site/tools/render-scorecard.js`，重新生成两页的静态曲线与记录表；
3. 同步本文件的「记录」表与明细；加分项写进 `total`，在 changes 里说明拆解；
4. 更新 `site/learn.zh.html` / `site/learn.html` 的英雄区分数、八维表与 `<title>`/meta；
5. 打开页面（或 `node` 冒烟）确认曲线与表格，再提交。
