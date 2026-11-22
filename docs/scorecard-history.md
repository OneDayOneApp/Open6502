# 香山对齐评分：分数记录

> 网页版曲线与下表同源：[site/tools/scorecard-history.js](../site/tools/scorecard-history.js)
> 里的 `HISTORY` 数组，经 [site/tools/render-scorecard.js](../site/tools/render-scorecard.js)
> 生成**静态 SVG + HTML**，写进 [site/learn.zh.html](../site/learn.zh.html) 与
> [site/learn.html](../site/learn.html) 的 `score-history` 标记之间。
> **改分时：改数据 → 跑生成器 → 更新本文件 → 核对九维表。**

## 口径

- 总分 = Σ（维度分 0–10 × 权重），九维权重：参考模型 14%、锁步差分 22%、
  形式化验证 10%、生成化 14%、CI 门禁 14%、检查点 9%、性能 9%、后端 6%、
  开源协作 2%。
- 只认可复现的证据：能跑、能查、能重复。计划、口号和百分比进度不计分。
- **v2 口径**：2026-11-22 起形式化验证升为第九维。新增 10% 后其余八维按比例归一化，
  历史点按新权重重算（2026-11-15 34.4 → 30.9、2026-11-16 46.8 → 41.9）；
  **v1 与 v2 总分不可直接比较**。
- 分数是工程判断，不是认证；每次复核留日期、理由与证据链接。

## 记录

| 日期 | 总分 | 等级 | 变化 | 这次改了什么 |
|---|---|---:|---:|---|
| 2026-11-15 | 30.9 | D+ | — | 首版评分（原八维基线，按九维 v2 权重重算） |
| 2026-11-16 | 41.9 | C | +11.0 | am6502 M1–M3 落地 + 根 MIT 许可证（按九维重算，明细见下） |
| 2026-11-22 | 45.9 | C | +4.0 | 形式化验证升为第九维（10%）；历史点重算（明细见下） |

## 2026-11-16 的明细

| 维度 | 旧 → 新 | 证据 |
|---|---:|---|
| 锁步差分验证 | 4.0 → 6.5 | `make difftest`：同一 64 KiB 镜像在 NEMU 与 RTL 上逐条 retire 比对（`PC/A/X/Y/SP/P`），首个分歧带上下文；hello 1,216 条、Klaus 局部 20 万条一致 |
| CI 门禁 | 1.5 → 4.5 | [.github/workflows/am6502.yml](../.github/workflows/am6502.yml)：平坦镜像、卡带在 NEMU/GEM6502/RTL 三后端、逐指令 DiffTest |
| 参考模型 | 8.5 → 9.0 | [labs/am6502](../labs/am6502/README.zh-CN.md)：镜像由构建产出、reset 向量自带、逐字节可复现 |
| 开源协作 | 5.0 → 8.0 | 根 [LICENSE](../LICENSE)（MIT）与 `THIRD_PARTY_NOTICES.md` 已发布，CONTRIBUTING 解封 |

未变的维度及理由：生成化 0（仍无 `.scala`）；检查点 4.0（RTL 侧无状态导出/恢复）；
性能 2.5（无计数器与改动收益报告）；后端 2.0（无 bitstream/PDK）。这些维度分数在
v2 重算中保持不变，只是权重变了。

## 2026-11-22 的明细（第九维与权重归一化）

| 项目 | 旧 → 新 | 证据 |
|---|---:|---|
| 形式化验证（第九维） | 0 → 4.0/10 | [formal/](../formal/README.md)：151 条官方指令译码与 RTL 分派一致（`dispatch_check` + `scripts/check_rtl_sync.py`）；ADC/SBC（二进制与 BCD）、CMP、移位、BIT 的全操作数规格（`bv_decide`）；19 条指令的 `mrun n m = step m` 精化；15 个内核执行程序测试；4 个程序 Lean↔RTL 协同仿真；`lake build` 约 15 秒、无 `sorry` |
| 其余八维权重 | 15/25/15/15/10/10/7/3 → 14/22/14/14/9/9/6/2 | 新增 10% 后按比例归一化；八维**分数**不变 |
| 历史点 | 34.4 → 30.9；46.8 → 41.9 | 重新加权，不是证据退步 |

**为什么是 4.0/10**：精化覆盖 19/151 条指令；IRQ/NMI 入口与复位序列未精化；周期数按
当前 RTL（无页跨越惩罚、部分堆栈 dummy 周期缺失），不是 datasheet 时序；ALU 的
`bv_decide` 引理带一个证书校验信任点。译码一致性与 BCD/ALU 全输入证明是强证据，
但指令级覆盖仍不足一半，因此给 4.0。

**为什么八维分数不变**：生成化仍是 0，帧级/总线比对与后端闭环没有变化，checkpoints 与
性能评估也没有新增证据；形式化单独成维，避免与锁步差分/CI 重复计分。

**关于比较**：v2 的 45.9 与 v1 的 46.8 / 49.3 不可直接比较。v2 口径下的曲线是
**30.9 → 41.9 → 45.9**，仍然单调；即新增第九维后，同样的历史证据被重新归一化到
更低的绝对分，这是权重变化的算术结果。

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
3. 同步本文件的「记录」表与明细，并说明是哪个维度的分数变化；
4. 更新 `site/learn.zh.html` / `site/learn.html` 的英雄区分数、九维表与 `<title>`/meta；
5. 打开页面（或 `node` 冒烟）确认曲线与表格，再提交。
