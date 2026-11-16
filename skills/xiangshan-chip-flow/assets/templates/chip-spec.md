# <CHIP> 芯片规格（Chip Brief）

> 复制到 `docs/spec/chip-brief.md`。填不出“必须回答”的项，不要进入 RTL 编码。

## 1. 一句话

<CHIP> 是面向 <用户/场景> 的 <类型>，提供 <核心能力>。

## 2. 范围

| 项 | v0 | 非目标（明确不做） |
|---|---|---|
| ISA/协议 | | |
| Profile | | |
| 时钟/复位 | | |
| 地址图 | | |
| 外设 | | |
| 视频/音频 | | |
| 调试 | | |
| 封装/板级 | | |

## 3. 必须回答

| 问题 | 答案 |
|---|---|
| 目标芯片是什么？ | |
| ISA/协议及版本？ | |
| Profile（时钟/复位/电气可见行为）？ | |
| v0 必须有什么？ | |
| 明确不做什么？ | |
| 外部接口有哪些？ | |
| 参考模型用哪个？ | |
| 精度目标（functional/approximate/calibrated）？ | |
| 后端（FPGA/PDK/SRAM/DFT）？ | |
| 团队/周期？ | |

## 4. 框图和时钟

```text
<master clk> -> <div/enable> -> <block>
                    |
                <bus> <-> <memory/device>
```

| 时钟/使能 | 频率 | 用途 |
|---|---:|---|
| `clk_<freq>` | | master |
| `<domain>_en` | | |

复位：异步 assert / 同步 release；复位值见 RTL 规范。

## 5. 地址图

| Base | Size | 对象 | 权限 | 镜像/副作用 |
|---|---:|---|---|---|
| | | | R/W | |

## 6. Top ports

```systemverilog
module <chip>_top (
  input  logic clk,
  input  logic rst_n,
  // video/audio/storage/input/debug
);
```

## 7. 镜像/ABI 契约

| 项 | 值 |
|---|---|
| 容器 | |
| 装载地址 | |
| 入口/reset 向量 | 由构建写 |
| host I/O | |
| 成功判据 | `halt` / `pc:<addr>` |
| 超时 | |
| 调试产物 | `.map/.lst/.sym` |
| 可复现 | 双构建 SHA-256 相同 |

## 8. 验收

```text
[ ] S0 契约冻结
[ ] REF 通过 <suite>
[ ] 镜像可复现
[ ] lint/sim 绿
[ ] difftest 零差异
[ ] CI required check
[ ] 检查点 restore == 连续
[ ] FPGA/PDK 门禁（如适用）
```

## 9. 已知限制

- 
- 

## 10. 变更记录

| 日期 | 变更 | ADR | 证据 |
|---|---|---|---|
| | | | |
