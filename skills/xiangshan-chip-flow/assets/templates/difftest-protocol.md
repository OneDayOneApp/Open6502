# <CHIP> DiffTest 协议

> 复制到 `docs/spec/difftest.md`。协议先冻结，再写 runner；否则“通过”不可复核。

## 1. 目标

在相同镜像、输入、初始状态和配置下，比较 DUT 与 REF 的：

- [ ] 架构状态（指令 retire）
- [ ] 总线事务
- [ ] 设备寄存器/状态
- [ ] 帧/显示
- [ ] 音频
- [ ] 外设/mapper
- [ ] 最终状态哈希

## 2. 角色

| 角色 | 实现 | 版本 | 是否综合 |
|---|---|---|---|
| DUT | | | 是 |
| REF | | | 否 |
| Runner | | | 否 |

独立性声明：<REF 与 DUT 是否共享代码；共享部分标非独立证据>

## 3. 镜像/输入

| 项 | 值 |
|---|---|
| 容器 | |
| 装载地址 | |
| 入口 | 构建写 reset 向量 |
| 输入脚本 | |
| 种子 | |
| 成功判据 | `halt` / `pc:<addr>` |
| 超时 | |

## 4. 观测记录

```systemverilog
typedef struct packed {
  logic        valid;
  logic [1:0]  kind;
  logic [63:0] tick;
  logic [15:0] addr;
  logic [7:0]  data;
  logic        we;
  logic [7:0]  source;
} dbg_event;
```

扩展字段：<txn_id/instr_id/parent_event/phase/sequence/arb_result/irq_source>

## 5. 同步点

| 粒度 | RTL 触发 | REF 调用 | 比较字段 |
|---|---|---|---|
| 指令 | `cpu_retire` | `cpu_step` | PC/A/X/Y/SP/P/cycles |
| 总线 | `bus_strobe` | bus trace | addr/data/we/tick/source |
| 设备 | `<event>` | `<state>` | |
| 帧 | `frame_end` | framebuffer | SHA-256 |
| 音频 | `aud_valid` | samples | hash/容差 |
| 外设 | `<event>` | `<state>` | |

## 6. 模式

```text
cpu-only     先跑 Klaus/hello
frame-only   每帧哈希
scanline     每扫描线状态
audio        每采样
full         bus + device + frame + audio + peripheral
```

## 7. 比较规则

- 首个差异立即停，dump 前后窗口。
- 掩码：<字段/理由/硬件依据>。
- 拒绝空 trace、格式错误、异常退出、截断、长度不等。
- 不做任意周期偏移；adapter 显式转换。
- 超时/预算耗尽 = 失败。

## 8. 首个差异报告

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

## 9. 预算

| 项 | 目标 |
|---|---|
| Verilator + REF | ≥10^6 cycles/s |
| 单次 CI | ≤<N> 分钟 |
| trace 大小 | ≤<N> MB（超出用环形缓冲） |

## 10. 验收

```text
[ ] <N> 指令零差异
[ ] <N> 周期零差异
[ ] 故意注入错误能在预期同步点失败
[ ] 同种子双跑一致
[ ] 失败报告可复现
```
