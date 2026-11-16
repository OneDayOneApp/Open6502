# 契约：镜像、REF/DUT、事件、检查点、Manifest

> 契约是门禁的地基。契约不清，后面所有“通过”都不可复核。本文给最小字段集，按芯片裁剪，但不要删到无法复现。

## 1. 镜像契约（Image Contract）

### 1.1 必须写清的八件事

| 项 | 说明 | Open6502 实例 |
|---|---|---|
| 容器格式 | 文件头、段、对齐、校验 | 平坦 64 KiB `.bin`；iNES `.nes` |
| 装载地址 | 每个段的物理/虚拟地址 | flat: 0x0000；NES: PRG 映射 `$8000+` |
| 入口 | reset 向量/entry point 由谁写 | 构建写 `$FFFC/$FFFD`，harness 不改 |
| host I/O | putc/exit/输入/帧计数端口 | `$F001-$F003`；`$4018/$4019` |
| 成功判据 | 什么算 PASS | `halt` 退出码 0；或 `pc:0x3469` |
| 超时 | 指令/周期预算 | `MAX_INST`；耗尽 = FAIL |
| 调试产物 | map/lst/sym/trace | `.map`、`.lst`、`.hex`、`.sym` |
| 可复现 | 构建两次字节相同 | 无时间戳、无随机、符号排序 |

### 1.2 五条不变量

```text
I1  每个镜像都是真程序：_halt 可达，不落穿到 BRK/非法指令
I2  平坦镜像大小精确；reset 向量指向镜像内
I3  容器头/段大小/映射字节与 ARCH 一致
I4  构建两次字节相同（SHA-256 相同）
I5  同一镜像原样跑过所有后端（REF/周期模型/RTL/FPGA）
```

`[事实]` am6502 的 I1–I5 就是这五条；其中 I4 是“golden 哈希能成立”的前提。

### 1.3 禁止事项

- harness 改写 reset 向量/入口/段表。
- 每个后端各生成一份“适配版”镜像。
- 用时间戳、随机数、未排序符号污染构建。
- 超时/未 halt 当 PASS。
- 用 shell 变量临时定义成功判据；判据必须属于 app 声明。

## 2. REF/DUT 契约

### 2.1 角色

| 角色 | 职责 | 禁止 |
|---|---|---|
| REF（golden） | 提供功能/状态/时序基准；可快照/回放 | 被综合；与 DUT 共享核心实现还宣称独立 |
| DUT | 待验证 RTL/网表/FPGA | 复制 REF 状态；跳过事务追平 |
| Runner | 加载同一镜像、同输入、比对、报告 | 改镜像；掩盖首个差异 |

### 2.2 独立性要求

- REF 和 DUT 不能是同一份代码的两个编译产物。
- 共享 PPU/APU/总线代码时，一致只能算“非独立证据”；报告要标注。
- REF 必须用外部测试、规范或硬件捕获校准；不能只和另一个仿真器对比。
- 每次运行记录：REF commit/版本、配置、状态版本、镜像哈希、输入脚本、种子。

### 2.3 接口

最小 REF 接口（按需扩展）：

```c
ref_load_image(path, sha256);
ref_reset();
ref_step_instruction();          // 或 step_cycle/step_frame
ref_get_arch_state(&state);      // PC/A/X/Y/SP/P/cycles...
ref_get_bus_trace(&buf);
ref_get_device_state(&state);
ref_state_hash(&hash);
ref_frame_hash(&hash);
ref_snapshot(&buf, &len);
ref_restore(buf, len);
ref_set_input(port, value, tick);
```

## 3. 观测记录（Event Record）

### 3.1 最小字段

```systemverilog
typedef struct packed {
  logic        valid;
  logic [1:0]  kind;    // cpu/bus/device/other
  logic [63:0] tick;    // master tick（整数）
  logic [15:0] addr;
  logic [7:0]  data;
  logic        we;
  logic [7:0]  source;  // 发起者：CPU/DMA/DMC/...
} dbg_event;
```

扩展字段（按芯片）：

- `txn_id`：事务 ID，用于请求/响应配对。
- `instr_id`：关联到同一条指令。
- `parent_event`：因果父事件。
- `phase/delta/sequence`：同一 tick 内的确定顺序。
- `arb_result`：仲裁结果。
- `irq_source`：中断来源。

### 3.2 记录纪律

- tick 用整数；不用浮点 JSON number 表示大整数。
- 同一 tick 的事件必须有稳定全序（phase → sequence）。
- 高频事件用 DPI/共享内存/环形缓冲；不要全量写文本。
- FPGA 只导出摘要/哈希；dot 级留在仿真。
- trace 开关不能改变状态；trace 级别要可配置。

## 4. 同步点（Sync Points）

| 粒度 | 触发 | 比较 | 适用阶段 |
|---|---|---|---|
| 指令 | retire | PC/A/X/Y/SP/P/cycles | S3 起 |
| 总线 | `bus_strobe` | addr/data/we/tick/source | S3 起 |
| 设备寄存器 | 写/读提交 | addr/data/old/new | L1 |
| 扫描线/帧 | scanline_end/frame_end | v/t/x/w/OAM/status/frame hash | 有显示设备 |
| 音频 | sample strobe | sample hash/容差 | 有音频 |
| 外设/mapper | cart_event | bank/IRQ/PRG RAM | 有映射 |

**对齐规则**：

- 只在声明的同步点比较；不要任意周期偏移。
- 需要跨模型对齐时，在 adapter 显式转换并写理由。
- 字段掩码逐项有硬件依据；例如 6502 的 B/U 位可在比较时屏蔽。
- 首个差异立即停；dump 前后 N 条事件。

## 5. 检查点与恢复（Checkpoint）

### 5.1 先做 drained checkpoint

```text
drain：停止发起新事务 → 允许在途完成 → 所有组件冻结在确定时间
```

不要一上来做 in-flight checkpoint；只有显式保存队列、等待方、事务状态后才允许。

### 5.2 必须保存

- 架构状态：PC/寄存器/标志/周期/中断挂起。
- 微架构状态：流水线/状态机/有效地址/待完成事务。
- 设备状态：VRAM/OAM/palette/APU 通道/mapper 寄存器。
- 时间：master tick、相位、frame/dot。
- 调度：全局 sequence、未来事件、队列/等待方。
- 输入：回放位置、控制器状态、种子。
- 存储摘要：RAM/ROM 引用摘要（不是裸指针）。
- 统计窗口：计数器基线。

### 5.3 序列化纪律

- 版本化字段 + 明确端序；不 `fwrite` 整个 struct。
- 不保存指针、padding、函数指针、`FILE*`、SDL 设备号、主机地址。
- 恢复时重新绑定对象 ID/event kind；校验长度、范围、配置摘要。
- 配置不兼容/缺组件/恢复失败必须显式报错，不许静默降级。
- 冷启动/软复位分开；逐 profile 列出保留状态。

### 5.4 验收

```text
save → restore → 继续跑  ==  连续跑
后续规范化事件流、状态、帧、音频一致
```

`[事实]` GEM6502 需求把这条列为 T07；RTL 侧检查点仍是 Open6502 的缺口，新芯片应尽早补。

## 6. 实验 Manifest

每次实验/CI 运行输出一个 manifest，至少：

```json
{
  "schema_version": 1,
  "date": "2026-11-16",
  "build_id": "git-sha or content digest",
  "config": { "path": "...", "sha256": "..." },
  "profile": "chip_profile_v1",
  "precision": { "cpu": "calibrated_timing", "ppu": "approximate_timing" },
  "image": { "path": "...", "sha256": "..." },
  "input": { "path": "...", "sha256": "..." },
  "seed": 0,
  "checkpoint": { "version": 1, "start": "..." },
  "result": "PASS",
  "end_reason": "halt",
  "limits": ["PPU sprite evaluation approximate"]
}
```

规则：

- 没有 manifest 的运行不可复现，不能作为门禁证据。
- 精度标签逐组件公布：`functional` / `approximate_timing` / `calibrated_timing`。
- 未校准组件参与的结论要显示适用限制；不能整机宣称 cycle-exact。
- 确定性统计与主机吞吐统计分开；墙钟 FPS 不是芯片性能。

## 7. 配置契约

- 严格 schema；未知字段、重叠地址、断开的必需端口、非法分频、无效等待协议 → 启动失败。
- 实际配置展开后写入 manifest；不把配置散成全局变量。
- 运行中可改参数要声明生效边界并产生配置事件；其余只在 drain/重启后改。
- 每个 profile 独立验收；一个 profile 过不代表其他过。
- 硬件语义变更必须附规范/波形/边界测试；不能用“实现偶然行为”反推规格。

## 8. 精度声明模板

```text
组件：PPU
标签：approximate_timing
已校准：vblank/NMI、$2002 读、帧哈希
未校准：sprite evaluation、MMC3 A12 过滤、odd-frame skip
证据：ppu_vbl_nmi PASS；frame hash 60/300 帧一致；无 dot 级捕获
限制：涉及 sprite 0 hit 精确 dot 的结论不适用
```

没有这段，就不要在 README/发布说明里写“cycle-accurate”。
