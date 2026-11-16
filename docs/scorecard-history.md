# 香山对齐评分：分数记录

> 网页版曲线与下表同源：[site/assets/js/scorecard-history.js](../site/assets/js/scorecard-history.js)
> 里的 `HISTORY` 数组驱动 [site/learn.zh.html](../site/learn.zh.html) 与
> [site/learn.html](../site/learn.html) 的曲线和记录表。**改分时两处一起改，并核对八维表。**

## 口径

- 总分 = Σ（维度分 0–10 × 权重），权重：参考模型 15%、锁步差分 25%、生成化 15%、
  CI 门禁 15%、检查点 10%、性能 10%、后端 7%、开源协作 3%。
- 只认可复现的证据：能跑、能查、能重复。计划、口号和百分比进度不计分。
- 分数是工程判断，不是认证；每次复核留日期、理由与证据链接。

## 记录

| 日期 | 总分 | 等级 | 变化 | 这次改了什么 |
|---|---|---:|---:|---|
| 2026-11-15 | 34.4 | D+ | — | 首版评分（网站上线）：参考模型 8.5、锁步 4.0、生成化 0、CI 1.5、检查点 4.0、性能 2.5、后端 2.0、协作 5.0 |
| 2026-11-16 | 46.8 | C | +12.4 | am6502 M1–M3 落地 + 根 MIT 许可证（明细见下） |

## 2026-11-16 的明细

| 维度 | 旧 → 新 | 证据 |
|---|---:|---|
| 锁步差分验证 | 4.0 → 6.5 | `make difftest`：同一 64 KiB 镜像在 NEMU 与 RTL 上逐条 retire 比对（`PC/A/X/Y/SP/P`），首个分歧带上下文；hello 1,216 条、Klaus 局部 20 万条一致 |
| CI 门禁 | 1.5 → 4.5 | [.github/workflows/am6502.yml](../.github/workflows/am6502.yml)：平坦镜像、卡带在 NEMU/GEM6502/RTL 三后端、逐指令 DiffTest |
| 参考模型 | 8.5 → 9.0 | [labs/am6502](../labs/am6502/README.zh-CN.md)：镜像由构建产出、reset 向量自带、逐字节可复现 |
| 开源协作 | 5.0 → 8.0 | 根 [LICENSE](../LICENSE)（MIT）与 `THIRD_PARTY_NOTICES.md` 已发布，CONTRIBUTING 解封 |

未变的维度及理由：生成化 0（仍无 `.scala`）；检查点 4.0（RTL 侧无状态导出/恢复）；
性能 2.5（无计数器与改动收益报告）；后端 2.0（无 bitstream/PDK）。

## 下次复核要看的证据

- `make -C labs/am6502 test`、`make -C labs/am6502 difftest` 的实际输出；
- `.github/workflows/am6502.yml` 在 GitHub 上的运行记录（目前尚未设为 required check）；
- 帧哈希一致性、总线/PPU 事务比对是否进入协议（锁步维度的下一档）；
- 是否出现第一个 Chisel 模块（生成化维度的唯一开门条件）。

## 怎么加一个点

1. 在 `site/assets/js/scorecard-history.js` 的 `HISTORY.points` 末尾追加：
   `{ date, total, grade, summary: {zh,en}, changes: {zh:[...], en:[...]} }`；
2. 同步本文件的「记录」表与明细；
3. 更新 `site/learn.zh.html` / `site/learn.html` 的英雄区分数、八维表与 `<title>`/meta；
4. `node -e` 冒烟一次图表（见仓库提交里的用法），再提交。
