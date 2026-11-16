# 验证体系：L0–L7、DiffTest 阶梯、CI、发布门禁

## 1. 测试层级（L0–L7）

| 层 | 对象 | 方法 | 通过标准 | 失败时先看 |
|---|---|---|---|---|
| L0 | package/decode/counter/ALU/寄存器 | SV/C 单元测试 | 定向向量全过 | 单元 waveform |
| L1 | CPU/设备/mapper 块 | 定向测试 + 向量 | 协议/边界/复位全过 | 块级 trace |
| L2 | CPU + bus + DMA | Klaus/nestest/总线测试 | 指令语义 + 总线序列 | retire/bus trace |
| L3 | CPU + 显示/设备 | 设备 ROM + frame hash | 帧/扫描线一致 | frame diff |
| L4 | 音频 | apu_test/DMC/sample hash | 采样/IRQ/DMA 一致 | sample trace |
| L5 | mapper/外设 | bank/IRQ/PRG RAM 测试 | 状态/时序一致 | mapper event |
| L6 | 系统 | 开源/自有 workload | 启动/关键帧/输入/音频 | manifest |
| L7 | FPGA/硬件 | 板级 + 逻辑分析仪 | 稳定/时序容差内 | 捕获对齐 |

**规则**：低层失败不许用高层“能跑”掩盖；高层失败先回低层定位。

## 2. 必需测试套件（按芯片替换）

### CPU

- Klaus 6502 functional test（官方指令）。
- nestest（opcode/周期/标志）。
- instr_test-v5（指令覆盖）。
- cpu_timing_test6（周期）。
- cpu_interrupts_v2（NMI/IRQ/BRK/RTI）。
- dummy read / RMW 双写 / 跨页 / 总线冲突测试。
- OAM/DMC DMA stall 测试。

### 显示/PPU（如有）

- ppu_vbl_nmi：vblank/NMI 精确 dot。
- sprite_hit_tests / sprite_overflow_tests：sprite 0/overflow。
- ppu_open_bus：open bus/调色板读。
- palette/scroll/split-screen 测试。
- 240p Test Suite（系统级）。
- frame hash：60/300 帧一致。

### 音频/APU（如有）

- apu_test / apu_reset。
- pulse/triangle/noise 定向。
- frame sequencer 精确 CPU 周期。
- DMC address/length/rate/loop/IRQ/DMA。
- mixer/采样率/容差。

### 外设/mapper（如有）

每个 mapper/外设至少覆盖：reset 状态、bank/寄存器边界、镜像、使能/保护、IRQ latch/reload/ack、save/load 往返。

### 系统 workload

- 6 个开源 ROM 或等价的自有 workload。
- 启动、N 帧无 abort、关键帧哈希、输入改变状态、音频非静音、save/load 续跑。

> `[事实]` Open6502 的必需套件与 6 个开源 ROM 列在 `docs/open6502-chip/verification.md`；发布门禁要求 Klaus、PPU vblank/NMI/sprite 0、APU 四通道、mapper 0/1/2/3/4/7、6 ROM、CPU+帧 DiffTest。

## 3. DiffTest 阶梯（逐层加，不要跳）

```text
D0  最终状态冒烟        PC/A/X/Y/SP/P 一次比对
D1  逐指令 retire       每条指令一行，首差异定位
D2  总线事务            addr/data/we/tick/source；事务守恒
D3  设备协议            $4016/$2006/$2007/NMI 等提交点
D4  扫描线/帧           设备状态 + frame hash
D5  音频                sample hash/容差
D6  外设/mapper         bank/IRQ/PRG RAM 状态
D7  全状态 + 检查点      任意点 restore 后继续一致
```

**每层的退出标准**：

- D1：规定指令数零差异；失败打印首个分歧及上下文。
- D2：请求/响应守恒；无重复提交；dummy/RMW 序列一致。
- D3：设备寄存器读写次数/顺序一致；电平不被重复提交。
- D4：关键帧哈希一致；扫描线状态一致。
- D5：采样哈希或容差内一致；IRQ/DMA 时刻一致。
- D6：bank/IRQ/PRG RAM 状态一致。
- D7：save→restore→继续 == 连续运行。

**性能预算**：Verilator + REF ≥ 10^6 cycles/s；停在第首个差异并 dump 窗口；FPGA 只做帧/哈希级。

## 4. 失败诊断：从 L0 往上

```text
L0 单元：单独跑块，固定输入
L1 协议：看设备寄存器提交序列
L2 总线：每个访问是否恰好一个 strobe
L3 SoC：NMI/关键状态变量/写序列
L4 golden：帧/哈希，字节级 diff
```

**首个差异报告**必须能回答：哪个同步点、哪个 tick/周期/扫描线、哪个字段、DUT/REF 各是什么、最近事件、如何回放。

## 5. CI 门禁

### 5.1 顺序

```text
lint/编译
  → L0/L1
  → 镜像构建 + 双构建哈希
  → 同一镜像多后端
  → retire DiffTest
  → 帧/音频哈希
  → 确定性双跑
  → 失败上传 trace/manifest
```

### 5.2 规则

- PR 不绿不能合；required check 必须真的会红。
- 同种子跑两遍；trace 开/关结果一致。
- 失败自动上传 trace、waveform、manifest、首个差异窗口。
- 外部下载型测试（Klaus 等）可 opt-in，但不能因此让主门禁变空。
- 至少保留一条“无 RTL 工具链也能绿”的快速门禁，保护只改软件的贡献者。
- 禁止 placeholder 目标永远成功；超时必须非零退出。

`[事实]` `am6502.yml` 把平坦镜像、卡带三后端、逐指令 DiffTest 分开跑；Klaus 因需下载而 opt-in，这是有意的取舍。

### 5.3 覆盖率（新芯片建议）

- 行/分支/翻转覆盖率只是底线；要加**功能覆盖率**：指令/寻址/中断/DMA/设备状态。
- 覆盖率下降的 PR 要解释；不许用 exclude 把未测逻辑藏掉。
- 形式验证优先补：总线互斥、FIFO 不溢出、状态机不可达态、请求/响应守恒。

## 6. Golden 治理

| 事件 | 允许？ | 要求 |
|---|---|---|
| golden 哈希变化 | 允许 | 单独 PR/评审；原因、影响、重跑证据 |
| 删除测试 | 禁止 | 除非测试本身错误，且需 ADR |
| 弱化断言 | 禁止 | 同上 |
| 掩码字段 | 有条件 | 逐字段硬件依据；写进协议 |
| 更新 ROM/输入 | 允许 | 记录新哈希、版本、许可证 |
| 跳过不稳定测试 | 有条件 | 标记 flaky、开 issue、限时修复 |

golden 文件旁边放：生成命令、输入哈希、REF 版本、生成日期。

## 7. 发布门禁（v0 示例）

```text
[ ] CPU 套件全过（Klaus/nestest/instr_test-v5/interrupts）
[ ] 显示套件全过（vblank/NMI/sprite 0/overflow/open bus）
[ ] 音频套件全过（四通道/DMC/frame sequencer）
[ ] 外设/mapper 矩阵全过
[ ] 6 个 workload 可跑，关键帧哈希一致
[ ] CPU + 帧 DiffTest 通过
[ ] 确定性双跑一致
[ ] 已知限制文档化
```

ASIC 追加：DFT/scan、STA、DRC/LVS、power、package、硬件对齐。**未清不发布。**

## 8. 新芯片怎么替换测试集

1. 列出目标芯片的“规范条目”清单（指令/寄存器/时序/协议）。
2. 每条规范至少一个测试；无法测的标“未覆盖”，不要假装覆盖。
3. 先做参考模型自测，再做 DUT vs REF。
4. 用外部权威套件（如 ISA 测试、协议一致性）校准 REF。
5. 保留一个“最小失败用例”，每次修 bug 先加回归。
6. 每季度清理 flaky/失效测试；清理也要评审。

## 9. 验证计划模板（可直接抄）

```markdown
# <chip> Verification Plan

## Levels
L0 ... L7 ...

## Required suites
- CPU/ISA:
- Bus/DMA:
- Device A:
- Device B:
- System workloads:

## DiffTest
- sync points:
- modes:
- budget:
- report fields:

## CI
- required checks:
- determinism:
- artifacts:

## Release gate
- [ ] ...
- Known limits:
```
